#include "EditorInspectorModel.h"

#include "EditorCommands.h"
#include "Runtime/Bounds.h"
#include "Runtime/Reflection/BuiltinTypes.h"
#include "Runtime/Reflection/ComponentReflection.h"
#include "Runtime/Reflection/PropertyAccess.h"
#include "Runtime/Reflection/ReflectionRegistry.h"
#include "Runtime/Reflection/ReflectionString.h"
#include "Runtime/World.h"
#include "Resources/ResourceHandle.h"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <new>

namespace nocturne::editor
{
    namespace
    {
        const char* DisplayNameOrCanonical(
            const noc::ReflectionRegistry& registry,
            noc::TypeId typeId,
            const char* canonical)
        {
            const noc::AttributeMetadata* display =
                registry.FindTypeAttribute(
                    typeId,
                    noc::AttributeKind::DisplayName);

            return display
                && display->valueKind
                    == noc::AttributeValueKind::String
                && display->stringValue
                    && display->stringValue[0] != '\0'
                    ? display->stringValue
                    : canonical;
        }

        const char* PropertyDisplayName(
            const noc::ReflectionRegistry& registry,
            noc::TypeId owner,
            const noc::PropertyMetadata& property)
        {
            const noc::AttributeMetadata* display =
                registry.FindPropertyAttribute(
                    owner,
                    property.propertyId,
                    noc::AttributeKind::DisplayName);

            return display
                && display->valueKind
                    == noc::AttributeValueKind::String
                && display->stringValue
                    && display->stringValue[0] != '\0'
                    ? display->stringValue
                    : property.canonicalName;
        }

        bool SupportsGenericTextEdit(
            const noc::TypeMetadata& valueType)
        {
            switch (valueType.kind)
            {
            case noc::TypeKind::Bool:
            case noc::TypeKind::SignedInteger:
            case noc::TypeKind::UnsignedInteger:
            case noc::TypeKind::FloatingPoint:
            case noc::TypeKind::String:
            case noc::TypeKind::Enum:
                return true;

            case noc::TypeKind::Struct:
                return valueType.typeId == noc::BuiltinTypeIds::Vec2
                    || valueType.typeId == noc::BuiltinTypeIds::Vec3
                    || valueType.typeId == noc::BuiltinTypeIds::Vec4
                    || valueType.typeId == noc::BuiltinTypeIds::Quat
                    || valueType.typeId == noc::BuiltinTypeIds::AABB;

            default:
                return false;
            }
        }

        bool ParseFloatList(
            const char* text,
            float* values,
            uint32_t count)
        {
            if (!text || !values || count == 0)
                return false;

            const char* cursor = text;

            for (uint32_t i = 0; i < count; ++i)
            {
                while (*cursor == ' '
                    || *cursor == '\t'
                    || *cursor == ','
                    || *cursor == ';')
                {
                    ++cursor;
                }

                if (*cursor == '\0')
                    return false;

                errno = 0;
                char* end = nullptr;
                const float value =
                    std::strtof(cursor, &end);

                if (end == cursor
                    || errno == ERANGE
                    || !std::isfinite(value))
                {
                    return false;
                }

                values[i] = value;
                cursor = end;
            }

            while (*cursor == ' '
                || *cursor == '\t'
                || *cursor == ','
                || *cursor == ';')
            {
                ++cursor;
            }

            return *cursor == '\0';
        }

        template <typename T>
        bool ExecuteValueCommand(
            EditorCommandContext& context,
            EditorCommandHistory& history,
            noc::EntityHandle entity,
            noc::TypeId componentTypeId,
            noc::PropertyId propertyId,
            noc::TypeId valueTypeId,
            const T& value)
        {
            try
            {
                auto command =
                    std::make_unique<
                        SetReflectedPropertyCommand>();

                if (!command->Init(
                        context,
                        entity,
                        componentTypeId,
                        propertyId,
                        noc::ReflectedConstValueView{
                            valueTypeId,
                            &value }))
                {
                    return false;
                }

                return history.Execute(
                    context,
                    std::move(command));
            }
            catch (const std::bad_alloc&)
            {
                return false;
            }
        }
    }

