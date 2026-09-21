#include "EditorShellV3.h"
#include "EditorShellV3Controls.h"
#include "EditorSession.h"
#include "EditorCommands.h"
#include "EditorTransformMath.h"
#include "EditorTheme.h"

#include "Core/Log.h"
#include "Runtime/Engine.h"
#include "Runtime/World.h"
#include "Runtime/Components/TransformComponent.h"
#include "Runtime/Reflection/BuiltinTypes.h"
#include "Resources/Typed/MeshResource.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <new>
#include <sstream>
#include <string>
#include <vector>

#include <CommCtrl.h>
#include <Windowsx.h>

namespace nocturne::editor
{
    using namespace shellv3;

    namespace
    {
        constexpr noc::TypeId kInspectorTransformTypeId{
            noc::kTransformComponentTypeId.value
        };
        constexpr noc::PropertyId kInspectorTranslationPropertyId =
            noc::MakePropertyId(
                "Nocturne.Transform.localTranslation");
        constexpr noc::PropertyId kInspectorRotationPropertyId =
            noc::MakePropertyId(
                "Nocturne.Transform.localRotation");
        constexpr noc::PropertyId kInspectorScalePropertyId =
            noc::MakePropertyId(
                "Nocturne.Transform.localScale");
        
        InspectorEditPresentation InspectorPresentationFor(
            noc::TypeId componentTypeId,
            const InspectorPropertyView& property) noexcept
        {
            if (componentTypeId == kInspectorTransformTypeId
                && property.propertyId == kInspectorRotationPropertyId
                && property.valueTypeId == noc::BuiltinTypeIds::Quat)
            {
                return InspectorEditPresentation::EulerDegreesAxis;
            }
        
            if (property.displayAngleDegrees)
                return InspectorEditPresentation::AngleDegrees;
        
            if (property.valueTypeId == noc::BuiltinTypeIds::Vec2)
                return InspectorEditPresentation::Vector2Axis;
            if (property.valueTypeId == noc::BuiltinTypeIds::Vec3)
                return InspectorEditPresentation::Vector3Axis;
            if (property.valueTypeId == noc::BuiltinTypeIds::Vec4)
                return InspectorEditPresentation::Vector4Axis;
            if (property.valueTypeId == noc::BuiltinTypeIds::AABB)
                return InspectorEditPresentation::AabbMinMaxAxes;
        
            return InspectorEditPresentation::Generic;
        }
        
        uint32_t InspectorEditControlCount(
            noc::TypeId componentTypeId,
            const InspectorPropertyView& property) noexcept
        {
            switch (InspectorPresentationFor(
                componentTypeId,
                property))
            {
            case InspectorEditPresentation::Vector2Axis:
                return 2u;
            case InspectorEditPresentation::Vector3Axis:
            case InspectorEditPresentation::EulerDegreesAxis:
                return 3u;
            case InspectorEditPresentation::Vector4Axis:
                return 4u;
            case InspectorEditPresentation::AabbMinMaxAxes:
                return 6u;
            default:
                return 1u;
            }
        }
        
        uint32_t InspectorPropertyRowCount(
            noc::TypeId componentTypeId,
            const InspectorPropertyView& property) noexcept
        {
            return InspectorPresentationFor(
                       componentTypeId,
                       property)
                    == InspectorEditPresentation::AabbMinMaxAxes
                ? 2u
                : 1u;
        }
        
        noc::PropertyId InspectorVectorAxisPropertyId(
            InspectorEditPresentation presentation,
            uint32_t axis) noexcept
        {
            if (presentation
                == InspectorEditPresentation::Vector2Axis)
            {
                constexpr noc::PropertyId ids[] = {
                    noc::MakePropertyId("Nocturne.Vec2.x"),
                    noc::MakePropertyId("Nocturne.Vec2.y")
                };
                return axis < 2 ? ids[axis] : noc::PropertyId{};
            }
        
            if (presentation
                == InspectorEditPresentation::Vector3Axis)
            {
                constexpr noc::PropertyId ids[] = {
                    noc::MakePropertyId("Nocturne.Vec3.x"),
                    noc::MakePropertyId("Nocturne.Vec3.y"),
                    noc::MakePropertyId("Nocturne.Vec3.z")
                };
                return axis < 3 ? ids[axis] : noc::PropertyId{};
            }
        
            if (presentation
                == InspectorEditPresentation::Vector4Axis)
            {
                constexpr noc::PropertyId ids[] = {
                    noc::MakePropertyId("Nocturne.Vec4.x"),
                    noc::MakePropertyId("Nocturne.Vec4.y"),
                    noc::MakePropertyId("Nocturne.Vec4.z"),
                    noc::MakePropertyId("Nocturne.Vec4.w")
                };
                return axis < 4 ? ids[axis] : noc::PropertyId{};
            }
        
            return {};
        }
        
        const wchar_t* InspectorPropertyPresentationLabel(
            noc::TypeId componentTypeId,
            const InspectorPropertyView& property) noexcept
        {
            if (componentTypeId != kInspectorTransformTypeId)
                return nullptr;
        
            if (property.propertyId == kInspectorTranslationPropertyId)
                return L"Position";
            if (property.propertyId == kInspectorRotationPropertyId)
                return L"Rotation";
            if (property.propertyId == kInspectorScalePropertyId)
                return L"Scale";
        
            return nullptr;
        }
        
        bool ParseFiniteInspectorFloat(
            const char* text,
            float& outValue) noexcept
        {
            if (!text)
                return false;
        
            errno = 0;
            char* end = nullptr;
            const float value =
                std::strtof(text, &end);
        
            if (end == text
                || errno == ERANGE
                || !std::isfinite(value))
            {
                return false;
            }
        
            while (*end == ' '
                || *end == '\t'
                || *end == '\r'
                || *end == '\n')
            {
                ++end;
            }
        
            if (*end != '\0')
                return false;
        
            outValue = value;
            return true;
        }
        
        bool IsResourcePickerProperty(
            const InspectorPropertyView& property) noexcept
        {
            return property.valueTypeId
                    == noc::BuiltinTypeIds::ResourceHandle
                && noc::HasFlag(
                    property.flags,
                    noc::PropertyFlags::ResourceReference)
                && !noc::HasFlag(
                    property.flags,
                    noc::PropertyFlags::ReadOnly);
        }
        
        bool IsBoolToggleProperty(
            const InspectorPropertyView& property) noexcept
        {
            return property.editable
                && property.valueKind == noc::TypeKind::Bool;
        }
        
        bool IsEnumPickerProperty(
            const noc::ReflectionRegistry* registry,
            const InspectorPropertyView& property) noexcept
        {
            if (!registry
                || !property.editable
                || property.valueKind != noc::TypeKind::Enum)
            {
                return false;
            }
        
            const noc::TypeMetadata* valueType =
                registry->FindType(property.valueTypeId);
        
            return valueType
                && valueType->enumMetadata
                && !valueType->enumMetadata->isFlags
                && valueType->enumMetadata->values
                && valueType->enumMetadata->valueCount > 0;
        }
    }

    void EditorShellV3::RefreshInspector()
    {
        const noc::EntityHandle previousEntity =
            inspectorModel_.Entity();

        DestroyInspectorControls_();
        inspectorModel_.Clear();

        if (!engine_ || !session_)
            return;

        const noc::EntityHandle selected =
            session_->SelectedEntity();

        if (selected != previousEntity)
            inspectorScrollY_ = 0;

        if (selected.IsValid())
        {
            auto context =
                session_->CommandContext();

            if (!inspectorModel_.Refresh(
                    context,
                    selected))
            {
                inspectorModel_.Clear();
                AppendConsole_(
                    L"Inspector refresh failed.");
            }
        }

        ClampInspectorScroll_();

        if (!RebuildInspectorControls_())
        {
            AppendConsole_(
                L"Inspector control rebuild failed.");
        }

        if (inspector_.body)
            InvalidateRect(
                inspector_.body,
                nullptr,
                FALSE);
    }


    void EditorShellV3::DestroyInspectorControls_() noexcept
    {
        inspectorControlsRefreshing_ = true;

        for (InspectorEditBinding& binding : inspectorEdits_)
        {
            if (!binding.hwnd)
                continue;

            RemoveWindowSubclass(
                binding.hwnd,
                &EditorShellV3::InspectorEditSubclassProc_,
                0x1620);
            DestroyWindow(binding.hwnd);
            binding.hwnd = nullptr;
        }

        for (InspectorComponentActionBinding& binding :
             inspectorRemoveButtons_)
        {
            if (binding.hwnd)
                DestroyWindow(binding.hwnd);
            binding.hwnd = nullptr;
        }

        for (InspectorResourceBinding& binding :
             inspectorResourceButtons_)
        {
            if (binding.hwnd)
                DestroyWindow(binding.hwnd);
            binding.hwnd = nullptr;
        }

        for (InspectorBoolBinding& binding :
             inspectorBoolButtons_)
        {
            if (binding.hwnd)
                DestroyWindow(binding.hwnd);
            binding.hwnd = nullptr;
        }

        for (InspectorEnumBinding& binding :
             inspectorEnumButtons_)
        {
            if (binding.hwnd)
                DestroyWindow(binding.hwnd);
            binding.hwnd = nullptr;
        }

        inspectorEdits_.clear();
        inspectorRemoveButtons_.clear();
        inspectorResourceButtons_.clear();
        inspectorBoolButtons_.clear();
        inspectorEnumButtons_.clear();
        inspectorControlsRefreshing_ = false;
    }


    bool EditorShellV3::RebuildInspectorControls_()
    {
        DestroyInspectorControls_();

        if (!hwnd_
            || !inspector_.body
            || !inspectorModel_.Entity().IsValid())
        {
            return true;
        }

        size_t editableCount = 0;
        size_t removableCount = 0;
        size_t resourceCount = 0;
        size_t boolCount = 0;
        size_t enumCount = 0;

        const noc::ReflectionRegistry* reflection =
            engine_ ? &engine_->Reflection() : nullptr;

        for (const InspectorComponentView& component :
             inspectorModel_.Components())
        {
            if (component.removable)
                ++removableCount;

            for (const InspectorPropertyView& property :
                 component.properties)
            {
                if (IsResourcePickerProperty(property))
                {
                    ++resourceCount;
                }
                else if (IsBoolToggleProperty(property))
                {
                    ++boolCount;
                }
                else if (IsEnumPickerProperty(
                    reflection,
                    property))
                {
                    ++enumCount;
                }
                else if (property.editable)
                {
                    editableCount +=
                        InspectorEditControlCount(
                            component.typeId,
                            property);
                }
            }
        }

        try
        {
            inspectorEdits_.reserve(editableCount);
            inspectorRemoveButtons_.reserve(removableCount);
            inspectorResourceButtons_.reserve(resourceCount);
            inspectorBoolButtons_.reserve(boolCount);
            inspectorEnumButtons_.reserve(enumCount);
        }
        catch (const std::bad_alloc&)
        {
            return false;
        }

        for (const InspectorComponentView& component :
             inspectorModel_.Components())
        {
            if (component.removable)
            {
                const size_t removeIndex =
                    inspectorRemoveButtons_.size();

                if (removeIndex
                    > static_cast<size_t>(
                        0xFFFF - IdInspectorRemoveBase))
                {
                    DestroyInspectorControls_();
                    return false;
                }

                HWND removeButton = MakeButton(
                    hwnd_,
                    IdInspectorRemoveBase
                        + static_cast<int>(removeIndex),
                    L"Remove",
                    Icon::None,
                    ButtonKind::Neutral,
                    smallFont_);

                if (!removeButton)
                {
                    DestroyInspectorControls_();
                    return false;
                }

                ShowWindow(removeButton, SW_HIDE);

                try
                {
                    inspectorRemoveButtons_.push_back({
                        removeButton,
                        component.typeId
                    });
                }
                catch (const std::bad_alloc&)
                {
                    DestroyWindow(removeButton);
                    DestroyInspectorControls_();
                    return false;
                }
            }

            for (const InspectorPropertyView& property :
                 component.properties)
            {
                if (IsResourcePickerProperty(property))
                {
                    const size_t resourceIndex =
                        inspectorResourceButtons_.size();

                    if (resourceIndex
                        > static_cast<size_t>(
                            0xFFFF - IdInspectorResourceBase))
                    {
                        DestroyInspectorControls_();
                        return false;
                    }

                    const noc::AttributeMetadata* constraint =
                        engine_
                            ? engine_->Reflection().FindPropertyAttribute(
                                component.typeId,
                                property.propertyId,
                                noc::AttributeKind::ResourceTypeConstraint)
                            : nullptr;

                    const noc::TypeId resourceConstraint =
                        constraint
                            && constraint->valueKind
                                == noc::AttributeValueKind::TypeId
                            ? constraint->typeIdValue
                            : noc::TypeId::Invalid();

                    HWND picker = MakeButton(
                        hwnd_,
                        IdInspectorResourceBase
                            + static_cast<int>(resourceIndex),
                        L"<None>",
                        Icon::Mesh,
                        ButtonKind::Neutral,
                        uiFont_);

                    if (!picker)
                    {
                        DestroyInspectorControls_();
                        return false;
                    }

                    ShowWindow(picker, SW_HIDE);

                    try
                    {
                        inspectorResourceButtons_.push_back({
                            picker,
                            component.typeId,
                            property.propertyId,
                            resourceConstraint
                        });
                    }
                    catch (const std::bad_alloc&)
                    {
                        DestroyWindow(picker);
                        DestroyInspectorControls_();
                        return false;
                    }

                    (void)SyncInspectorResourceValue_(
                        inspectorResourceButtons_.back());
                    continue;
                }

                if (IsBoolToggleProperty(property))
                {
                    const size_t boolIndex =
                        inspectorBoolButtons_.size();

                    if (boolIndex
                        > static_cast<size_t>(
                            0xFFFF - IdInspectorBoolBase))
                    {
                        DestroyInspectorControls_();
                        return false;
                    }

                    HWND toggle = MakeButton(
                        hwnd_,
                        IdInspectorBoolBase
                            + static_cast<int>(boolIndex),
                        L"Off",
                        Icon::None,
                        ButtonKind::Tool,
                        uiFont_);

                    if (!toggle)
                    {
                        DestroyInspectorControls_();
                        return false;
                    }

                    ShowWindow(toggle, SW_HIDE);

                    try
                    {
                        inspectorBoolButtons_.push_back({
                            toggle,
                            component.typeId,
                            property.propertyId
                        });
                    }
                    catch (const std::bad_alloc&)
                    {
                        DestroyWindow(toggle);
                        DestroyInspectorControls_();
                        return false;
                    }

                    (void)SyncInspectorBoolValue_(
                        inspectorBoolButtons_.back());
                    continue;
                }

                if (IsEnumPickerProperty(
                        reflection,
                        property))
                {
                    const size_t enumIndex =
                        inspectorEnumButtons_.size();

                    if (enumIndex
                        > static_cast<size_t>(
                            0xFFFF - IdInspectorEnumBase))
                    {
                        DestroyInspectorControls_();
                        return false;
                    }

                    HWND picker = MakeButton(
                        hwnd_,
                        IdInspectorEnumBase
                            + static_cast<int>(enumIndex),
                        L"",
                        Icon::None,
                        ButtonKind::Neutral,
                        uiFont_);

                    if (!picker)
                    {
                        DestroyInspectorControls_();
                        return false;
                    }

                    ShowWindow(picker, SW_HIDE);

                    try
                    {
                        inspectorEnumButtons_.push_back({
                            picker,
                            component.typeId,
                            property.propertyId,
                            property.valueTypeId
                        });
                    }
                    catch (const std::bad_alloc&)
                    {
                        DestroyWindow(picker);
                        DestroyInspectorControls_();
                        return false;
                    }

                    (void)SyncInspectorEnumValue_(
                        inspectorEnumButtons_.back());
                    continue;
                }

                if (!property.editable)
                    continue;

                const InspectorEditPresentation presentation =
                    InspectorPresentationFor(
                        component.typeId,
                        property);

                const uint32_t controlCount =
                    InspectorEditControlCount(
                        component.typeId,
                        property);

                for (uint32_t axis = 0;
                     axis < controlCount;
                     ++axis)
                {
                    const size_t bindingIndex =
                        inspectorEdits_.size();

                    if (bindingIndex
                        > static_cast<size_t>(
                            0xFFFF - IdInspectorEditBase))
                    {
                        DestroyInspectorControls_();
                        return false;
                    }

                    const int controlId =
                        IdInspectorEditBase
                        + static_cast<int>(bindingIndex);

                    const std::wstring displayValue =
                        presentation
                                == InspectorEditPresentation::Generic
                            ? Utf8ToWide_(
                                property.displayValue.c_str())
                            : L"";

                    HWND edit = CreateWindowExW(
                        0,
                        L"EDIT",
                        displayValue.c_str(),
                        WS_CHILD | WS_TABSTOP
                            | WS_BORDER | ES_AUTOHSCROLL,
                        0, 0, 1, 1,
                        hwnd_,
                        reinterpret_cast<HMENU>(
                            static_cast<INT_PTR>(controlId)),
                        GetModuleHandleW(nullptr),
                        nullptr);

                    if (!edit)
                    {
                        DestroyInspectorControls_();
                        return false;
                    }

                    SendMessageW(
                        edit,
                        WM_SETFONT,
                        reinterpret_cast<WPARAM>(uiFont_),
                        TRUE);

                    if (!SetWindowSubclass(
                            edit,
                            &EditorShellV3::InspectorEditSubclassProc_,
                            0x1620,
                            reinterpret_cast<DWORD_PTR>(this)))
                    {
                        DestroyWindow(edit);
                        DestroyInspectorControls_();
                        return false;
                    }

                    try
                    {
                        inspectorEdits_.push_back({
                            edit,
                            component.typeId,
                            property.propertyId,
                            presentation,
                            static_cast<uint8_t>(axis)
                        });

                        InspectorEditBinding& binding =
                            inspectorEdits_.back();

                        const noc::PropertyId axisProperty =
                            InspectorVectorAxisPropertyId(
                                presentation,
                                axis);

                        if (axisProperty.IsValid())
                        {
                            binding.nestedPath[0] =
                                axisProperty;
                            binding.nestedPathCount = 1;
                        }
                        else if (presentation
                            == InspectorEditPresentation::AabbMinMaxAxes)
                        {
                            binding.nestedPath[0] =
                                axis < 3
                                    ? noc::MakePropertyId(
                                        "Nocturne.AABB.min")
                                    : noc::MakePropertyId(
                                        "Nocturne.AABB.max");
                            binding.nestedPath[1] =
                                InspectorVectorAxisPropertyId(
                                    InspectorEditPresentation::Vector3Axis,
                                    axis % 3u);
                            binding.nestedPathCount = 2;
                        }
                    }
                    catch (const std::bad_alloc&)
                    {
                        RemoveWindowSubclass(
                            edit,
                            &EditorShellV3::InspectorEditSubclassProc_,
                            0x1620);
                        DestroyWindow(edit);
                        DestroyInspectorControls_();
                        return false;
                    }
                }
            }
        }

        SyncInspectorControlValues_();
        LayoutInspectorControls_();
        return true;
    }