    bool EditorInspectorModel::Refresh(
        EditorCommandContext& context,
        noc::EntityHandle entity)
    {
        Clear();

        if (!entity.IsValid())
            return true;

        if (!context.world.IsAlive(entity)
            || context.IsToolOwned(entity))
        {
            return false;
        }

        try
        {
            const uint32_t componentCount =
                noc::ReflectedComponentCountForEntity(
                    context.reflection,
                    context.world,
                    entity);

            components_.reserve(componentCount);

            for (uint32_t componentIndex = 0;
                 componentIndex < componentCount;
                 ++componentIndex)
            {
                const noc::TypeMetadata* type =
                    noc::ReflectedComponentAtForEntity(
                        context.reflection,
                        context.world,
                        entity,
                        componentIndex);

                if (!type
                    || !noc::HasFlag(
                        type->flags,
                        noc::TypeFlags::EditorVisible))
                {
                    continue;
                }

                InspectorComponentView component{};
                component.typeId = type->typeId;
                component.displayName =
                    DisplayNameOrCanonical(
                        context.reflection,
                        type->typeId,
                        type->canonicalName);

                component.properties.reserve(
                    type->propertyCount);

                noc::ComponentPropertyRuntimeContext runtime{
                    &context.world,
                    entity
                };
                noc::PropertyAccessContext propertyContext =
                    noc::MakeComponentPropertyAccessContext(
                        runtime);

                for (uint32_t i = 0;
                     i < type->propertyCount;
                     ++i)
                {
                    const noc::PropertyMetadata& property =
                        type->properties[i];

                    if (!noc::HasFlag(
                            property.flags,
                            noc::PropertyFlags::EditorVisible)
                        || noc::HasFlag(
                            property.flags,
                            noc::PropertyFlags::Hidden))
                    {
                        continue;
                    }

                    const noc::TypeMetadata* valueType =
                        context.reflection.FindType(
                            property.valueTypeId);
                    if (!valueType)
                        return false;

                    noc::OwnedReflectedValue value;
                    if (noc::ReadPropertyValue(
                            context.reflection,
                            property,
                            propertyContext,
                            context.allocator,
                            value)
                        != noc::PropertyAccessStatus::Success)
                    {
                        return false;
                    }

                    InspectorPropertyView view{};
                    view.propertyId = property.propertyId;
                    view.valueTypeId = property.valueTypeId;
                    view.valueKind = valueType->kind;
                    view.flags = property.flags;
                    view.displayName =
                        PropertyDisplayName(
                            context.reflection,
                            type->typeId,
                            property);
                    view.editable =
                        property.write != nullptr
                        && !noc::HasFlag(
                            property.flags,
                            noc::PropertyFlags::ReadOnly)
                        && SupportsGenericTextEdit(*valueType);

                    if (!FormatValue_(
                            context,
                            *valueType,
                            value,
                            view.displayValue))
                    {
                        return false;
                    }

                    component.properties.push_back(
                        std::move(view));
                }

                components_.push_back(
                    std::move(component));
            }
        }
        catch (const std::bad_alloc&)
        {
            Clear();
            return false;
        }

        entity_ = entity;
        return true;
    }

    void EditorInspectorModel::Clear() noexcept
    {
        components_.clear();
        entity_ = noc::EntityHandle::Invalid();
    }

    noc::EntityHandle
    EditorInspectorModel::Entity() const noexcept
    {
        return entity_;
    }

    const std::vector<InspectorComponentView>&
    EditorInspectorModel::Components() const noexcept
    {
        return components_;
    }

    const InspectorPropertyView*
    EditorInspectorModel::FindProperty(
        noc::TypeId componentTypeId,
        noc::PropertyId propertyId) const noexcept
    {
        for (const InspectorComponentView& component :
             components_)
        {
            if (component.typeId != componentTypeId)
                continue;

            for (const InspectorPropertyView& property :
                 component.properties)
            {
                if (property.propertyId == propertyId)
                    return &property;
            }
        }

        return nullptr;
    }