    void EditorShellV3::LayoutInspectorControls_()
    {
        if (!hwnd_ || !inspector_.body)
            return;

        RECT bodyWindow{};
        RECT bodyClient{};
        GetWindowRect(inspector_.body, &bodyWindow);
        GetClientRect(inspector_.body, &bodyClient);

        POINT bodyPoints[2]{
            { bodyWindow.left, bodyWindow.top },
            { bodyWindow.right, bodyWindow.bottom }
        };
        MapWindowPoints(
            HWND_DESKTOP,
            hwnd_,
            bodyPoints,
            2);

        const int bodyLeft = bodyPoints[0].x;
        const int bodyTop = bodyPoints[0].y;
        const int bodyWidth =
            static_cast<int>(
                bodyClient.right - bodyClient.left);
        const int bodyHeight =
            static_cast<int>(
                bodyClient.bottom - bodyClient.top);

        const int labelWidth =
            (std::max)(95, bodyWidth * 42 / 100);

        const noc::ReflectionRegistry* reflection =
            engine_ ? &engine_->Reflection() : nullptr;

        if (inspectorAddComponent_)
        {
            MoveWindow(
                inspectorAddComponent_,
                bodyLeft + 12,
                bodyTop + (std::max)(0, bodyHeight - 35),
                (std::max)(60, bodyWidth - 24),
                28,
                TRUE);
            const bool showAddComponent =
                inspectorModel_.Entity().IsValid();
            ShowWindow(
                inspectorAddComponent_,
                showAddComponent
                    ? SW_SHOW
                    : SW_HIDE);
            if (showAddComponent)
            {
                RedrawWindow(
                    inspectorAddComponent_,
                    nullptr,
                    nullptr,
                    RDW_INVALIDATE | RDW_UPDATENOW);
            }
        }

        size_t bindingIndex = 0;
        size_t removeIndex = 0;
        size_t resourceIndex = 0;
        size_t boolIndex = 0;
        size_t enumIndex = 0;
        int y = 43 - inspectorScrollY_;

        for (const InspectorComponentView& component :
             inspectorModel_.Components())
        {
            if (component.removable)
            {
                if (removeIndex
                    < inspectorRemoveButtons_.size())
                {
                    HWND button =
                        inspectorRemoveButtons_[removeIndex].hwnd;

                    if (button)
                    {
                        MoveWindow(
                            button,
                            bodyLeft
                                + (std::max)(8, bodyWidth - 70),
                            bodyTop + y + 2,
                            58,
                            22,
                            TRUE);

                        ShowWindow(
                            button,
                            y >= 40
                                && y + 26 <= bodyHeight - 40
                                ? SW_SHOW
                                : SW_HIDE);
                    }
                }

                ++removeIndex;
            }

            y += 31;

            for (const InspectorPropertyView& property :
                 component.properties)
            {
                if (IsResourcePickerProperty(property))
                {
                    if (resourceIndex
                        >= inspectorResourceButtons_.size())
                    {
                        return;
                    }

                    HWND picker =
                        inspectorResourceButtons_[resourceIndex].hwnd;

                    const int x =
                        labelWidth + 4;
                    const int width =
                        (std::max)(
                            32,
                            bodyWidth - x - 12);
                    const bool visible =
                        y >= 40
                        && y + 24 <= bodyHeight - 40;

                    if (picker)
                    {
                        MoveWindow(
                            picker,
                            bodyLeft + x,
                            bodyTop + y + 1,
                            width,
                            22,
                            TRUE);
                        ShowWindow(
                            picker,
                            visible ? SW_SHOW : SW_HIDE);
                    }

                    ++resourceIndex;
                }
                else if (IsBoolToggleProperty(property))
                {
                    if (boolIndex
                        >= inspectorBoolButtons_.size())
                    {
                        return;
                    }

                    HWND toggle =
                        inspectorBoolButtons_[boolIndex].hwnd;

                    const int x =
                        labelWidth + 4;
                    const int width =
                        (std::max)(
                            32,
                            bodyWidth - x - 12);
                    const bool visible =
                        y >= 40
                        && y + 24 <= bodyHeight - 40;

                    if (toggle)
                    {
                        MoveWindow(
                            toggle,
                            bodyLeft + x,
                            bodyTop + y + 1,
                            width,
                            22,
                            TRUE);
                        ShowWindow(
                            toggle,
                            visible ? SW_SHOW : SW_HIDE);
                    }

                    ++boolIndex;
                }
                else if (IsEnumPickerProperty(
                    reflection,
                    property))
                {
                    if (enumIndex
                        >= inspectorEnumButtons_.size())
                    {
                        return;
                    }

                    HWND picker =
                        inspectorEnumButtons_[enumIndex].hwnd;

                    const int x =
                        labelWidth + 4;
                    const int width =
                        (std::max)(
                            32,
                            bodyWidth - x - 12);
                    const bool visible =
                        y >= 40
                        && y + 24 <= bodyHeight - 40;

                    if (picker)
                    {
                        MoveWindow(
                            picker,
                            bodyLeft + x,
                            bodyTop + y + 1,
                            width,
                            22,
                            TRUE);
                        ShowWindow(
                            picker,
                            visible ? SW_SHOW : SW_HIDE);
                    }

                    ++enumIndex;
                }
                else if (property.editable)
                {
                    const InspectorEditPresentation presentation =
                        InspectorPresentationFor(
                            component.typeId,
                            property);

                    const uint32_t controlCount =
                        InspectorEditControlCount(
                            component.typeId,
                            property);

                    if (bindingIndex + controlCount
                        > inspectorEdits_.size())
                    {
                        return;
                    }

                    const int x =
                        labelWidth + 4;
                    const int width =
                        (std::max)(
                            32,
                            bodyWidth - x - 12);

                    const bool visible =
                        y >= 40
                        && y + 24 <= bodyHeight - 40;

                    if (presentation
                        == InspectorEditPresentation::AabbMinMaxAxes)
                    {
                        constexpr int kAxisGap = 4;
                        constexpr int kAxisLabelWidth = 11;
                        const int totalGap = kAxisGap * 2;
                        const int segmentWidth =
                            (std::max)(
                                24,
                                (width - totalGap) / 3);

                        for (uint32_t control = 0;
                             control < 6;
                             ++control)
                        {
                            const uint32_t row =
                                control / 3u;
                            const uint32_t axis =
                                control % 3u;

                            const int rowY =
                                y + static_cast<int>(row) * 27;
                            const bool rowVisible =
                                rowY >= 40
                                && rowY + 24 <= bodyHeight - 40;

                            const int segmentLeft =
                                x
                                + static_cast<int>(axis)
                                    * (segmentWidth + kAxisGap);
                            const int editLeft =
                                segmentLeft + kAxisLabelWidth;
                            const int segmentRight =
                                axis == 2
                                    ? x + width
                                    : segmentLeft + segmentWidth;
                            const int editWidth =
                                (std::max)(
                                    20,
                                    segmentRight - editLeft);

                            HWND edit =
                                inspectorEdits_[
                                    bindingIndex + control].hwnd;

                            if (edit)
                            {
                                MoveWindow(
                                    edit,
                                    bodyLeft + editLeft,
                                    bodyTop + rowY + 1,
                                    editWidth,
                                    22,
                                    TRUE);
                                ShowWindow(
                                    edit,
                                    rowVisible ? SW_SHOW : SW_HIDE);
                            }
                        }
                    }
                    else if (controlCount == 1)
                    {
                        HWND edit =
                            inspectorEdits_[bindingIndex].hwnd;

                        if (edit)
                        {
                            MoveWindow(
                                edit,
                                bodyLeft + x,
                                bodyTop + y + 1,
                                width,
                                22,
                                TRUE);
                            ShowWindow(
                                edit,
                                visible ? SW_SHOW : SW_HIDE);
                        }
                    }
                    else
                    {
                        constexpr int kAxisGap = 4;
                        constexpr int kAxisLabelWidth = 11;
                        const int totalGap =
                            kAxisGap
                            * static_cast<int>(
                                controlCount - 1);
                        const int segmentWidth =
                            (std::max)(
                                24,
                                (width - totalGap)
                                    / static_cast<int>(
                                        controlCount));

                        for (uint32_t axis = 0;
                             axis < controlCount;
                             ++axis)
                        {
                            const int segmentLeft =
                                x
                                + static_cast<int>(axis)
                                    * (segmentWidth + kAxisGap);
                            const int editLeft =
                                segmentLeft + kAxisLabelWidth;
                            const int segmentRight =
                                axis + 1 == controlCount
                                    ? x + width
                                    : segmentLeft + segmentWidth;
                            const int editWidth =
                                (std::max)(
                                    20,
                                    segmentRight - editLeft);

                            HWND edit =
                                inspectorEdits_[
                                    bindingIndex + axis].hwnd;

                            if (edit)
                            {
                                MoveWindow(
                                    edit,
                                    bodyLeft + editLeft,
                                    bodyTop + y + 1,
                                    editWidth,
                                    22,
                                    TRUE);
                                ShowWindow(
                                    edit,
                                    visible ? SW_SHOW : SW_HIDE);
                            }
                        }
                    }

                    bindingIndex += controlCount;
                }

                y += 27 * static_cast<int>(
                    InspectorPropertyRowCount(
                        component.typeId,
                        property));
            }

            y += 7;
        }
    }


    int EditorShellV3::InspectorContentHeight_() const noexcept
    {
        if (!inspectorModel_.Entity().IsValid())
            return 0;

        int y = 43;

        for (const InspectorComponentView& component :
             inspectorModel_.Components())
        {
            y += 31;

            for (const InspectorPropertyView& property :
                 component.properties)
            {
                y += 27 * static_cast<int>(
                    InspectorPropertyRowCount(
                        component.typeId,
                        property));
            }

            y += 7;
        }

        return y + 6;
    }


    void EditorShellV3::ClampInspectorScroll_() noexcept
    {
        if (!inspector_.body)
        {
            inspectorScrollY_ = 0;
            return;
        }

        RECT rc{};
        GetClientRect(inspector_.body, &rc);

        const int viewportBottom =
            (std::max)(43, static_cast<int>(rc.bottom) - 40);
        const int maxScroll =
            (std::max)(
                0,
                InspectorContentHeight_() - viewportBottom);

        inspectorScrollY_ =
            (std::clamp)(
                inspectorScrollY_,
                0,
                maxScroll);
    }


    EditorShellV3::InspectorEditBinding*

    EditorShellV3::FindInspectorEdit_(HWND source) noexcept
    {
        for (InspectorEditBinding& binding : inspectorEdits_)
        {
            if (binding.hwnd == source)
                return &binding;
        }

        return nullptr;
    }


    const EditorShellV3::InspectorEditBinding*

    EditorShellV3::FindInspectorEdit_(HWND source) const noexcept
    {
        for (const InspectorEditBinding& binding : inspectorEdits_)
        {
            if (binding.hwnd == source)
                return &binding;
        }

        return nullptr;
    }


    EditorShellV3::InspectorComponentActionBinding*

    EditorShellV3::FindInspectorRemoveButton_(HWND source) noexcept
    {
        for (InspectorComponentActionBinding& binding :
             inspectorRemoveButtons_)
        {
            if (binding.hwnd == source)
                return &binding;
        }

        return nullptr;
    }


    EditorShellV3::InspectorResourceBinding*

    EditorShellV3::FindInspectorResourceButton_(
        HWND source) noexcept
    {
        for (InspectorResourceBinding& binding :
             inspectorResourceButtons_)
        {
            if (binding.hwnd == source)
                return &binding;
        }

        return nullptr;
    }


    bool EditorShellV3::SyncInspectorResourceValue_(
        const InspectorResourceBinding& binding)
    {
        if (!binding.hwnd
            || !session_
            || !inspectorModel_.Entity().IsValid())
        {
            return false;
        }

        auto context =
            session_->CommandContext();

        noc::OwnedReflectedValue value;
        if (!inspectorModel_.ReadValue(
                context,
                inspectorModel_.Entity(),
                binding.componentTypeId,
                binding.propertyId,
                value)
            || value.Type()
                != noc::BuiltinTypeIds::ResourceHandle
            || !value.Data())
        {
            return false;
        }

        const noc::ResourceHandle handle =
            *static_cast<const noc::ResourceHandle*>(
                value.Data());

        wchar_t label[96]{};
        if (!handle.IsValid())
        {
            wcscpy_s(label, L"<None>");
        }
        else
        {
            swprintf_s(
                label,
                L"Mesh %u:%u",
                handle.index,
                handle.generation);
        }

        SetWindowTextW(
            binding.hwnd,
            label);
        return true;
    }