    bool EditorInspectorModel::CommitTextEdit(
        EditorCommandContext& context,
        EditorCommandHistory& history,
        noc::EntityHandle entity,
        noc::TypeId componentTypeId,
        noc::PropertyId propertyId,
        const char* utf8Text)
    {
        if (!utf8Text
            || !context.world.IsAlive(entity)
            || context.IsToolOwned(entity))
        {
            return false;
        }

        const noc::PropertyMetadata* property =
            context.reflection.FindProperty(
                componentTypeId,
                propertyId);
        if (!property
            || !property->write
            || noc::HasFlag(
                property->flags,
                noc::PropertyFlags::ReadOnly))
        {
            return false;
        }

        const noc::TypeMetadata* valueType =
            context.reflection.FindType(
                property->valueTypeId);
        if (!valueType)
            return false;

        switch (valueType->kind)
        {
        case noc::TypeKind::Bool:
        {
            bool value = false;
            if (std::strcmp(utf8Text, "true") == 0
                || std::strcmp(utf8Text, "1") == 0)
            {
                value = true;
            }
            else if (std::strcmp(utf8Text, "false") != 0
                && std::strcmp(utf8Text, "0") != 0)
            {
                return false;
            }

            return ExecuteValueCommand(
                context,
                history,
                entity,
                componentTypeId,
                propertyId,
                valueType->typeId,
                value);
        }

        case noc::TypeKind::SignedInteger:
        {
            errno = 0;
            char* end = nullptr;
            const long long parsed =
                std::strtoll(utf8Text, &end, 10);
            if (end == utf8Text
                || *end != '\0'
                || errno == ERANGE)
            {
                return false;
            }

            if (valueType->size == sizeof(int8_t))
            {
                if (parsed < (std::numeric_limits<int8_t>::min)()
                    || parsed > (std::numeric_limits<int8_t>::max)())
                {
                    return false;
                }
                const int8_t value =
                    static_cast<int8_t>(parsed);
                return ExecuteValueCommand(
                    context, history, entity,
                    componentTypeId, propertyId,
                    valueType->typeId, value);
            }
            if (valueType->size == sizeof(int16_t))
            {
                if (parsed < (std::numeric_limits<int16_t>::min)()
                    || parsed > (std::numeric_limits<int16_t>::max)())
                {
                    return false;
                }
                const int16_t value =
                    static_cast<int16_t>(parsed);
                return ExecuteValueCommand(
                    context, history, entity,
                    componentTypeId, propertyId,
                    valueType->typeId, value);
            }
            if (valueType->size == sizeof(int32_t))
            {
                if (parsed < (std::numeric_limits<int32_t>::min)()
                    || parsed > (std::numeric_limits<int32_t>::max)())
                {
                    return false;
                }
                const int32_t value =
                    static_cast<int32_t>(parsed);
                return ExecuteValueCommand(
                    context, history, entity,
                    componentTypeId, propertyId,
                    valueType->typeId, value);
            }

            const int64_t value =
                static_cast<int64_t>(parsed);
            return ExecuteValueCommand(
                context, history, entity,
                componentTypeId, propertyId,
                valueType->typeId, value);
        }

        case noc::TypeKind::UnsignedInteger:
        {
            errno = 0;
            char* end = nullptr;
            if (utf8Text[0] == '-')
                return false;

            const unsigned long long parsed =
                std::strtoull(utf8Text, &end, 10);
            if (end == utf8Text
                || *end != '\0'
                || errno == ERANGE)
            {
                return false;
            }

            if (valueType->size == sizeof(uint8_t))
            {
                if (parsed > (std::numeric_limits<uint8_t>::max)())
                    return false;
                const uint8_t value =
                    static_cast<uint8_t>(parsed);
                return ExecuteValueCommand(
                    context, history, entity,
                    componentTypeId, propertyId,
                    valueType->typeId, value);
            }
            if (valueType->size == sizeof(uint16_t))
            {
                if (parsed > (std::numeric_limits<uint16_t>::max)())
                    return false;
                const uint16_t value =
                    static_cast<uint16_t>(parsed);
                return ExecuteValueCommand(
                    context, history, entity,
                    componentTypeId, propertyId,
                    valueType->typeId, value);
            }
            if (valueType->size == sizeof(uint32_t))
            {
                if (parsed > (std::numeric_limits<uint32_t>::max)())
                    return false;
                const uint32_t value =
                    static_cast<uint32_t>(parsed);
                return ExecuteValueCommand(
                    context, history, entity,
                    componentTypeId, propertyId,
                    valueType->typeId, value);
            }

            const uint64_t value =
                static_cast<uint64_t>(parsed);
            return ExecuteValueCommand(
                context, history, entity,
                componentTypeId, propertyId,
                valueType->typeId, value);
        }

        case noc::TypeKind::FloatingPoint:
        {
            errno = 0;
            char* end = nullptr;
            const double parsed =
                std::strtod(utf8Text, &end);
            if (end == utf8Text
                || *end != '\0'
                || errno == ERANGE
                || !std::isfinite(parsed))
            {
                return false;
            }

            if (valueType->size == sizeof(float))
            {
                const float value =
                    static_cast<float>(parsed);
                return std::isfinite(value)
                    && ExecuteValueCommand(
                        context, history, entity,
                        componentTypeId, propertyId,
                        valueType->typeId, value);
            }

            const double value = parsed;
            return ExecuteValueCommand(
                context, history, entity,
                componentTypeId, propertyId,
                valueType->typeId, value);
        }

        case noc::TypeKind::String:
        {
            noc::ReflectionString value(
                context.allocator);
            if (!value.Assign(utf8Text))
                return false;

            return ExecuteValueCommand(
                context, history, entity,
                componentTypeId, propertyId,
                valueType->typeId, value);
        }

        case noc::TypeKind::Enum:
        {
            const noc::EnumValueMetadata* enumValue =
                context.reflection.FindEnumValueByName(
                    valueType->typeId,
                    utf8Text);
            if (!enumValue)
                return false;

            const uint64_t raw = enumValue->rawValue;
            if (valueType->size == 1)
            {
                const uint8_t value =
                    static_cast<uint8_t>(raw);
                return ExecuteValueCommand(
                    context, history, entity,
                    componentTypeId, propertyId,
                    valueType->typeId, value);
            }
            if (valueType->size == 2)
            {
                const uint16_t value =
                    static_cast<uint16_t>(raw);
                return ExecuteValueCommand(
                    context, history, entity,
                    componentTypeId, propertyId,
                    valueType->typeId, value);
            }
            if (valueType->size == 4)
            {
                const uint32_t value =
                    static_cast<uint32_t>(raw);
                return ExecuteValueCommand(
                    context, history, entity,
                    componentTypeId, propertyId,
                    valueType->typeId, value);
            }

            const uint64_t value = raw;
            return ExecuteValueCommand(
                context, history, entity,
                componentTypeId, propertyId,
                valueType->typeId, value);
        }

        case noc::TypeKind::Struct:
        {
            float values[6]{};

            if (valueType->typeId == noc::BuiltinTypeIds::Vec2
                && ParseFloatList(utf8Text, values, 2))
            {
                const noc::Vec2 value{
                    values[0], values[1] };
                return ExecuteValueCommand(
                    context, history, entity,
                    componentTypeId, propertyId,
                    valueType->typeId, value);
            }

            if (valueType->typeId == noc::BuiltinTypeIds::Vec3
                && ParseFloatList(utf8Text, values, 3))
            {
                const noc::Vec3 value{
                    values[0], values[1], values[2] };
                return ExecuteValueCommand(
                    context, history, entity,
                    componentTypeId, propertyId,
                    valueType->typeId, value);
            }

            if (valueType->typeId == noc::BuiltinTypeIds::Vec4
                && ParseFloatList(utf8Text, values, 4))
            {
                const noc::Vec4 value{
                    values[0], values[1],
                    values[2], values[3] };
                return ExecuteValueCommand(
                    context, history, entity,
                    componentTypeId, propertyId,
                    valueType->typeId, value);
            }

            if (valueType->typeId == noc::BuiltinTypeIds::Quat
                && ParseFloatList(utf8Text, values, 4))
            {
                const noc::Quat value{
                    values[0], values[1],
                    values[2], values[3] };
                return ExecuteValueCommand(
                    context, history, entity,
                    componentTypeId, propertyId,
                    valueType->typeId, value);
            }

            if (valueType->typeId == noc::BuiltinTypeIds::AABB
                && ParseFloatList(utf8Text, values, 6))
            {
                const noc::AABB value{
                    noc::Vec3{
                        values[0], values[1], values[2] },
                    noc::Vec3{
                        values[3], values[4], values[5] }
                };
                return ExecuteValueCommand(
                    context, history, entity,
                    componentTypeId, propertyId,
                    valueType->typeId, value);
            }

            return false;
        }

        default:
            return false;
        }
    }