    void EditorShellV3::ShowResourcePicker_(
        InspectorResourceBinding& binding)
    {
        if (!engine_
            || !session_
            || !binding.hwnd
            || !inspectorModel_.Entity().IsValid())
        {
            return;
        }

        if (binding.resourceConstraint
            != noc::BuiltinTypeIds::MeshResource)
        {
            AppendConsole_(
                L"Resource picker: reflected resource type is not supported by the Phase 16 picker.");
            return;
        }

        struct MeshCandidate
        {
            std::wstring label;
            std::string runtimeVPath;
        };

        std::vector<MeshCandidate> candidates;
        namespace fs = std::filesystem;
        std::error_code ec;
        const fs::path contentRoot(contentRoot_);

        try
        {
            if (fs::exists(contentRoot, ec))
            {
                for (fs::recursive_directory_iterator it(
                         contentRoot,
                         fs::directory_options::skip_permission_denied,
                         ec),
                     end;
                     it != end;
                     it.increment(ec))
                {
                    if (ec)
                    {
                        ec.clear();
                        continue;
                    }

                    if (!it->is_regular_file(ec))
                        continue;

                    fs::path extension =
                        it->path().extension();
                    std::wstring ext =
                        extension.wstring();
                    std::transform(
                        ext.begin(),
                        ext.end(),
                        ext.begin(),
                        ::towlower);

                    if (ext != L".obj"
                        && ext != L".nmsh")
                    {
                        continue;
                    }

                    fs::path relative =
                        fs::relative(
                            it->path(),
                            contentRoot,
                            ec);
                    if (ec)
                    {
                        ec.clear();
                        continue;
                    }

                    std::wstring relativeWide =
                        relative.generic_wstring();

                    std::string relativeUtf8;
                    if (!WideToUtf8_(
                            relativeWide.c_str(),
                            relativeUtf8))
                    {
                        continue;
                    }

                    std::string runtimeVPath =
                        relativeUtf8;

                    if (ext == L".obj")
                    {
                        runtimeVPath += ".nmsh";

                        const fs::path artifact =
                            fs::path(L"DerivedDataCache")
                            / fs::path(
                                relativeWide + L".nmsh");

                        if (!fs::exists(artifact, ec))
                        {
                            ec.clear();
                            continue;
                        }
                    }

                    candidates.push_back({
                        relativeWide,
                        std::move(runtimeVPath)
                    });
                }
            }

            std::sort(
                candidates.begin(),
                candidates.end(),
                [](const MeshCandidate& a,
                   const MeshCandidate& b)
                {
                    return a.label < b.label;
                });
        }
        catch (const std::bad_alloc&)
        {
            AppendConsole_(
                L"Resource picker failed: allocation failure.");
            return;
        }

        HMENU menu = CreatePopupMenu();
        if (!menu)
            return;

        AppendMenuW(
            menu,
            MF_STRING,
            1,
            L"<None>");

        if (candidates.empty())
        {
            AppendMenuW(
                menu,
                MF_STRING | MF_GRAYED,
                0,
                L"No imported mesh assets");
        }
        else
        {
            for (size_t i = 0;
                 i < candidates.size();
                 ++i)
            {
                if (i
                    > static_cast<size_t>(
                        0xFFFF - 2))
                {
                    break;
                }

                AppendMenuW(
                    menu,
                    MF_STRING,
                    static_cast<UINT_PTR>(i + 2),
                    candidates[i].label.c_str());
            }
        }

        RECT buttonRect{};
        GetWindowRect(
            binding.hwnd,
            &buttonRect);

        const int command =
            TrackPopupMenuEx(
                menu,
                TPM_RETURNCMD
                    | TPM_LEFTALIGN
                    | TPM_TOPALIGN,
                buttonRect.left,
                buttonRect.bottom + 2,
                hwnd_,
                nullptr);

        DestroyMenu(menu);

        if (command <= 0)
            return;

        noc::ResourceHandle newHandle{};
        noc::AABB newBounds{
            noc::Vec3::Zero(),
            noc::Vec3::Zero()
        };

        if (command > 1)
        {
            const size_t candidateIndex =
                static_cast<size_t>(command - 2);

            if (candidateIndex >= candidates.size())
                return;

            auto typedHandle =
                engine_->Resources().RequestMesh(
                    candidates[candidateIndex]
                        .runtimeVPath.c_str());

            if (!typedHandle.IsValid())
            {
                AppendConsole_(
                    L"Mesh assignment failed: ResourceManager rejected the runtime vpath.");
                return;
            }

            // Design choice (not directly from the book): Phase 16 validates
            // a picker selection before authoring it so a failed decode never
            // replaces the previously assigned mesh. A bounded wait avoids an
            // unbounded editor-main-thread stall; async picker UX is deferred.
            constexpr uint32_t kEditorMeshValidationTimeoutMs =
                2000;

            if (!engine_->Resources().WaitUntilReady(
                    typedHandle.Untyped(),
                    kEditorMeshValidationTimeoutMs))
            {
                const char* error =
                    engine_->Resources().GetError(
                        typedHandle.Untyped());

                std::wstring message =
                    L"Mesh assignment failed; previous mesh retained";
                if (error && error[0] != '\0')
                {
                    message += L": ";
                    message += Utf8ToWide_(error);
                }
                message += L".";
                AppendConsole_(message.c_str());
                return;
            }

            const noc::MeshResource* meshResource =
                engine_->Resources().GetMesh(typedHandle);
            if (!meshResource)
            {
                AppendConsole_(
                    L"Mesh assignment failed; decoded mesh is unavailable.");
                return;
            }

            const noc::IntermediateMesh& cpuMesh =
                meshResource->CpuMesh();
            const size_t vertexCount =
                cpuMesh.positions.size() / 3u;
            if (vertexCount == 0
                || cpuMesh.positions.size()
                    != vertexCount * 3u)
            {
                AppendConsole_(
                    L"Mesh assignment failed; decoded mesh has no valid positions.");
                return;
            }

            noc::Vec3 minimum{
                cpuMesh.positions[0],
                cpuMesh.positions[1],
                cpuMesh.positions[2]
            };
            noc::Vec3 maximum = minimum;

            for (size_t vertex = 1;
                 vertex < vertexCount;
                 ++vertex)
            {
                const noc::Vec3 position{
                    cpuMesh.positions[vertex * 3u + 0u],
                    cpuMesh.positions[vertex * 3u + 1u],
                    cpuMesh.positions[vertex * 3u + 2u]
                };

                if (!std::isfinite(position.x)
                    || !std::isfinite(position.y)
                    || !std::isfinite(position.z))
                {
                    AppendConsole_(
                        L"Mesh assignment failed; decoded mesh contains non-finite positions.");
                    return;
                }

                minimum.x = (std::min)(minimum.x, position.x);
                minimum.y = (std::min)(minimum.y, position.y);
                minimum.z = (std::min)(minimum.z, position.z);
                maximum.x = (std::max)(maximum.x, position.x);
                maximum.y = (std::max)(maximum.y, position.y);
                maximum.z = (std::max)(maximum.z, position.z);
            }

            if (!std::isfinite(minimum.x)
                || !std::isfinite(minimum.y)
                || !std::isfinite(minimum.z))
            {
                AppendConsole_(
                    L"Mesh assignment failed; decoded mesh contains non-finite positions.");
                return;
            }

            newHandle = typedHandle.Untyped();
            newBounds = noc::AABB{
                minimum,
                maximum
            };
        }

        const noc::EntityHandle entity =
            inspectorModel_.Entity();

        auto context =
            session_->CommandContext();

        try
        {
            auto meshCommand =
                std::make_unique<
                    SetReflectedPropertyCommand>();
            auto boundsCommand =
                std::make_unique<
                    SetReflectedPropertyCommand>();
            auto compound =
                std::make_unique<
                    CompoundEditorCommand>();

            const noc::TypeId renderableType{
                static_cast<uint64_t>(
                    noc::kRenderableComponentTypeId.value)
            };

            if (binding.componentTypeId != renderableType
                || !meshCommand->Init(
                    context,
                    entity,
                    binding.componentTypeId,
                    binding.propertyId,
                    noc::ReflectedConstValueView{
                        noc::BuiltinTypeIds::ResourceHandle,
                        &newHandle })
                || !boundsCommand->Init(
                    context,
                    entity,
                    renderableType,
                    noc::MakePropertyId(
                        "Nocturne.Renderable.localBounds"),
                    noc::ReflectedConstValueView{
                        noc::BuiltinTypeIds::AABB,
                        &newBounds })
                || !compound->Init(
                    "Assign Mesh + Bounds")
                || !compound->Append(
                    std::move(meshCommand))
                || !compound->Append(
                    std::move(boundsCommand))
                || !session_->History().Execute(
                    context,
                    std::move(compound)))
            {
                AppendConsole_(
                    L"Mesh assignment rejected; mesh and bounds were not changed.");
                return;
            }
        }
        catch (const std::bad_alloc&)
        {
            AppendConsole_(
                L"Mesh assignment failed: allocation failure.");
            return;
        }

        session_->NotifyAuthoredMutation();

        if (!inspectorModel_.Refresh(
                context,
                entity))
        {
            AppendConsole_(
                L"Inspector refresh after mesh assignment failed.");
            return;
        }

        (void)SyncInspectorResourceValue_(
            binding);

        if (inspector_.body)
        {
            InvalidateRect(
                inspector_.body,
                nullptr,
                FALSE);
        }

        UpdateStatus_();
        AppendConsole_(
            newHandle.IsValid()
                ? L"Mesh assigned through reflected ResourceHandle property."
                : L"Mesh assignment cleared.");
    }


    EditorShellV3::InspectorBoolBinding*

    EditorShellV3::FindInspectorBoolButton_(
        HWND source) noexcept
    {
        for (InspectorBoolBinding& binding :
             inspectorBoolButtons_)
        {
            if (binding.hwnd == source)
                return &binding;
        }

        return nullptr;
    }


    bool EditorShellV3::SyncInspectorBoolValue_(
        const InspectorBoolBinding& binding)
    {
        if (!binding.hwnd
            || !session_
            || !inspectorModel_.Entity().IsValid())
        {
            return false;
        }

        auto context =
            session_->CommandContext();

        noc::OwnedReflectedValue value;
        if (!inspectorModel_.ReadValue(
                context,
                inspectorModel_.Entity(),
                binding.componentTypeId,
                binding.propertyId,
                value)
            || value.Type() != noc::BuiltinTypeIds::Bool
            || !value.Data())
        {
            return false;
        }

        const bool enabled =
            *static_cast<const bool*>(value.Data());

        SetWindowTextW(
            binding.hwnd,
            enabled ? L"On" : L"Off");
        ButtonActive(
            binding.hwnd,
            enabled);
        return true;
    }


    void EditorShellV3::ToggleInspectorBool_(
        InspectorBoolBinding& binding)
    {
        if (!session_
            || !inspectorModel_.Entity().IsValid())
        {
            return;
        }

        const noc::EntityHandle entity =
            inspectorModel_.Entity();

        auto context =
            session_->CommandContext();

        noc::OwnedReflectedValue value;
        if (!inspectorModel_.ReadValue(
                context,
                entity,
                binding.componentTypeId,
                binding.propertyId,
                value)
            || value.Type() != noc::BuiltinTypeIds::Bool
            || !value.Data())
        {
            AppendConsole_(
                L"Boolean Inspector toggle could not read the reflected value.");
            return;
        }

        const bool current =
            *static_cast<const bool*>(value.Data());

        if (!inspectorModel_.CommitTextEdit(
                context,
                session_->History(),
                entity,
                binding.componentTypeId,
                binding.propertyId,
                current ? "false" : "true"))
        {
            AppendConsole_(
                L"Boolean Inspector toggle was rejected by the reflected semantic setter.");
            return;
        }

        session_->NotifyAuthoredMutation();

        if (!inspectorModel_.Refresh(
                context,
                entity))
        {
            AppendConsole_(
                L"Inspector refresh after boolean edit failed.");
            return;
        }

        SyncInspectorControlValues_();

        if (inspector_.body)
            InvalidateRect(inspector_.body, nullptr, FALSE);

        UpdateStatus_();
    }


    EditorShellV3::InspectorEnumBinding*

    EditorShellV3::FindInspectorEnumButton_(
        HWND source) noexcept
    {
        for (InspectorEnumBinding& binding :
             inspectorEnumButtons_)
        {
            if (binding.hwnd == source)
                return &binding;
        }

        return nullptr;
    }


    bool EditorShellV3::SyncInspectorEnumValue_(
        const InspectorEnumBinding& binding)
    {
        if (!binding.hwnd)
            return false;

        const InspectorPropertyView* property =
            inspectorModel_.FindProperty(
                binding.componentTypeId,
                binding.propertyId);
        if (!property)
            return false;

        const std::wstring value =
            Utf8ToWide_(
                property->displayValue.c_str());

        SetWindowTextW(
            binding.hwnd,
            value.c_str());
        return true;
    }


    void EditorShellV3::ShowInspectorEnumPopup_(
        InspectorEnumBinding& binding)
    {
        if (!engine_
            || !session_
            || !binding.hwnd
            || !inspectorModel_.Entity().IsValid())
        {
            return;
        }

        const noc::TypeMetadata* valueType =
            engine_->Reflection().FindType(
                binding.valueTypeId);

        if (!valueType
            || valueType->kind != noc::TypeKind::Enum
            || !valueType->enumMetadata
            || valueType->enumMetadata->isFlags
            || !valueType->enumMetadata->values
            || valueType->enumMetadata->valueCount == 0)
        {
            AppendConsole_(
                L"Enum Inspector picker is unavailable for this reflected enum.");
            return;
        }

        const InspectorPropertyView* property =
            inspectorModel_.FindProperty(
                binding.componentTypeId,
                binding.propertyId);
        if (!property)
            return;

        HMENU menu = CreatePopupMenu();
        if (!menu)
            return;

        const noc::EnumMetadata& enumMetadata =
            *valueType->enumMetadata;

        const uint32_t visibleCount =
            (std::min)(
                enumMetadata.valueCount,
                static_cast<uint32_t>(0xFFFE));

        for (uint32_t i = 0;
             i < visibleCount;
             ++i)
        {
            const noc::EnumValueMetadata& value =
                enumMetadata.values[i];

            const std::wstring label =
                Utf8ToWide_(value.canonicalName);

            UINT flags = MF_STRING;
            if (property->displayValue
                == value.canonicalName)
            {
                flags |= MF_CHECKED;
            }

            AppendMenuW(
                menu,
                flags,
                static_cast<UINT_PTR>(i + 1),
                label.c_str());
        }

        RECT buttonRect{};
        GetWindowRect(
            binding.hwnd,
            &buttonRect);

        const int command =
            TrackPopupMenuEx(
                menu,
                TPM_RETURNCMD
                    | TPM_LEFTALIGN
                    | TPM_TOPALIGN,
                buttonRect.left,
                buttonRect.bottom + 2,
                hwnd_,
                nullptr);

        DestroyMenu(menu);

        if (command <= 0)
            return;

        const uint32_t valueIndex =
            static_cast<uint32_t>(command - 1);
        if (valueIndex >= visibleCount)
            return;

        const char* canonicalName =
            enumMetadata.values[
                valueIndex].canonicalName;
        if (!canonicalName)
            return;

        const noc::EntityHandle entity =
            inspectorModel_.Entity();
        auto context =
            session_->CommandContext();

        if (!inspectorModel_.CommitTextEdit(
                context,
                session_->History(),
                entity,
                binding.componentTypeId,
                binding.propertyId,
                canonicalName))
        {
            AppendConsole_(
                L"Enum Inspector selection was rejected by the reflected semantic setter.");
            return;
        }

        session_->NotifyAuthoredMutation();

        if (!inspectorModel_.Refresh(
                context,
                entity))
        {
            AppendConsole_(
                L"Inspector refresh after enum edit failed.");
            return;
        }

        SyncInspectorControlValues_();

        if (inspector_.body)
            InvalidateRect(inspector_.body, nullptr, FALSE);

        UpdateStatus_();
    }