    bool EditorInspectorModel::FormatValue_(
        EditorCommandContext& context,
        const noc::TypeMetadata& valueType,
        const noc::OwnedReflectedValue& value,
        std::string& outText) const
    {
        char buffer[256]{};

        switch (valueType.kind)
        {
        case noc::TypeKind::Bool:
            outText =
                *static_cast<const bool*>(value.Data())
                    ? "true"
                    : "false";
            return true;

        case noc::TypeKind::SignedInteger:
            if (valueType.size == 1)
                std::snprintf(buffer, sizeof(buffer), "%d",
                    static_cast<int>(*static_cast<const int8_t*>(value.Data())));
            else if (valueType.size == 2)
                std::snprintf(buffer, sizeof(buffer), "%d",
                    static_cast<int>(*static_cast<const int16_t*>(value.Data())));
            else if (valueType.size == 4)
                std::snprintf(buffer, sizeof(buffer), "%d",
                    *static_cast<const int32_t*>(value.Data()));
            else
                std::snprintf(buffer, sizeof(buffer), "%lld",
                    static_cast<long long>(*static_cast<const int64_t*>(value.Data())));
            outText = buffer;
            return true;

        case noc::TypeKind::UnsignedInteger:
            if (valueType.size == 1)
                std::snprintf(buffer, sizeof(buffer), "%u",
                    static_cast<unsigned>(*static_cast<const uint8_t*>(value.Data())));
            else if (valueType.size == 2)
                std::snprintf(buffer, sizeof(buffer), "%u",
                    static_cast<unsigned>(*static_cast<const uint16_t*>(value.Data())));
            else if (valueType.size == 4)
                std::snprintf(buffer, sizeof(buffer), "%u",
                    *static_cast<const uint32_t*>(value.Data()));
            else
                std::snprintf(buffer, sizeof(buffer), "%llu",
                    static_cast<unsigned long long>(*static_cast<const uint64_t*>(value.Data())));
            outText = buffer;
            return true;

        case noc::TypeKind::FloatingPoint:
            if (valueType.size == sizeof(float))
                std::snprintf(buffer, sizeof(buffer), "%.6g",
                    static_cast<double>(*static_cast<const float*>(value.Data())));
            else
                std::snprintf(buffer, sizeof(buffer), "%.10g",
                    *static_cast<const double*>(value.Data()));
            outText = buffer;
            return true;

        case noc::TypeKind::String:
            outText =
                static_cast<const noc::ReflectionString*>(
                    value.Data())->CStr();
            return true;

        case noc::TypeKind::Enum:
        {
            uint64_t raw = 0;
            std::memcpy(
                &raw,
                value.Data(),
                valueType.size);

            const noc::EnumValueMetadata* enumValue =
                context.reflection.FindEnumValueByRawValue(
                    valueType.typeId,
                    raw);

            if (enumValue)
                outText = enumValue->canonicalName;
            else
            {
                std::snprintf(
                    buffer,
                    sizeof(buffer),
                    "%llu",
                    static_cast<unsigned long long>(raw));
                outText = buffer;
            }
            return true;
        }

        case noc::TypeKind::EntityReference:
        {
            const auto& handle =
                *static_cast<const noc::EntityHandle*>(
                    value.Data());
            std::snprintf(
                buffer,
                sizeof(buffer),
                "%u:%u",
                handle.index,
                handle.generation);
            outText = buffer;
            return true;
        }

        case noc::TypeKind::ResourceReference:
        {
            const auto& handle =
                *static_cast<const noc::ResourceHandle*>(
                    value.Data());
            std::snprintf(
                buffer,
                sizeof(buffer),
                "%u:%u",
                handle.index,
                handle.generation);
            outText = buffer;
            return true;
        }

        case noc::TypeKind::Struct:
            if (valueType.typeId == noc::BuiltinTypeIds::Vec2)
            {
                const auto& v =
                    *static_cast<const noc::Vec2*>(value.Data());
                std::snprintf(buffer, sizeof(buffer),
                    "%.4g, %.4g", v.x, v.y);
                outText = buffer; return true;
            }
            if (valueType.typeId == noc::BuiltinTypeIds::Vec3)
            {
                const auto& v =
                    *static_cast<const noc::Vec3*>(value.Data());
                std::snprintf(buffer, sizeof(buffer),
                    "%.4g, %.4g, %.4g", v.x, v.y, v.z);
                outText = buffer; return true;
            }
            if (valueType.typeId == noc::BuiltinTypeIds::Vec4)
            {
                const auto& v =
                    *static_cast<const noc::Vec4*>(value.Data());
                std::snprintf(buffer, sizeof(buffer),
                    "%.4g, %.4g, %.4g, %.4g",
                    v.x, v.y, v.z, v.w);
                outText = buffer; return true;
            }
            if (valueType.typeId == noc::BuiltinTypeIds::Quat)
            {
                const auto& q =
                    *static_cast<const noc::Quat*>(value.Data());
                std::snprintf(buffer, sizeof(buffer),
                    "%.4g, %.4g, %.4g, %.4g",
                    q.x, q.y, q.z, q.w);
                outText = buffer; return true;
            }
            if (valueType.typeId == noc::BuiltinTypeIds::AABB)
            {
                const auto& b =
                    *static_cast<const noc::AABB*>(value.Data());
                std::snprintf(
                    buffer,
                    sizeof(buffer),
                    "%.4g, %.4g, %.4g, %.4g, %.4g, %.4g",
                    b.min.x, b.min.y, b.min.z,
                    b.max.x, b.max.y, b.max.z);
                outText = buffer; return true;
            }

            outText = "{...}";
            return true;

        case noc::TypeKind::FixedArray:
        case noc::TypeKind::DynamicSequence:
            outText = "[...]";
            return true;

        case noc::TypeKind::Opaque:
            outText = "<opaque>";
            return true;

        default:
            outText = "<unsupported>";
            return true;
        }
    }
}