    bool EditorShellV3::SyncInspectorBindingValue_(
        const InspectorEditBinding& binding)
    {
        if (!binding.hwnd)
            return false;

        if (binding.nestedPathCount > 0)
        {
            if (!session_
                || !inspectorModel_.Entity().IsValid())
            {
                return false;
            }

            auto context =
                session_->CommandContext();

            noc::OwnedReflectedValue value;
            if (!inspectorModel_.ReadNestedValue(
                    context,
                    inspectorModel_.Entity(),
                    binding.componentTypeId,
                    binding.propertyId,
                    binding.nestedPath.data(),
                    binding.nestedPathCount,
                    value)
                || !value.Data())
            {
                return false;
            }

            double numericValue = 0.0;
            if (value.Type() == noc::BuiltinTypeIds::Float32)
            {
                numericValue =
                    static_cast<double>(
                        *static_cast<const float*>(
                            value.Data()));
            }
            else if (value.Type() == noc::BuiltinTypeIds::Float64)
            {
                numericValue =
                    *static_cast<const double*>(
                        value.Data());
            }
            else
            {
                return false;
            }

            wchar_t buffer[64]{};
            swprintf_s(
                buffer,
                L"%.6g",
                numericValue);
            SetWindowTextW(
                binding.hwnd,
                buffer);
            return true;
        }

        if (binding.presentation
            == InspectorEditPresentation::Generic)
        {
            const InspectorPropertyView* property =
                inspectorModel_.FindProperty(
                    binding.componentTypeId,
                    binding.propertyId);
            if (!property)
                return false;

            const std::wstring value =
                Utf8ToWide_(
                    property->displayValue.c_str());

            SetWindowTextW(
                binding.hwnd,
                value.c_str());
            return true;
        }

        if (!session_
            || !inspectorModel_.Entity().IsValid())
        {
            return false;
        }

        auto context =
            session_->CommandContext();

        noc::OwnedReflectedValue value;
        if (!inspectorModel_.ReadValue(
                context,
                inspectorModel_.Entity(),
                binding.componentTypeId,
                binding.propertyId,
                value))
        {
            return false;
        }

        float componentValue = 0.0f;

        if (binding.presentation
            == InspectorEditPresentation::AngleDegrees)
        {
            constexpr double kRadiansToDegrees =
                57.29577951308232;

            if (value.Type() == noc::BuiltinTypeIds::Float32
                && value.Data())
            {
                componentValue =
                    static_cast<float>(
                        static_cast<double>(
                            *static_cast<const float*>(
                                value.Data()))
                        * kRadiansToDegrees);
            }
            else if (value.Type() == noc::BuiltinTypeIds::Float64
                && value.Data())
            {
                componentValue =
                    static_cast<float>(
                        *static_cast<const double*>(
                            value.Data())
                        * kRadiansToDegrees);
            }
            else
            {
                return false;
            }
        }
        else
        {
            if (binding.axis > 2)
                return false;

            if (binding.presentation
                == InspectorEditPresentation::Vector3Axis)
            {
                if (value.Type() != noc::BuiltinTypeIds::Vec3
                    || !value.Data())
                {
                    return false;
                }

                const noc::Vec3& vector =
                    *static_cast<const noc::Vec3*>(
                        value.Data());

                componentValue =
                    binding.axis == 0
                        ? vector.x
                        : (binding.axis == 1
                            ? vector.y
                            : vector.z);
            }
            else
            {
                if (value.Type() != noc::BuiltinTypeIds::Quat
                    || !value.Data())
                {
                    return false;
                }

                const noc::Vec3 euler =
                    EditorEulerXYZDegreesFromQuat(
                        *static_cast<const noc::Quat*>(
                            value.Data()));

                componentValue =
                    binding.axis == 0
                        ? euler.x
                        : (binding.axis == 1
                            ? euler.y
                            : euler.z);
            }
        }

        wchar_t buffer[64]{};
        swprintf_s(
            buffer,
            L"%.6g",
            static_cast<double>(componentValue));

        SetWindowTextW(
            binding.hwnd,
            buffer);
        return true;
    }


    void EditorShellV3::SyncInspectorControlValues_()
    {
        inspectorControlsRefreshing_ = true;

        for (const InspectorEditBinding& binding :
             inspectorEdits_)
        {
            (void)SyncInspectorBindingValue_(binding);
        }

        for (const InspectorResourceBinding& binding :
             inspectorResourceButtons_)
        {
            (void)SyncInspectorResourceValue_(binding);
        }

        for (const InspectorBoolBinding& binding :
             inspectorBoolButtons_)
        {
            (void)SyncInspectorBoolValue_(binding);
        }

        for (const InspectorEnumBinding& binding :
             inspectorEnumButtons_)
        {
            (void)SyncInspectorEnumValue_(binding);
        }

        inspectorControlsRefreshing_ = false;
    }


    bool EditorShellV3::CommitInspectorEdit_(HWND source)
    {
        InspectorEditBinding* binding =
            FindInspectorEdit_(source);
        if (!binding
            || !session_
            || !engine_
            || !inspectorModel_.Entity().IsValid())
        {
            return false;
        }

        const noc::EntityHandle entity =
            inspectorModel_.Entity();
        const noc::TypeId componentTypeId =
            binding->componentTypeId;
        const noc::PropertyId propertyId =
            binding->propertyId;

        const int length =
            GetWindowTextLengthW(source);

        std::wstring wide;
        try
        {
            const size_t count =
                static_cast<size_t>(
                    (std::max)(0, length));

            wide.resize(count + 1u);
            GetWindowTextW(
                source,
                wide.data(),
                static_cast<int>(count + 1u));
            wide.resize(count);
        }
        catch (const std::bad_alloc&)
        {
            AppendConsole_(
                L"Inspector edit failed: allocation failure.");
            return false;
        }

        std::string utf8;
        if (!WideToUtf8_(
                wide.c_str(),
                utf8))
        {
            AppendConsole_(
                L"Inspector edit rejected: invalid Unicode input.");
            CancelInspectorEdit_(source);
            return false;
        }

        auto context =
            session_->CommandContext();

        bool committed = false;

        if (binding->nestedPathCount > 0)
        {
            committed =
                inspectorModel_.CommitNestedTextEdit(
                    context,
                    session_->History(),
                    entity,
                    componentTypeId,
                    propertyId,
                    binding->nestedPath.data(),
                    binding->nestedPathCount,
                    utf8.c_str());
        }
        else if (binding->presentation
            == InspectorEditPresentation::Generic)
        {
            committed =
                inspectorModel_.CommitTextEdit(
                    context,
                    session_->History(),
                    entity,
                    componentTypeId,
                    propertyId,
                    utf8.c_str());
        }
        else if (binding->presentation
            == InspectorEditPresentation::AngleDegrees)
        {
            float degrees = 0.0f;
            if (ParseFiniteInspectorFloat(
                    utf8.c_str(),
                    degrees))
            {
                constexpr double kDegreesToRadians =
                    0.017453292519943295;

                const double radians =
                    static_cast<double>(degrees)
                    * kDegreesToRadians;

                char radiansText[96]{};
                std::snprintf(
                    radiansText,
                    sizeof(radiansText),
                    "%.17g",
                    radians);

                committed =
                    inspectorModel_.CommitTextEdit(
                        context,
                        session_->History(),
                        entity,
                        componentTypeId,
                        propertyId,
                        radiansText);
            }
        }
        else
        {
            float parsed = 0.0f;
            if (!ParseFiniteInspectorFloat(
                    utf8.c_str(),
                    parsed)
                || binding->axis > 2)
            {
                committed = false;
            }
            else
            {
                noc::OwnedReflectedValue reflectedValue;

                if (inspectorModel_.ReadValue(
                        context,
                        entity,
                        componentTypeId,
                        propertyId,
                        reflectedValue))
                {
                    char composite[192]{};

                    if (binding->presentation
                        == InspectorEditPresentation::Vector3Axis
                        && reflectedValue.Type()
                            == noc::BuiltinTypeIds::Vec3
                        && reflectedValue.Data())
                    {
                        noc::Vec3 value =
                            *static_cast<const noc::Vec3*>(
                                reflectedValue.Data());

                        if (binding->axis == 0)
                            value.x = parsed;
                        else if (binding->axis == 1)
                            value.y = parsed;
                        else
                            value.z = parsed;

                        std::snprintf(
                            composite,
                            sizeof(composite),
                            "%.9g, %.9g, %.9g",
                            static_cast<double>(value.x),
                            static_cast<double>(value.y),
                            static_cast<double>(value.z));

                        committed =
                            inspectorModel_.CommitTextEdit(
                                context,
                                session_->History(),
                                entity,
                                componentTypeId,
                                propertyId,
                                composite);
                    }
                    else if (binding->presentation
                        == InspectorEditPresentation::EulerDegreesAxis
                        && reflectedValue.Type()
                            == noc::BuiltinTypeIds::Quat
                        && reflectedValue.Data())
                    {
                        noc::Vec3 euler =
                            EditorEulerXYZDegreesFromQuat(
                                *static_cast<const noc::Quat*>(
                                    reflectedValue.Data()));

                        if (binding->axis == 0)
                            euler.x = parsed;
                        else if (binding->axis == 1)
                            euler.y = parsed;
                        else
                            euler.z = parsed;

                        const noc::Quat value =
                            EditorQuatFromEulerXYZDegrees(
                                euler);

                        std::snprintf(
                            composite,
                            sizeof(composite),
                            "%.9g, %.9g, %.9g, %.9g",
                            static_cast<double>(value.x),
                            static_cast<double>(value.y),
                            static_cast<double>(value.z),
                            static_cast<double>(value.w));

                        committed =
                            inspectorModel_.CommitTextEdit(
                                context,
                                session_->History(),
                                entity,
                                componentTypeId,
                                propertyId,
                                composite);
                    }
                }
            }
        }

        if (!committed)
        {
            AppendConsole_(
                L"Inspector edit rejected by reflected semantic setter.");
            CancelInspectorEdit_(source);
            return false;
        }

        session_->NotifyAuthoredMutation();

        if (!inspectorModel_.Refresh(
                context,
                entity))
        {
            AppendConsole_(
                L"Inspector refresh after edit failed.");
            return false;
        }

        PopulateScene_();
        SyncInspectorControlValues_();

        if (inspector_.body)
            InvalidateRect(
                inspector_.body,
                nullptr,
                FALSE);

        UpdateStatus_();
        return true;
    }


    void EditorShellV3::CancelInspectorEdit_(HWND source)
    {
        const InspectorEditBinding* binding =
            FindInspectorEdit_(source);
        if (!binding)
            return;

        (void)SyncInspectorBindingValue_(
            *binding);

        SendMessageW(
            source,
            EM_SETSEL,
            0,
            -1);
    }


    LRESULT CALLBACK EditorShellV3::InspectorBodySubclassProc_(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        UINT_PTR subclassId,
        DWORD_PTR refData)
    {
        (void)subclassId;

        auto* self =
            reinterpret_cast<EditorShellV3*>(refData);
        if (!self)
            return DefSubclassProc(
                hwnd,
                message,
                wParam,
                lParam);

        switch (message)
        {
        case WM_MOUSEWHEEL:
        {
            const int notches =
                GET_WHEEL_DELTA_WPARAM(wParam)
                / WHEEL_DELTA;

            self->inspectorScrollY_ -=
                notches * 81;
            self->ClampInspectorScroll_();
            self->LayoutInspectorControls_();
            InvalidateRect(
                hwnd,
                nullptr,
                FALSE);
            return 0;
        }

        case WM_SIZE:
            self->ClampInspectorScroll_();
            self->LayoutInspectorControls_();
            InvalidateRect(
                hwnd,
                nullptr,
                FALSE);
            break;
        }

        return DefSubclassProc(
            hwnd,
            message,
            wParam,
            lParam);
    }


    LRESULT CALLBACK EditorShellV3::InspectorEditSubclassProc_(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        UINT_PTR subclassId,
        DWORD_PTR refData)
    {
        (void)subclassId;

        auto* self =
            reinterpret_cast<EditorShellV3*>(refData);
        if (!self)
        {
            return DefSubclassProc(
                hwnd,
                message,
                wParam,
                lParam);
        }

        switch (message)
        {
        case WM_KEYDOWN:
            if (wParam == VK_RETURN)
            {
                (void)self->CommitInspectorEdit_(hwnd);
                return 0;
            }

            if (wParam == VK_ESCAPE)
            {
                self->inspectorControlsRefreshing_ = true;
                self->CancelInspectorEdit_(hwnd);
                if (self->sceneTree_)
                    SetFocus(self->sceneTree_);
                self->inspectorControlsRefreshing_ = false;
                return 0;
            }
            break;

        case WM_KILLFOCUS:
            if (!self->inspectorControlsRefreshing_)
                (void)self->CommitInspectorEdit_(hwnd);
            break;
        }

        return DefSubclassProc(
            hwnd,
            message,
            wParam,
            lParam);
    }


    bool EditorShellV3::ExecuteAddComponent_(
        noc::TypeId componentTypeId)
    {
        if (!session_)
            return false;

        const noc::EntityHandle entity =
            session_->SelectedEntity();
        if (!entity.IsValid())
            return false;

        auto context =
            session_->CommandContext();

        try
        {
            auto command =
                std::make_unique<AddComponentCommand>();

            if (!command->Init(
                    context,
                    entity,
                    componentTypeId)
                || !session_->History().Execute(
                    context,
                    std::move(command)))
            {
                AppendConsole_(
                    L"Add Component failed.");
                return false;
            }
        }
        catch (const std::bad_alloc&)
        {
            AppendConsole_(
                L"Add Component failed: allocation failure.");
            return false;
        }

        session_->NotifyAuthoredMutation();
        PopulateScene_();
        PostMessageW(
            hwnd_,
            WM_NOC_V3_INSPECTOR_REFRESH,
            0,
            0);
        UpdateStatus_();
        AppendConsole_(L"Component added.");
        return true;
    }


    bool EditorShellV3::ExecuteRemoveComponent_(
        noc::TypeId componentTypeId)
    {
        if (!session_)
            return false;

        const noc::EntityHandle entity =
            session_->SelectedEntity();
        if (!entity.IsValid())
            return false;

        auto context =
            session_->CommandContext();

        try
        {
            auto command =
                std::make_unique<RemoveComponentCommand>();

            if (!command->Init(
                    context,
                    entity,
                    componentTypeId)
                || !session_->History().Execute(
                    context,
                    std::move(command)))
            {
                AppendConsole_(
                    L"Remove Component failed.");
                return false;
            }
        }
        catch (const std::bad_alloc&)
        {
            AppendConsole_(
                L"Remove Component failed: allocation failure.");
            return false;
        }

        session_->NotifyAuthoredMutation();
        PopulateScene_();
        PostMessageW(
            hwnd_,
            WM_NOC_V3_INSPECTOR_REFRESH,
            0,
            0);
        UpdateStatus_();
        AppendConsole_(L"Component removed.");
        return true;
    }


    void EditorShellV3::ShowAddComponentPopup_()
    {
        if (!engine_
            || !session_
            || !inspectorAddComponent_)
        {
            return;
        }

        const noc::EntityHandle entity =
            session_->SelectedEntity();
        if (!entity.IsValid())
            return;

        const noc::ReflectionRegistry& reflection =
            engine_->Reflection();
        noc::World& world =
            engine_->GetWorld();

        std::vector<noc::TypeId> candidates;

        try
        {
            candidates.reserve(
                reflection.ComponentTypeCount());
        }
        catch (const std::bad_alloc&)
        {
            AppendConsole_(
                L"Add Component menu allocation failed.");
            return;
        }

        HMENU menu = CreatePopupMenu();
        if (!menu)
            return;

        for (uint32_t i = 0;
             i < reflection.ComponentTypeCount();
             ++i)
        {
            const noc::TypeMetadata* type =
                reflection.ComponentTypeAt(i);

            if (!type
                || !type->componentMetadata
                || !noc::HasFlag(
                    type->componentMetadata->flags,
                    noc::ComponentReflectionFlags::EditorAddable)
                || type->componentMetadata->has(
                    world,
                    entity))
            {
                continue;
            }

            try
            {
                candidates.push_back(
                    type->typeId);
            }
            catch (const std::bad_alloc&)
            {
                DestroyMenu(menu);
                AppendConsole_(
                    L"Add Component menu allocation failed.");
                return;
            }

            const noc::AttributeMetadata* display =
                reflection.FindTypeAttribute(
                    type->typeId,
                    noc::AttributeKind::DisplayName);

            const char* labelUtf8 =
                display
                && display->valueKind
                    == noc::AttributeValueKind::String
                && display->stringValue
                    ? display->stringValue
                    : type->canonicalName;

            const std::wstring label =
                Utf8ToWide_(labelUtf8);

            AppendMenuW(
                menu,
                MF_STRING,
                static_cast<UINT_PTR>(
                    candidates.size()),
                label.empty()
                    ? L"Unnamed Component"
                    : label.c_str());
        }

        if (candidates.empty())
        {
            AppendMenuW(
                menu,
                MF_STRING | MF_GRAYED,
                0,
                L"No addable components");
        }

        RECT buttonRect{};
        GetWindowRect(
            inspectorAddComponent_,
            &buttonRect);

        const int command =
            TrackPopupMenuEx(
                menu,
                TPM_RETURNCMD
                    | TPM_LEFTALIGN
                    | TPM_TOPALIGN,
                buttonRect.left,
                buttonRect.bottom + 2,
                hwnd_,
                nullptr);

        if (command > 0
            && static_cast<size_t>(command)
                <= candidates.size())
        {
            (void)ExecuteAddComponent_(
                candidates[
                    static_cast<size_t>(command - 1)]);
        }

        DestroyMenu(menu);
    }

}
