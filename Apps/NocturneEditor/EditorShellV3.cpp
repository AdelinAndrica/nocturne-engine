#include "EditorShellV3.h"
#include "EditorShellV3Controls.h"
#include "EditorSession.h"
#include "EditorCommands.h"
#include "EditorTransformMath.h"
#include "Runtime/Entity.h"
#include "EditorTheme.h"
#include "EditorIconRenderer.h"
#include "NocturneEditorResource.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <new>
#include <sstream>
#include <string>
#include <vector>

#include <CommCtrl.h>
#include <Windowsx.h>
#include <dwmapi.h>
#include <Richedit.h>

#include "Core/Log.h"
#include "Runtime/Engine.h"
#include "Runtime/World.h"
#include "Runtime/Components/TransformComponent.h"
#include "Runtime/Reflection/BuiltinTypes.h"
#include "Resources/Typed/MeshResource.h"

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Dwmapi.lib")

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

    bool EditorShellV3::Init(
        noc::Engine& engine,
        noc::WinWindow& window,
        EditorSession& session)
    {
        engine_ = &engine; window_ = &window; session_ = &session; hwnd_ = static_cast<HWND>(window.Handle()); if (!hwnd_) return false;
        const HINSTANCE instance = GetModuleHandleW(nullptr);
        const auto appIcon = reinterpret_cast<HICON>(LoadImageW(
            instance, MAKEINTRESOURCEW(IDI_NOCTURNE_EDITOR), IMAGE_ICON,
            GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON),
            LR_DEFAULTCOLOR | LR_SHARED));
        const auto appIconSmall = reinterpret_cast<HICON>(LoadImageW(
            instance, MAKEINTRESOURCEW(IDI_NOCTURNE_EDITOR), IMAGE_ICON,
            GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
            LR_DEFAULTCOLOR | LR_SHARED));
        if (appIcon) SendMessageW(hwnd_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(appIcon));
        if (appIconSmall) SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(appIconSmall));
        if (!RegisterV3Classes(instance)) return false;
        constexpr DWORD kDark = 20; BOOL dark = TRUE; DwmSetWindowAttribute(hwnd_, kDark, &dark, sizeof(dark));

        uiFont_ = MakeFont(12, FW_NORMAL, L"Segoe UI Variable Text"); uiBold_ = MakeFont(12, FW_SEMIBOLD, L"Segoe UI Variable Text"); menuFont_ = MakeFont(14, FW_SEMIBOLD, L"Segoe UI Variable Text");
        smallFont_ = MakeFont(11, FW_NORMAL, L"Segoe UI Variable Text"); consoleFont_ = MakeFont(11, FW_NORMAL, L"Cascadia Mono"); brandFont_ = MakeFont(42, FW_SEMIBOLD, L"Segoe UI Variable Display");
        const auto& c = EditorTheme::Colors(); windowBrush_ = CreateSolidBrush(c.windowBg); editBrush_ = CreateSolidBrush(c.inputBg);

        std::wstring configured = L"Data";
        const auto& cfg = engine.Config();
        if (cfg.contentRoot && cfg.contentRoot[0] != '\0') configured = Utf8ToWide_(cfg.contentRoot);
        contentRoot_ = ResolveContentRoot_(configured);

        CreateChrome_(); CreateToolbar_(); CreatePanels_(); PopulateScene_(); PopulateContent_(); window.SetMessageSink(this);
        RECT rc{}; GetClientRect(hwnd_, &rc); Layout_(static_cast<int>(rc.right - rc.left), static_cast<int>(rc.bottom - rc.top));
        RedrawWindow(hwnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
        AppendConsole_(L"Nocturne Editor initialized."); AppendConsole_(L"Phase 13: UI Fidelity Pass 3 active."); AppendConsole_(L"Vector icons, toolbar grouping and Content Browser composition enabled.");
        UpdateStatus_(); NOC_LOG_INFO("Editor", "Nocturne Editor UI Fidelity Pass 3 initialized"); return true;
    }

    void EditorShellV3::Shutdown()
    {
        DestroyInspectorControls_();

        if (inspector_.body)
        {
            RemoveWindowSubclass(
                inspector_.body,
                &EditorShellV3::InspectorBodySubclassProc_,
                0x1630);
        }

        if (renameEdit_)
        {
            RemoveWindowSubclass(
                renameEdit_,
                &EditorShellV3::RenameEditSubclassProc_,
                0x1610);
            DestroyWindow(renameEdit_);
            renameEdit_ = nullptr;
        }
        renameEntity_ = noc::EntityHandle::Invalid();
        renameEnding_ = false;

        if (window_) window_->SetMessageSink(nullptr);
        if (fileMenu_) DestroyMenu(fileMenu_);
        if (buildMenu_) DestroyMenu(buildMenu_);
        if (actorMenu_) DestroyMenu(actorMenu_);
        fileMenu_ = buildMenu_ = actorMenu_ = nullptr;
        if (uiFont_) DeleteObject(uiFont_); if (uiBold_) DeleteObject(uiBold_); if (menuFont_) DeleteObject(menuFont_); if (smallFont_) DeleteObject(smallFont_); if (consoleFont_) DeleteObject(consoleFont_); if (brandFont_) DeleteObject(brandFont_);
        if (windowBrush_) DeleteObject(windowBrush_); if (editBrush_) DeleteObject(editBrush_);
        uiFont_ = uiBold_ = menuFont_ = smallFont_ = consoleFont_ = brandFont_ = nullptr; windowBrush_ = editBrush_ = nullptr; session_ = nullptr; engine_ = nullptr; window_ = nullptr; hwnd_ = nullptr;
    }

    void EditorShellV3::CreateChrome_()
    {
        menuBand_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW, 0,0,0,0, hwnd_, nullptr, GetModuleHandleW(nullptr), nullptr);
        const struct M { const wchar_t* text; int id; } defs[] = { {L"File",IdMenuFile},{L"Edit",IdMenuEdit},{L"Window",IdMenuWindow},{L"Tools",IdMenuTools},{L"Build",IdMenuBuild},{L"Select",IdMenuSelect},{L"Actor",IdMenuActor},{L"Help",IdMenuHelp} };
        for (const auto& d : defs) menuButtons_.push_back(MakeButton(hwnd_, d.id, d.text, Icon::None, ButtonKind::Menu, menuFont_));
        fileMenu_ = CreatePopupMenu(); AppendMenuW(fileMenu_, MF_STRING, IdToolbarNew, L"New Scene"); AppendMenuW(fileMenu_, MF_STRING, IdToolbarOpen, L"Open Scene..."); AppendMenuW(fileMenu_, MF_STRING, IdToolbarSave, L"Save Scene"); AppendMenuW(fileMenu_, MF_SEPARATOR, 0, nullptr); AppendMenuW(fileMenu_, MF_STRING, IDCANCEL, L"Exit");
        buildMenu_ = CreatePopupMenu(); AppendMenuW(buildMenu_, MF_STRING, IdToolbarBuild, L"Build Content"); AppendMenuW(buildMenu_, MF_STRING, IdToolbarPlay, L"Play");
        actorMenu_ = CreatePopupMenu();
        HMENU createMenu = CreatePopupMenu();
        HMENU lightMenu = CreatePopupMenu();
        HMENU primitiveMenu = CreatePopupMenu();
        AppendMenuW(createMenu, MF_STRING, IdActorCreate, L"Empty Entity");
        AppendMenuW(createMenu, MF_STRING, IdActorCreateStaticMesh, L"Static Mesh Entity");
        AppendMenuW(createMenu, MF_STRING, IdActorCreateCamera, L"Camera");
        AppendMenuW(createMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(lightMenu, MF_STRING, IdActorCreateDirectionalLight, L"Directional Light");
        AppendMenuW(lightMenu, MF_STRING, IdActorCreatePointLight, L"Point Light");
        AppendMenuW(lightMenu, MF_STRING, IdActorCreateSpotLight, L"Spot Light");
        AppendMenuW(createMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(lightMenu), L"Light");
        AppendMenuW(createMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(primitiveMenu, MF_STRING, IdActorCreatePlane, L"Plane");
        AppendMenuW(primitiveMenu, MF_STRING, IdActorCreateCube, L"Cube");
        AppendMenuW(primitiveMenu, MF_STRING, IdActorCreateSphere, L"Sphere");
        AppendMenuW(primitiveMenu, MF_STRING, IdActorCreateCylinder, L"Cylinder");
        AppendMenuW(createMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(primitiveMenu), L"Primitive");
        AppendMenuW(actorMenu_, MF_POPUP, reinterpret_cast<UINT_PTR>(createMenu), L"Create");
        AppendMenuW(actorMenu_, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(actorMenu_, MF_STRING, IdActorDuplicate, L"Duplicate Entity\tCtrl+D");
        AppendMenuW(actorMenu_, MF_STRING, IdActorDelete, L"Delete Entity\tDelete");
        AppendMenuW(actorMenu_, MF_STRING, IdActorRename, L"Rename Entity\tF2");
    }

    void EditorShellV3::CreateToolbar_()
    {
        toolbarBand_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW, 0,0,0,0, hwnd_, nullptr, GetModuleHandleW(nullptr), nullptr);
        const struct T { const wchar_t* text; int id; Icon icon; ButtonKind kind; } defs[] = {
            {L"New",IdToolbarNew,Icon::Document,ButtonKind::Neutral},{L"Open",IdToolbarOpen,Icon::Folder,ButtonKind::Neutral},{L"Save",IdToolbarSave,Icon::Save,ButtonKind::Neutral},
            {L"Undo",IdToolbarUndo,Icon::Undo,ButtonKind::Neutral},{L"Redo",IdToolbarRedo,Icon::Redo,ButtonKind::Neutral},
            {L"Select",IdToolbarSelect,Icon::Cursor,ButtonKind::Tool},{L"Move",IdToolbarMove,Icon::Move,ButtonKind::Tool},{L"Rotate",IdToolbarRotate,Icon::Rotate,ButtonKind::Tool},{L"Scale",IdToolbarScale,Icon::Scale,ButtonKind::Tool},
            {L"Play",IdToolbarPlay,Icon::Play,ButtonKind::Success},{L"Stop",IdToolbarStop,Icon::Stop,ButtonKind::Neutral},{L"Build",IdToolbarBuild,Icon::Cube,ButtonKind::Neutral} };
        for (const auto& d : defs) { HWND h = MakeButton(hwnd_, d.id, d.text, d.icon, d.kind, uiBold_); if (d.id == activeToolId_) ButtonActive(h, true); toolbarButtons_.push_back(h); }
    }

    void EditorShellV3::CreatePanels_()
    {
        auto makeBody = [&](int id = 0) { return CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | SS_OWNERDRAW, 0,0,0,0, hwnd_, id ? reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)) : nullptr, GetModuleHandleW(nullptr), nullptr); };
        scene_.header = MakeHeader(hwnd_, L"Scene Hierarchy", Icon::Hierarchy, uiBold_); sceneTree_ = MakeTree(hwnd_, IdSceneTree, uiFont_); scene_.body = sceneTree_;
        viewport_.header = MakeHeader(hwnd_, L"Viewport", Icon::Viewport, uiBold_);
        viewport_.body = makeBody();
        viewportPerspective_ = MakeButton(hwnd_, IdViewportPerspective, L"Perspective", Icon::None, ButtonKind::Tool, uiFont_);
        viewportLit_ = MakeButton(hwnd_, IdViewportLit, L"Lit", Icon::None, ButtonKind::Neutral, uiFont_);
        viewportShow_ = MakeButton(hwnd_, IdViewportShow, L"Show", Icon::None, ButtonKind::Neutral, uiFont_);
        viewportOrientation_ = MakeButton(
            hwnd_,
            IdViewportOrientation,
            session_ && session_->Orientation() == TransformOrientation::World
                ? L"World"
                : L"Local",
            Icon::None,
            ButtonKind::Tool,
            uiFont_);
        ButtonActive(viewportPerspective_, true);
        ButtonActive(viewportOrientation_, true);
        inspector_.header = MakeHeader(hwnd_, L"Inspector / Properties", Icon::Inspector, uiBold_);
        inspector_.body = makeBody(IdInspector);
        if (inspector_.body
            && !SetWindowSubclass(
                inspector_.body,
                &EditorShellV3::InspectorBodySubclassProc_,
                0x1630,
                reinterpret_cast<DWORD_PTR>(this)))
        {
            NOC_LOG_WARN(
                "Editor",
                "%s",
                "Inspector body subclass registration failed; wheel scrolling unavailable");
        }
        inspectorAddComponent_ = MakeButton(
            hwnd_,
            IdInspectorAddComponent,
            L"+ Add Component",
            Icon::None,
            ButtonKind::Neutral,
            uiFont_);
        ShowWindow(inspectorAddComponent_, SW_HIDE);
        content_.header = MakeHeader(hwnd_, L"Content Browser", Icon::Folder, uiBold_); content_.body = makeBody();
        contentSearch_ = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0,0,0,0, hwnd_, reinterpret_cast<HMENU>(IdContentSearch), GetModuleHandleW(nullptr), nullptr); SendMessageW(contentSearch_, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont_), FALSE); SendMessageW(contentSearch_, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Search Assets..."));
        contentListMode_ = MakeButton(hwnd_, IdContentListMode, L"", Icon::List, ButtonKind::IconOnly, uiFont_); contentGridMode_ = MakeButton(hwnd_, IdContentGridMode, L"", Icon::Grid, ButtonKind::IconOnly, uiFont_); contentSettings_ = MakeButton(hwnd_, IdContentSettings, L"", Icon::Settings, ButtonKind::IconOnly, uiFont_); ButtonActive(contentListMode_, true);
        contentTree_ = MakeTree(hwnd_, IdContentTree, uiFont_); contentTable_ = MakeTable(hwnd_, IdContentTable, uiFont_);
        console_.header = MakeHeader(hwnd_, L"Console / Output", Icon::Console, uiBold_); console_.body = makeBody(); LoadLibraryW(L"Msftedit.dll");
        consoleEdit_ = CreateWindowExW(0, MSFTEDIT_CLASS, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | ES_NOHIDESEL, 0,0,0,0, hwnd_, reinterpret_cast<HMENU>(IdConsole), GetModuleHandleW(nullptr), nullptr); SendMessageW(consoleEdit_, WM_SETFONT, reinterpret_cast<WPARAM>(consoleFont_), FALSE); SendMessageW(consoleEdit_, EM_SETBKGNDCOLOR, 0, EditorTheme::Colors().inputBg); SendMessageW(consoleEdit_, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(8,8)); consoleScroll_ = MakeScroll(hwnd_, IdConsoleScroll);
        buildPlay_.header = MakeHeader(hwnd_, L"Build / Play", Icon::Play, uiBold_); buildPlay_.body = makeBody(); playButton_ = MakeButton(hwnd_, IdPlay, L"Play (F5)", Icon::Play, ButtonKind::Primary, uiBold_); buildButton_ = MakeButton(hwnd_, IdBuild, L"Build", Icon::Cube, ButtonKind::Neutral, uiBold_);
        status_ = makeBody(IdStatus);
    }

    void EditorShellV3::PopulateScene_()
    {
        std::vector<EditorHierarchyExpansionEntry> expansionState;
        bool rootExpanded = true;
        TreeCaptureExpansion(
            sceneTree_,
            expansionState,
            rootExpanded);

        TreeClear(sceneTree_);
        TreeAdd(
            sceneTree_,
            L"Scene (Runtime World)",
            0,
            Icon::World,
            true,
            rootExpanded);

        if (!engine_ || !session_)
        {
            hierarchyModel_.Clear();
            return;
        }

        noc::World& world =
            engine_->GetWorld();

        if (!hierarchyModel_.Rebuild(
                world,
                session_->ToolCamera()))
        {
            AppendConsole_(
                L"Scene Hierarchy rebuild failed: allocation failure.");
            return;
        }

        bool invalidUtf8Detected = false;

        auto displayName = [&](noc::EntityHandle entity)
        {
            const noc::NameComponent* name =
                world.GetName(entity);

            if (name && name->value[0] != '\0')
            {
                // NameComponent payload is bounded, so a fixed buffer avoids
                // temporary conversion allocations and lets the hierarchy
                // distinguish invalid UTF-8 from an intentionally empty name.
                wchar_t converted[128]{};
                const int convertedCount =
                    MultiByteToWideChar(
                        CP_UTF8,
                        MB_ERR_INVALID_CHARS,
                        name->value,
                        -1,
                        converted,
                        static_cast<int>(
                            std::size(converted)));

                if (convertedCount > 1)
                    return std::wstring(converted);

                invalidUtf8Detected = true;
            }

            std::wstringstream fallback;
            fallback << L"Entity "
                << entity.index
                << L":"
                << entity.generation;
            return fallback.str();
        };

        auto iconFor = [&](noc::EntityHandle entity)
        {
            if (world.HasCamera(entity))
                return Icon::Camera;
            if (world.HasRenderable(entity))
                return Icon::Cube;
            return Icon::Hierarchy;
        };

        for (const EditorHierarchyRow& row :
             hierarchyModel_.Rows())
        {
            TreeAdd(
                sceneTree_,
                displayName(row.entity),
                row.depth,
                iconFor(row.entity),
                row.hasAuthoredChildren,
                EditorHierarchyWasExpanded(
                    expansionState,
                    row.entity,
                    true),
                row.entity);
        }

        if (invalidUtf8Detected
            && !hierarchyInvalidUtf8Reported_)
        {
            AppendConsole_(
                L"Scene Hierarchy: invalid UTF-8 entity name; using EntityHandle fallback label.");
        }

        hierarchyInvalidUtf8Reported_ =
            invalidUtf8Detected;

        SyncSceneSelection();
    }

    void EditorShellV3::SyncSceneSelection()
    {
        if (!sceneTree_ || !session_)
            return;

        TreeSelectEntity(
            sceneTree_,
            session_->SelectedEntity());
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

    void EditorShellV3::PopulateContent_()
    {
        TreeClear(contentTree_); TableClear(contentTable_); TreeAdd(contentTree_, L"Content", 0, Icon::Folder, true, true);
        namespace fs = std::filesystem; std::error_code ec; const fs::path root(contentRoot_);
        if (!fs::exists(root, ec)) { TreeAdd(contentTree_, L"Content root unavailable", 1, Icon::Folder); return; }
        std::vector<fs::directory_entry> entries; for (const auto& e : fs::directory_iterator(root, ec)) { if (ec) break; entries.push_back(e); }
        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) { return a.path().filename().wstring() < b.path().filename().wstring(); });
        for (const auto& e : entries) { const std::wstring name = e.path().filename().wstring(); const bool dir = e.is_directory(ec); const std::wstring type = AssetTypeForPath(e.path(), dir); if (dir) TreeAdd(contentTree_, name, 1, Icon::Folder); TableAdd(contentTable_, name, type, IconForType(type)); }
    }

    void EditorShellV3::Layout_(int w, int h)
    {
        if (w <= 0 || h <= 0) return;
        constexpr int menuH = 36, toolbarH = 48, statusH = 24, gap = 7, headerH = 29, buttonH = 34;
        MoveWindow(menuBand_, 0,0,w,menuH, TRUE); int menuX = 12; const int menuWidths[] = {54,54,78,64,62,68,62,58}; for (size_t i = 0; i < menuButtons_.size(); ++i) { const int bw = menuWidths[i]; MoveWindow(menuButtons_[i], menuX,2,bw,menuH-4,TRUE); menuX += bw + 2; }
        MoveWindow(toolbarBand_, 0,menuH,w,toolbarH,TRUE); int x = 12; const int widths[] = {76,80,76,80,80,90,84,88,84,86,82,86}; for (size_t i = 0; i < toolbarButtons_.size(); ++i) { const int bw = widths[i]; MoveWindow(toolbarButtons_[i], x, menuH + (toolbarH - buttonH) / 2, bw, buttonH, TRUE); x += bw + 4; if (i == 2 || i == 4 || i == 8) x += 10; }
        const int top = menuH + toolbarH + gap; const int statusY = (std::max)(top, h - statusH); const int available = (std::max)(0, statusY - top - gap); const int bottomH = (std::clamp)(available * 34 / 100, 205, 280); const int topH = (std::max)(140, available - bottomH - gap); const int bottomY = top + topH + gap; const int actualBottom = (std::max)(0, statusY - bottomY);
        const int leftW = (std::clamp)(w * 20 / 100, 250, 330); const int rightW = (std::clamp)(w * 22 / 100, 285, 355); const int centerX = gap + leftW + gap; const int centerW = (std::max)(260, w - leftW - rightW - 4 * gap); const int rightX = centerX + centerW + gap;
        auto panel = [&](Panel& p, int px, int py, int pw, int ph) { MoveWindow(p.header, px,py,pw,headerH,TRUE); MoveWindow(p.body, px,py+headerH,pw,(std::max)(0,ph-headerH),TRUE); };
        panel(scene_, gap,top,leftW,topH); panel(viewport_, centerX,top,centerW,topH); panel(inspector_, rightX,top,rightW,topH);
        const int viewportY = top + headerH;
        MoveWindow(viewportPerspective_, centerX + 10, viewportY + 10, 76, 30, TRUE);
        MoveWindow(viewportLit_, centerX + 90, viewportY + 10, 40, 30, TRUE);
        MoveWindow(viewportShow_, centerX + 134, viewportY + 10, 42, 30, TRUE);
        MoveWindow(
            viewportOrientation_,
            centerX + (std::max)(182, centerW - 78),
            viewportY + 10,
            68,
            30,
            TRUE);
        const int bottomLeft = (std::clamp)(w * 31 / 100, 345, 495); const int bottomCenterX = gap + bottomLeft + gap; const int bottomCenter = (std::max)(260, w - bottomLeft - rightW - 4 * gap); const int bottomRightX = bottomCenterX + bottomCenter + gap;
        panel(content_, gap,bottomY,bottomLeft,actualBottom); panel(console_, bottomCenterX,bottomY,bottomCenter,actualBottom); panel(buildPlay_, bottomRightX,bottomY,rightW,actualBottom);
        const int contentY = bottomY + headerH, contentH = (std::max)(0, actualBottom - headerH), pad = 9, searchH = 30, iconW = 30, iconGap = 4; const int actionsW = iconW * 3 + iconGap * 2;
        MoveWindow(contentSearch_, gap+pad,contentY+pad,(std::max)(80,bottomLeft-2*pad-actionsW-7),searchH,TRUE); const int actionX = gap + bottomLeft - pad - actionsW; MoveWindow(contentListMode_, actionX,contentY+pad,iconW,searchH,TRUE); MoveWindow(contentGridMode_, actionX+iconW+iconGap,contentY+pad,iconW,searchH,TRUE); MoveWindow(contentSettings_, actionX+(iconW+iconGap)*2,contentY+pad,iconW,searchH,TRUE);
        const int browserY = contentY + pad + searchH + 7, browserH = (std::max)(0, contentH - (2*pad + searchH + 7)); const int treeW = (std::clamp)(bottomLeft * 30 / 100, 110, 145); MoveWindow(contentTree_, gap+pad,browserY,treeW,browserH,TRUE); MoveWindow(contentTable_, gap+pad+treeW+7,browserY,(std::max)(0,bottomLeft-2*pad-treeW-7),browserH,TRUE);
        const int consoleY = bottomY + headerH, consoleH = (std::max)(0, actualBottom - headerH), scrollW = 8; MoveWindow(consoleEdit_, bottomCenterX+8,consoleY+6,(std::max)(0,bottomCenter-8-6-scrollW-3),(std::max)(0,consoleH-12),TRUE); MoveWindow(consoleScroll_, bottomCenterX+bottomCenter-scrollW-6,consoleY+7,scrollW,(std::max)(0,consoleH-14),TRUE);
        const int buttonY = bottomY + actualBottom - buttonH - 9, actionGap = 9, actionW = (std::max)(96,(rightW-24-actionGap)/2); MoveWindow(playButton_, bottomRightX+12,buttonY,actionW,buttonH,TRUE); MoveWindow(buildButton_, bottomRightX+12+actionW+actionGap,buttonY,actionW,buttonH,TRUE);
        MoveWindow(status_, 0,statusY,w,statusH,TRUE);
        LayoutInspectorControls_();
        InvalidateRect(viewport_.body,nullptr,FALSE);
        InvalidateRect(inspector_.body,nullptr,FALSE);
        InvalidateRect(buildPlay_.body,nullptr,FALSE);
        InvalidateRect(status_,nullptr,FALSE);
    }

    void EditorShellV3::AppendConsole_(const wchar_t* message)
    {
        if (!consoleEdit_ || !message) return; SendMessageW(consoleEdit_, EM_SETREADONLY, FALSE, 0); SYSTEMTIME st{}; GetLocalTime(&st); wchar_t ts[32]{}; swprintf_s(ts, L"[%02u:%02u:%02u] ", st.wHour,st.wMinute,st.wSecond);
        const auto& c = EditorTheme::Colors(); RichAppend(consoleEdit_, ts, c.textMuted); RichAppend(consoleEdit_, L"[Editor] ", c.accent); std::wstring msg(message), lower = msg; std::transform(lower.begin(),lower.end(),lower.begin(),::towlower); COLORREF color = c.textPrimary; if (lower.find(L"error") != std::wstring::npos || lower.find(L"failed") != std::wstring::npos) color = c.danger; else if (lower.find(L"warning") != std::wstring::npos) color = c.warning; RichAppend(consoleEdit_, msg + L"\r\n", color); SendMessageW(consoleEdit_, EM_SETREADONLY, TRUE, 0); SendMessageW(consoleEdit_, EM_SCROLLCARET, 0, 0);
        RECT rc{}; GetClientRect(consoleEdit_, &rc); const int lines = static_cast<int>(SendMessageW(consoleEdit_, EM_GETLINECOUNT, 0, 0)); const int first = static_cast<int>(SendMessageW(consoleEdit_, EM_GETFIRSTVISIBLELINE, 0, 0)); const int page = (std::max)(1, static_cast<int>(rc.bottom) / 15); SetScroll(consoleScroll_, lines, page, first);
    }

    bool EditorShellV3::RequestEditorExit_()
    {
        if (!window_)
            return false;

        if (session_ && session_->SceneDirty())
        {
            // Design choice (not directly from the book): Phase 16 has no
            // persistence yet, so exit confirmation is intentionally
            // discard-or-cancel. Save remains Phase 17.
            const int choice =
                MessageBoxW(
                    hwnd_,
                    L"The current in-memory scene has unsaved authoring changes.\n\nDiscard them and exit Nocturne Editor?",
                    L"Exit Nocturne Editor",
                    MB_OKCANCEL
                        | MB_ICONWARNING
                        | MB_DEFBUTTON2);

            if (choice != IDOK)
            {
                AppendConsole_(
                    L"Exit cancelled; current scene retained.");
                return false;
            }
        }

        window_->RequestQuit();
        return true;
    }

    bool EditorShellV3::ExecuteCreateEntity_(
        noc::EntityHandle parent)
    {
        return ExecuteCreatePreset_(
            EditorEntityCreateKind::Empty,
            "Entity",
            nullptr,
            noc::AABB{ noc::Vec3::Zero(), noc::Vec3::Zero() },
            parent);
    }

    bool EditorShellV3::ExecuteCreatePreset_(
        EditorEntityCreateKind kind,
        const char* name,
        const char* meshVirtualPath,
        const noc::AABB& localBounds,
        noc::EntityHandle parent)
    {
        if (!session_ || !engine_ || !name)
            return false;

        noc::World& world = engine_->GetWorld();
        if (parent.IsValid()
            && (!world.IsAlive(parent)
                || session_->IsToolOwned(parent)
                || !world.HasTransform(parent)))
        {
            AppendConsole_(
                L"Create Entity rejected: parent is stale, tool-owned, or has no Transform.");
            return false;
        }

        noc::ResourceHandle mesh{};
        if (meshVirtualPath && meshVirtualPath[0] != '\0')
        {
            const auto typed =
                engine_->Resources().RequestMesh(meshVirtualPath);
            if (!typed.IsValid())
            {
                AppendConsole_(
                    L"Create primitive failed: built-in mesh resource request was rejected.");
                return false;
            }
            mesh = typed.Untyped();
        }

        auto context = session_->CommandContext();
        try
        {
            auto command = std::make_unique<CreateEntityCommand>();
            auto* commandRaw = command.get();

            if (!command->InitPreset(
                    name,
                    kind,
                    parent,
                    mesh,
                    localBounds)
                || !session_->History().Execute(
                    context,
                    std::move(command)))
            {
                AppendConsole_(L"Create Entity preset failed.");
                return false;
            }

            const noc::EntityHandle created =
                commandRaw->CurrentEntity();
            if (!created.IsValid()
                || !session_->SetSelection(created))
            {
                AppendConsole_(
                    L"Create Entity preset succeeded but selection update failed.");
                return false;
            }

            session_->NotifyAuthoredMutation();
            PopulateScene_();
            RefreshInspector();
            UpdateStatus_();
            AppendConsole_(L"Entity preset created.");
            return true;
        }
        catch (const std::bad_alloc&)
        {
            AppendConsole_(
                L"Create Entity preset failed: allocation failure.");
            return false;
        }
    }

    bool EditorShellV3::ExecuteDeleteSelection_()
    {
        if (!session_)
            return false;

        const noc::EntityHandle selected =
            session_->SelectedEntity();
        if (!selected.IsValid())
            return false;

        auto context = session_->CommandContext();

        try
        {
            auto command =
                std::make_unique<DeleteEntityCommand>();

            if (!command->Init(context, selected)
                || !session_->History().Execute(
                    context,
                    std::move(command)))
            {
                AppendConsole_(L"Delete Entity failed.");
                return false;
            }

            session_->ClearSelection();
            session_->NotifyAuthoredMutation();
            PopulateScene_();
            RefreshInspector();
            UpdateStatus_();
            AppendConsole_(L"Entity subtree deleted.");
            return true;
        }
        catch (const std::bad_alloc&)
        {
            AppendConsole_(L"Delete Entity failed: allocation failure.");
            return false;
        }
    }

    bool EditorShellV3::ExecuteDuplicateSelection_()
    {
        if (!session_)
            return false;

        const noc::EntityHandle selected =
            session_->SelectedEntity();
        if (!selected.IsValid())
            return false;

        auto context = session_->CommandContext();

        try
        {
            auto command =
                std::make_unique<DuplicateEntityCommand>();
            auto* commandRaw = command.get();

            if (!command->Init(context, selected)
                || !session_->History().Execute(
                    context,
                    std::move(command)))
            {
                AppendConsole_(L"Duplicate Entity failed.");
                return false;
            }

            const noc::EntityHandle duplicate =
                commandRaw->CurrentRoot();

            if (!duplicate.IsValid()
                || !session_->SetSelection(duplicate))
            {
                AppendConsole_(
                    L"Duplicate Entity succeeded but selection update failed.");
                return false;
            }

            session_->NotifyAuthoredMutation();
            PopulateScene_();
            RefreshInspector();
            UpdateStatus_();
            AppendConsole_(L"Entity subtree duplicated.");
            return true;
        }
        catch (const std::bad_alloc&)
        {
            AppendConsole_(L"Duplicate Entity failed: allocation failure.");
            return false;
        }
    }

    bool EditorShellV3::ExecuteReparentEntity_(
        noc::EntityHandle child,
        noc::EntityHandle parent)
    {
        if (!session_
            || !engine_
            || !child.IsValid())
        {
            return false;
        }

        const noc::EntityHandle selected =
            session_->SelectedEntity();
        auto context =
            session_->CommandContext();

        try
        {
            auto command =
                std::make_unique<
                    ReparentEntityCommand>();

            if (!command->Init(
                    context,
                    child,
                    parent))
            {
                AppendConsole_(
                    L"Reparent rejected: invalid target, cycle, or non-representable preserve-world transform.");
                return false;
            }

            if (!session_->History().Execute(
                    context,
                    std::move(command)))
            {
                AppendConsole_(
                    L"Reparent failed; hierarchy unchanged.");
                return false;
            }
        }
        catch (const std::bad_alloc&)
        {
            AppendConsole_(
                L"Reparent failed: allocation failure.");
            return false;
        }

        session_->NotifyAuthoredMutation();

        if (selected.IsValid())
            (void)session_->SetSelection(selected);

        PopulateScene_();
        RefreshInspector();
        UpdateStatus_();
        AppendConsole_(
            parent.IsValid()
                ? L"Entity reparented; world pose preserved."
                : L"Entity unparented to scene root; world pose preserved.");
        return true;
    }

    void EditorShellV3::ShowHierarchyContextMenu_(
        POINT screenPoint)
    {
        if (!session_
            || !engine_
            || !sceneTree_)
        {
            return;
        }

        noc::World& world =
            engine_->GetWorld();

        const noc::EntityHandle selected =
            session_->SelectedEntity();

        const bool hasSelection =
            selected.IsValid()
            && world.IsAlive(selected)
            && !session_->IsToolOwned(selected);

        enum : UINT
        {
            ContextCreate = 1,
            ContextCreateChild,
            ContextRename,
            ContextDuplicate,
            ContextDelete,
            ContextAddComponent,
            ContextUnparent,
            ContextReparentBase = 1000
        };

        HMENU menu = CreatePopupMenu();
        if (!menu)
            return;

        const UINT selectionFlags =
            hasSelection ? MF_STRING : MF_STRING | MF_GRAYED;

        AppendMenuW(
            menu,
            MF_STRING,
            ContextCreate,
            L"Create Root Entity");
        AppendMenuW(
            menu,
            selectionFlags,
            ContextCreateChild,
            L"Create Child Entity");
        AppendMenuW(
            menu,
            MF_SEPARATOR,
            0,
            nullptr);
        AppendMenuW(
            menu,
            selectionFlags,
            ContextRename,
            L"Rename");
        AppendMenuW(
            menu,
            selectionFlags,
            ContextDuplicate,
            L"Duplicate");
        AppendMenuW(
            menu,
            selectionFlags,
            ContextDelete,
            L"Delete");
        AppendMenuW(
            menu,
            selectionFlags,
            ContextAddComponent,
            L"Add Component...");

        const noc::EntityHandle currentParent =
            hasSelection
                ? world.ParentOf(selected)
                : noc::EntityHandle::Invalid();

        AppendMenuW(
            menu,
            MF_SEPARATOR,
            0,
            nullptr);
        AppendMenuW(
            menu,
            hasSelection && currentParent.IsValid()
                ? MF_STRING
                : MF_STRING | MF_GRAYED,
            ContextUnparent,
            L"Unparent to Scene Root");

        HMENU reparentMenu = CreatePopupMenu();
        std::vector<noc::EntityHandle> reparentCandidates;

        if (reparentMenu && hasSelection)
        {
            try
            {
                reparentCandidates.reserve(
                    world.AliveCount());

                for (uint32_t i = 0;
                     i < world.EntityCapacity();
                     ++i)
                {
                    const noc::EntityHandle candidate =
                        world.EntityAtIndex(i);

                    if (!candidate.IsValid()
                        || candidate == selected
                        || candidate == currentParent
                        || !world.IsAlive(candidate)
                        || session_->IsToolOwned(candidate))
                    {
                        continue;
                    }

                    bool wouldCycle = false;
                    noc::EntityHandle ancestor =
                        candidate;

                    while (ancestor.IsValid())
                    {
                        if (ancestor == selected)
                        {
                            wouldCycle = true;
                            break;
                        }

                        ancestor =
                            world.ParentOf(ancestor);
                    }

                    if (wouldCycle)
                        continue;

                    reparentCandidates.push_back(
                        candidate);

                    std::wstring label;
                    const noc::NameComponent* name =
                        world.GetName(candidate);

                    if (name && name->value[0] != '\0')
                        label = Utf8ToWide_(name->value);

                    if (label.empty())
                    {
                        std::wstringstream ss;
                        ss << L"Entity "
                            << candidate.index
                            << L":"
                            << candidate.generation;
                        label = ss.str();
                    }

                    AppendMenuW(
                        reparentMenu,
                        MF_STRING,
                        ContextReparentBase
                            + static_cast<UINT>(
                                reparentCandidates.size() - 1),
                        label.c_str());

                    if (reparentCandidates.size()
                        >= 0xF000u)
                    {
                        break;
                    }
                }
            }
            catch (const std::bad_alloc&)
            {
                reparentCandidates.clear();
            }
        }

        if (reparentMenu)
        {
            if (reparentCandidates.empty())
            {
                AppendMenuW(
                    reparentMenu,
                    MF_STRING | MF_GRAYED,
                    0,
                    L"No valid parent targets");
            }

            AppendMenuW(
                menu,
                MF_POPUP
                    | (hasSelection
                        ? MF_ENABLED
                        : MF_GRAYED),
                reinterpret_cast<UINT_PTR>(
                    reparentMenu),
                L"Reparent To");
        }

        const int command =
            TrackPopupMenuEx(
                menu,
                TPM_RETURNCMD
                    | TPM_LEFTALIGN
                    | TPM_TOPALIGN,
                screenPoint.x,
                screenPoint.y,
                hwnd_,
                nullptr);

        DestroyMenu(menu);

        switch (command)
        {
        case ContextCreate:
            (void)ExecuteCreateEntity_();
            break;
        case ContextCreateChild:
            if (hasSelection)
                (void)ExecuteCreateEntity_(selected);
            break;
        case ContextRename:
            if (hasSelection)
                (void)BeginRenameSelection_();
            break;
        case ContextDuplicate:
            if (hasSelection)
                (void)ExecuteDuplicateSelection_();
            break;
        case ContextDelete:
            if (hasSelection)
                (void)ExecuteDeleteSelection_();
            break;
        case ContextAddComponent:
            if (hasSelection)
                ShowAddComponentPopup_();
            break;
        case ContextUnparent:
            if (hasSelection
                && currentParent.IsValid())
            {
                (void)ExecuteReparentEntity_(
                    selected,
                    noc::EntityHandle::Invalid());
            }
            break;
        default:
            if (command >=
                    static_cast<int>(
                        ContextReparentBase))
            {
                const size_t candidateIndex =
                    static_cast<size_t>(
                        command
                        - ContextReparentBase);

                if (candidateIndex
                    < reparentCandidates.size())
                {
                    (void)ExecuteReparentEntity_(
                        selected,
                        reparentCandidates[
                            candidateIndex]);
                }
            }
            break;
        }
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

    bool EditorShellV3::BeginRenameSelection_()
    {
        if (!engine_ || !session_ || !sceneTree_)
            return false;

        const noc::EntityHandle selected =
            session_->SelectedEntity();

        if (!selected.IsValid()
            || !engine_->GetWorld().IsAlive(selected)
            || !engine_->GetWorld().HasName(selected))
        {
            return false;
        }

        RECT row{};
        if (!TreeSelectedLabelRect(sceneTree_, row))
            return false;

        const noc::NameComponent* name =
            engine_->GetWorld().GetName(selected);
        if (!name)
            return false;

        if (!renameEdit_)
        {
            renameEdit_ = CreateWindowExW(
                WS_EX_CLIENTEDGE,
                L"EDIT",
                L"",
                WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
                row.left,
                row.top,
                row.right - row.left,
                row.bottom - row.top,
                sceneTree_,
                reinterpret_cast<HMENU>(
                    static_cast<INT_PTR>(IdSceneRenameEdit)),
                GetModuleHandleW(nullptr),
                nullptr);

            if (!renameEdit_)
                return false;

            SendMessageW(
                renameEdit_,
                WM_SETFONT,
                reinterpret_cast<WPARAM>(uiFont_),
                TRUE);

            if (!SetWindowSubclass(
                    renameEdit_,
                    &EditorShellV3::RenameEditSubclassProc_,
                    0x1610,
                    reinterpret_cast<DWORD_PTR>(this)))
            {
                DestroyWindow(renameEdit_);
                renameEdit_ = nullptr;
                return false;
            }
        }

        const std::wstring current =
            Utf8ToWide_(name->value);

        renameEntity_ = selected;
        renameEnding_ = false;

        MoveWindow(
            renameEdit_,
            row.left,
            row.top,
            row.right - row.left,
            row.bottom - row.top,
            TRUE);
        SetWindowTextW(
            renameEdit_,
            current.c_str());
        ShowWindow(renameEdit_, SW_SHOW);
        SetWindowPos(
            renameEdit_,
            HWND_TOP,
            0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE);
        SetFocus(renameEdit_);
        SendMessageW(
            renameEdit_,
            EM_SETSEL,
            0,
            -1);

        return true;
    }

    bool EditorShellV3::CommitRename_()
    {
        if (renameEnding_
            || !renameEdit_
            || !renameEntity_.IsValid()
            || !session_
            || !engine_)
        {
            return false;
        }

        renameEnding_ = true;

        const noc::EntityHandle entity =
            renameEntity_;
        const int length =
            GetWindowTextLengthW(renameEdit_);

        std::wstring wide;
        try
        {
            const size_t characterCount =
                static_cast<size_t>(
                    (std::max)(0, length));

            // Reserve explicit space for Win32's terminating NUL, then remove
            // it from the logical std::wstring length after the copy.
            wide.resize(characterCount + 1u);

            GetWindowTextW(
                renameEdit_,
                wide.data(),
                static_cast<int>(characterCount + 1u));

            wide.resize(characterCount);
        }
        catch (const std::bad_alloc&)
        {
            renameEnding_ = false;
            AppendConsole_(
                L"Rename failed: allocation failure.");
            CancelRename_();
            return false;
        }

        std::string utf8;
        if (!WideToUtf8_(
                wide.c_str(),
                utf8))
        {
            renameEnding_ = false;
            AppendConsole_(
                L"Rename failed: invalid Unicode input.");
            CancelRename_();
            return false;
        }

        const noc::NameComponent* current =
            engine_->GetWorld().GetName(entity);

        if (current
            && utf8 == current->value)
        {
            ShowWindow(renameEdit_, SW_HIDE);
            renameEntity_ =
                noc::EntityHandle::Invalid();
            renameEnding_ = false;
            SetFocus(sceneTree_);
            return true;
        }

        bool success = false;
        auto context = session_->CommandContext();

        try
        {
            auto command =
                std::make_unique<RenameEntityCommand>();

            success =
                command->Init(
                    context,
                    entity,
                    utf8.c_str())
                && session_->History().Execute(
                    context,
                    std::move(command));
        }
        catch (const std::bad_alloc&)
        {
            success = false;
        }

        ShowWindow(renameEdit_, SW_HIDE);
        renameEntity_ =
            noc::EntityHandle::Invalid();
        renameEnding_ = false;
        SetFocus(sceneTree_);

        if (!success)
        {
            AppendConsole_(
                L"Rename rejected; name unchanged.");
            return false;
        }

        session_->NotifyAuthoredMutation();
        PopulateScene_();
        (void)session_->SetSelection(entity);
        SyncSceneSelection();
        RefreshInspector();
        UpdateStatus_();
        AppendConsole_(L"Entity renamed.");
        return true;
    }

    void EditorShellV3::CancelRename_() noexcept
    {
        if (!renameEdit_)
            return;

        renameEnding_ = true;
        ShowWindow(renameEdit_, SW_HIDE);
        renameEntity_ =
            noc::EntityHandle::Invalid();
        renameEnding_ = false;

        if (sceneTree_)
            SetFocus(sceneTree_);
    }

    LRESULT CALLBACK EditorShellV3::RenameEditSubclassProc_(
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
        case WM_KEYDOWN:
            if (wParam == VK_RETURN)
            {
                (void)self->CommitRename_();
                return 0;
            }

            if (wParam == VK_ESCAPE)
            {
                self->CancelRename_();
                return 0;
            }
            break;

        case WM_KILLFOCUS:
            if (!self->renameEnding_
                && self->renameEntity_.IsValid())
            {
                (void)self->CommitRename_();
            }
            break;
        }

        return DefSubclassProc(
            hwnd,
            message,
            wParam,
            lParam);
    }

    bool EditorShellV3::HasTextInputFocus_() const
    {
        const HWND focus = GetFocus();
        if (!focus)
            return false;

        wchar_t className[64]{};
        const int length =
            GetClassNameW(
                focus,
                className,
                static_cast<int>(std::size(className)));
        if (length <= 0)
            return false;

        if (_wcsicmp(className, L"Edit") == 0)
            return true;

        return _wcsnicmp(
            className,
            L"RichEdit",
            8) == 0;
    }

    bool EditorShellV3::FilterMessage(const MSG& message)
    {
        if (message.message != WM_KEYDOWN
            && message.message != WM_SYSKEYDOWN)
        {
            return false;
        }

        if (HasTextInputFocus_())
            return false;

        const bool control =
            (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool shift =
            (GetKeyState(VK_SHIFT) & 0x8000) != 0;

        enum class ShortcutAction
        {
            None,
            Undo,
            Redo,
            Duplicate,
            Delete,
            Rename
        };

        ShortcutAction action = ShortcutAction::None;

        if (control && message.wParam == 'Z')
            action = shift
                ? ShortcutAction::Redo
                : ShortcutAction::Undo;
        else if (control && message.wParam == 'Y')
            action = ShortcutAction::Redo;
        else if (control && message.wParam == 'D')
            action = ShortcutAction::Duplicate;
        else if (!control && message.wParam == VK_DELETE)
            action = ShortcutAction::Delete;
        else if (!control && message.wParam == VK_F2)
            action = ShortcutAction::Rename;

        if (action == ShortcutAction::None)
            return false;

        // Consume keyboard auto-repeat for scene-authoring commands. One physical
        // press maps to one history operation.
        const bool wasDown =
            (static_cast<uint64_t>(message.lParam)
                & (1ull << 30u)) != 0;
        if (wasDown)
            return true;

        switch (action)
        {
        case ShortcutAction::Undo:
            HandleCommand_(IdToolbarUndo);
            break;
        case ShortcutAction::Redo:
            HandleCommand_(IdToolbarRedo);
            break;
        case ShortcutAction::Duplicate:
            (void)ExecuteDuplicateSelection_();
            break;
        case ShortcutAction::Delete:
            (void)ExecuteDeleteSelection_();
            break;
        case ShortcutAction::Rename:
            (void)BeginRenameSelection_();
            break;
        default:
            break;
        }

        return true;
    }

    void EditorShellV3::HandleCommand_(int id)
    {
        switch (id)
        {
        case IdToolbarNew:
            if (session_)
            {
                if (session_->SceneDirty())
                {
                    // Design choice (not directly from the book): because
                    // persistence is Phase 17, New Scene can only offer
                    // discard-or-cancel. It never pretends to save to disk.
                    const int choice =
                        MessageBoxW(
                            hwnd_,
                            L"The current in-memory scene has unsaved authoring changes.\n\nDiscard them and create a new empty scene?",
                            L"New Scene",
                            MB_OKCANCEL
                                | MB_ICONWARNING
                                | MB_DEFBUTTON2);

                    if (choice != IDOK)
                    {
                        AppendConsole_(
                            L"New Scene cancelled; current scene retained.");
                        break;
                    }
                }

                CancelRename_();

                if (!session_->ResetAuthoredScene())
                {
                    AppendConsole_(
                        L"New Scene failed while clearing authored entities.");
                    break;
                }

                inspectorScrollY_ = 0;
                PopulateScene_();
                RefreshInspector();
                UpdateStatus_();
                AppendConsole_(
                    L"New in-memory scene created. Tool camera retained; persistence remains Phase 17.");
            }
            break;
        case IdToolbarOpen: AppendConsole_(L"Open Scene requested. Scene serialization is scheduled for Phase 17."); break;
        case IdToolbarSave: AppendConsole_(L"Save requested. Serialization remains Phase 17 scope."); break;
        case IdToolbarUndo:
            if (session_ && session_->History().CanUndo())
            {
                auto context = session_->CommandContext();
                if (session_->History().Undo(context))
                {
                    session_->ValidateSelection();
                    const noc::EntityHandle hint =
                        session_->History().LastSelectionHint();
                    if (hint.IsValid())
                        (void)session_->SetSelection(hint);

                    session_->NotifyAuthoredMutation();
                    PopulateScene_();
                    RefreshInspector();
                    AppendConsole_(L"Undo applied.");
                }
                else AppendConsole_(L"Undo failed; history cursor preserved.");
            }
            break;
        case IdToolbarRedo:
            if (session_ && session_->History().CanRedo())
            {
                auto context = session_->CommandContext();
                if (session_->History().Redo(context))
                {
                    session_->ValidateSelection();
                    const noc::EntityHandle hint =
                        session_->History().LastSelectionHint();
                    if (hint.IsValid())
                        (void)session_->SetSelection(hint);

                    session_->NotifyAuthoredMutation();
                    PopulateScene_();
                    RefreshInspector();
                    AppendConsole_(L"Redo applied.");
                }
                else AppendConsole_(L"Redo failed; history cursor preserved.");
            }
            break;
        case IdToolbarSelect: case IdToolbarMove: case IdToolbarRotate: case IdToolbarScale:
            activeToolId_ = id;
            if (session_)
            {
                EditorTool tool = EditorTool::Select;
                if (id == IdToolbarMove) tool = EditorTool::Move;
                else if (id == IdToolbarRotate) tool = EditorTool::Rotate;
                else if (id == IdToolbarScale) tool = EditorTool::Scale;
                session_->SetActiveTool(tool);

                // Design choice (not directly from the book): scale authoring is
                // Local-only in Phase 16 because arbitrary world-scale edits can
                // require shear, which local TRS cannot represent.
                if (id == IdToolbarScale
                    && session_->Orientation()
                        == TransformOrientation::World)
                {
                    session_->SetTransformOrientation(
                        TransformOrientation::Local);
                    SetWindowTextW(
                        viewportOrientation_,
                        L"Local");
                    AppendConsole_(
                        L"Scale uses Local orientation; arbitrary World scale is not representable without shear.");
                }
            }
            for (HWND h : toolbarButtons_)
            {
                const int bid = GetDlgCtrlID(h);
                if (bid >= IdToolbarSelect && bid <= IdToolbarScale)
                    ButtonActive(h, bid == activeToolId_);
            }
            AppendConsole_(L"Viewport tool selected.");
            break;
        case IdToolbarPlay: case IdPlay: AppendConsole_(L"Play requested. Full Play-In-Editor remains Phase 27 scope."); break;
        case IdToolbarStop: AppendConsole_(L"Stop requested."); break;
        case IdToolbarBuild: case IdBuild:
            if (engine_) { AppendConsole_(L"Running asset import pass through the existing Phase 11 pipeline..."); const bool ok = engine_->Assets().ImportAll(); AppendConsole_(ok ? L"Asset import completed." : L"Asset import completed with errors. Check Log.txt."); PopulateContent_(); } break;
        case IdContentListMode: ButtonActive(contentListMode_, true); ButtonActive(contentGridMode_, false); AppendConsole_(L"Content Browser list view selected."); break;
        case IdContentGridMode: AppendConsole_(L"Grid view is a Phase 13 tooling-shell stub; list mode remains active."); break;
        case IdContentSettings: AppendConsole_(L"Content Browser settings shell selected."); break;
        case IdViewportPerspective: case IdViewportLit: case IdViewportShow:
            AppendConsole_(L"Viewport display control selected; rendering remains Phase 14 scope.");
            break;
        case IdViewportOrientation:
            if (session_)
            {
                if (activeToolId_ == IdToolbarScale)
                {
                    session_->SetTransformOrientation(
                        TransformOrientation::Local);
                    SetWindowTextW(
                        viewportOrientation_,
                        L"Local");
                    AppendConsole_(
                        L"World orientation is unavailable for Scale in Phase 16; Scale remains Local.");
                    break;
                }

                const TransformOrientation next =
                    session_->Orientation()
                        == TransformOrientation::Local
                            ? TransformOrientation::World
                            : TransformOrientation::Local;

                session_->SetTransformOrientation(next);
                SetWindowTextW(
                    viewportOrientation_,
                    next == TransformOrientation::World
                        ? L"World"
                        : L"Local");
                AppendConsole_(
                    next == TransformOrientation::World
                        ? L"Transform orientation: World."
                        : L"Transform orientation: Local.");
            }
            break;
        case IdActorCreate:
            (void)ExecuteCreateEntity_();
            break;
        case IdActorCreateStaticMesh:
            (void)ExecuteCreatePreset_(
                EditorEntityCreateKind::StaticMesh,
                "Static Mesh", nullptr,
                noc::AABB{ noc::Vec3::Zero(), noc::Vec3::Zero() });
            break;
        case IdActorCreateCamera:
            (void)ExecuteCreatePreset_(
                EditorEntityCreateKind::Camera,
                "Camera", nullptr,
                noc::AABB{ noc::Vec3::Zero(), noc::Vec3::Zero() });
            break;
        case IdActorCreateDirectionalLight:
            (void)ExecuteCreatePreset_(
                EditorEntityCreateKind::DirectionalLight,
                "Directional Light", nullptr,
                noc::AABB{ noc::Vec3::Zero(), noc::Vec3::Zero() });
            break;
        case IdActorCreatePointLight:
            (void)ExecuteCreatePreset_(
                EditorEntityCreateKind::PointLight,
                "Point Light", nullptr,
                noc::AABB{ noc::Vec3::Zero(), noc::Vec3::Zero() });
            break;
        case IdActorCreateSpotLight:
            (void)ExecuteCreatePreset_(
                EditorEntityCreateKind::SpotLight,
                "Spot Light", nullptr,
                noc::AABB{ noc::Vec3::Zero(), noc::Vec3::Zero() });
            break;
        case IdActorCreatePlane:
            (void)ExecuteCreatePreset_(
                EditorEntityCreateKind::StaticMesh,
                "Plane", "Meshes/Primitives/plane.nmsh",
                noc::AABB{ noc::Vec3{ -1.0f, 0.0f, -1.0f }, noc::Vec3{ 1.0f, 0.0f, 1.0f } });
            break;
        case IdActorCreateCube:
            (void)ExecuteCreatePreset_(
                EditorEntityCreateKind::StaticMesh,
                "Cube", "Meshes/Primitives/cube.nmsh",
                noc::AABB{ noc::Vec3{ -1.0f, -1.0f, -1.0f }, noc::Vec3{ 1.0f, 1.0f, 1.0f } });
            break;
        case IdActorCreateSphere:
            (void)ExecuteCreatePreset_(
                EditorEntityCreateKind::StaticMesh,
                "Sphere", "Meshes/Primitives/sphere.nmsh",
                noc::AABB{ noc::Vec3{ -1.0f, -1.0f, -1.0f }, noc::Vec3{ 1.0f, 1.0f, 1.0f } });
            break;
        case IdActorCreateCylinder:
            (void)ExecuteCreatePreset_(
                EditorEntityCreateKind::StaticMesh,
                "Cylinder", "Meshes/Primitives/cylinder.nmsh",
                noc::AABB{ noc::Vec3{ -1.0f, -1.0f, -1.0f }, noc::Vec3{ 1.0f, 1.0f, 1.0f } });
            break;
        case IdActorDuplicate:
            (void)ExecuteDuplicateSelection_();
            break;
        case IdActorDelete:
            (void)ExecuteDeleteSelection_();
            break;
        case IdActorRename:
            (void)BeginRenameSelection_();
            break;
        default: break;
        }
        PopulateScene_(); UpdateStatus_();
    }

    void EditorShellV3::ShowPopup_(int menuId, HWND anchor)
    {
        HMENU menu = nullptr;
        if (menuId == IdMenuFile) menu = fileMenu_;
        else if (menuId == IdMenuBuild) menu = buildMenu_;
        else if (menuId == IdMenuActor) menu = actorMenu_;
        bool temporary = false;
        if (!menu) { menu = CreatePopupMenu(); AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, L"Phase 13 tooling shell"); temporary = true; }
        RECT rc{}; GetWindowRect(anchor, &rc); const int command = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, rc.left,rc.bottom+2,hwnd_,nullptr); if (command) HandleCommand_(command); if (temporary) DestroyMenu(menu);
    }

    void EditorShellV3::UpdateStatus_() { if (status_) InvalidateRect(status_, nullptr, FALSE); }

    bool EditorShellV3::OnWindowMessage(void* hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam, intptr_t& result)
    {
        const HWND native = static_cast<HWND>(hwnd); const auto& c = EditorTheme::Colors();
        switch (msg)
        {
        case WM_SIZE: Layout_(LOWORD(lParam), HIWORD(lParam)); InvalidateRect(native, nullptr, TRUE); result = 0; return false;
        case WM_ERASEBKGND: { RECT rc{}; GetClientRect(native, &rc); FillRect(reinterpret_cast<HDC>(wParam), &rc, windowBrush_); result = 1; return true; }
        case WM_CTLCOLOREDIT:
        {
            const HWND edit =
                reinterpret_cast<HWND>(lParam);

            if (edit == contentSearch_
                || FindInspectorEdit_(edit) != nullptr)
            {
                HDC dc =
                    reinterpret_cast<HDC>(wParam);
                SetTextColor(dc, c.textPrimary);
                SetBkColor(dc, c.inputBg);
                result =
                    reinterpret_cast<intptr_t>(editBrush_);
                return true;
            }
            break;
        }
        case WM_CLOSE:
            (void)RequestEditorExit_();
            result = 0;
            return true;

        case WM_COMMAND:
        {
            const int id = LOWORD(wParam);
            const int notification = HIWORD(wParam);
            HWND source = reinterpret_cast<HWND>(lParam);

            if (id == IdInspectorAddComponent)
            {
                ShowAddComponentPopup_();
                result = 0;
                return true;
            }

            if (source)
            {
                InspectorEnumBinding* enumBinding =
                    FindInspectorEnumButton_(source);

                if (enumBinding)
                {
                    ShowInspectorEnumPopup_(
                        *enumBinding);
                    result = 0;
                    return true;
                }

                InspectorBoolBinding* boolBinding =
                    FindInspectorBoolButton_(source);

                if (boolBinding)
                {
                    ToggleInspectorBool_(
                        *boolBinding);
                    result = 0;
                    return true;
                }

                InspectorResourceBinding* resourceBinding =
                    FindInspectorResourceButton_(source);

                if (resourceBinding)
                {
                    ShowResourcePicker_(
                        *resourceBinding);
                    result = 0;
                    return true;
                }

                InspectorComponentActionBinding* removeBinding =
                    FindInspectorRemoveButton_(source);

                if (removeBinding)
                {
                    const noc::TypeId componentTypeId =
                        removeBinding->componentTypeId;
                    (void)ExecuteRemoveComponent_(
                        componentTypeId);
                    result = 0;
                    return true;
                }
            }

            if (id == IdSceneTree
                && notification == kTreeSelectionChanged
                && session_)
            {
                const noc::EntityHandle entity =
                    TreeSelectedEntity(sceneTree_);

                if (entity.IsValid())
                    (void)session_->SetSelection(entity);
                else
                    session_->ClearSelection();

                SyncSceneSelection();
                RefreshInspector();
                UpdateStatus_();
                result = 0;
                return true;
            }
            if (id >= IdMenuFile && id <= IdMenuHelp) { ShowPopup_(id, source); result = 0; return true; }
            if (id == IDCANCEL)
            {
                (void)RequestEditorExit_();
                result = 0;
                return true;
            }
            if ((id >= IdToolbarNew && id <= IdToolbarBuild)
                || (id >= IdActorCreate && id <= IdActorRename)
                || id == IdPlay || id == IdBuild
                || id == IdContentListMode || id == IdContentGridMode
                || id == IdContentSettings
                || id == IdViewportPerspective || id == IdViewportLit
                || id == IdViewportShow || id == IdViewportOrientation)
            {
                HandleCommand_(id);
                result = 0;
                return true;
            }
            break;
        }
        case WM_NOC_V3_TREE_REPARENT:
        {
            if (static_cast<int>(wParam) != IdSceneTree
                || !session_
                || !engine_)
            {
                break;
            }

            const auto* request =
                reinterpret_cast<const TreeReparentRequest*>(
                    lParam);
            if (!request
                || !request->child.IsValid())
            {
                result = 0;
                return true;
            }

            (void)ExecuteReparentEntity_(
                request->child,
                request->parent);

            result = 0;
            return true;
        }

        case WM_NOC_V3_TREE_CONTEXT:
        {
            if (static_cast<int>(wParam) != IdSceneTree)
                break;

            const auto* point =
                reinterpret_cast<const POINT*>(
                    lParam);
            if (!point)
            {
                result = 0;
                return true;
            }

            ShowHierarchyContextMenu_(
                *point);

            result = 0;
            return true;
        }

        case WM_NOC_V3_INSPECTOR_REFRESH:
            RefreshInspector();
            result = 0;
            return true;

        case WM_NOC_V3_SCROLL:
            if (static_cast<int>(wParam) == IdConsoleScroll && consoleEdit_) { const int target = static_cast<int>(lParam); const int current = static_cast<int>(SendMessageW(consoleEdit_, EM_GETFIRSTVISIBLELINE, 0, 0)); SendMessageW(consoleEdit_, EM_LINESCROLL, 0, target - current); result = 0; return true; } break;
        case WM_DRAWITEM:
        {
            auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam); if (!dis) break;
            if (dis->hwndItem == menuBand_ || dis->hwndItem == toolbarBand_) { Fill(dis->hDC, dis->rcItem, c.toolbarBg); Line(dis->hDC, 0, static_cast<int>(dis->rcItem.bottom)-1, static_cast<int>(dis->rcItem.right), static_cast<int>(dis->rcItem.bottom)-1, c.border); result = TRUE; return true; }
            if (dis->hwndItem == viewport_.body)
            {
                RECT rc = dis->rcItem; Fill(dis->hDC, rc, c.viewportBg); const int horizon = static_cast<int>(rc.top) + static_cast<int>(rc.bottom-rc.top) * 42 / 100; const COLORREF grid = Blend(c.viewportBg,c.border,38);
                for (int y = horizon; y < rc.bottom; y += 24) Line(dis->hDC, rc.left,y,rc.right,y,grid); const int cx = static_cast<int>((rc.left+rc.right)/2); for (int i=-10;i<=10;++i) Line(dis->hDC,cx,horizon,cx+i*56,rc.bottom,grid);
                RECT brand{rc.left,horizon-34,rc.right,horizon+18}; DrawTextUi(dis->hDC,L"N",brand,Blend(c.viewportBg,c.textMuted,34),brandFont_,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
                RECT title{rc.left,horizon+18,rc.right,horizon+42}; DrawTextUi(dis->hDC,L"NOCTURNE VIEWPORT",title,c.textPrimary,uiBold_,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
                RECT sub{rc.left+40,horizon+40,rc.right-40,horizon+62}; DrawTextUi(dis->hDC,L"Phase 14 — rendering viewport, camera navigation, selection and gizmos",sub,c.textMuted,smallFont_,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
                const int ox=static_cast<int>(rc.left)+34,oy=static_cast<int>(rc.bottom)-32; Line(dis->hDC,ox,oy,ox+28,oy,RGB(230,73,78),2); Line(dis->hDC,ox,oy,ox,oy-28,RGB(94,211,111),2);
                RECT badge{rc.right-112,rc.bottom-38,rc.right-14,rc.bottom-10}; RoundBox(dis->hDC,badge,Blend(c.viewportBg,c.panelBgAlt,60),c.border,6); DrawTextUi(dis->hDC,L"Grid: 1.0 m",badge,c.textMuted,smallFont_,DT_CENTER|DT_VCENTER|DT_SINGLELINE); result=TRUE; return true;
            }
            if (dis->hwndItem == inspector_.body)
            {
                RECT rc = dis->rcItem;
                Fill(dis->hDC, rc, c.panelBg);

                const noc::EntityHandle selected =
                    inspectorModel_.Entity();

                if (!selected.IsValid())
                {
                    RECT title{
                        rc.left + 18,
                        rc.top + 54,
                        rc.right - 18,
                        rc.top + 78
                    };
                    DrawTextUi(
                        dis->hDC,
                        L"No object selected",
                        title,
                        c.textPrimary,
                        uiBold_,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                    RECT helper{
                        rc.left + 20,
                        rc.top + 84,
                        rc.right - 20,
                        rc.top + 124
                    };
                    DrawTextUi(
                        dis->hDC,
                        L"Select an authored entity to inspect reflected components.",
                        helper,
                        c.textMuted,
                        smallFont_,
                        DT_CENTER | DT_WORDBREAK);

                    result = TRUE;
                    return true;
                }

                std::wstring entityTitle;
                if (engine_)
                {
                    const noc::NameComponent* name =
                        engine_->GetWorld().GetName(selected);
                    if (name && name->value[0] != '\0')
                        entityTitle = Utf8ToWide_(name->value);
                }

                if (entityTitle.empty())
                {
                    std::wstringstream ss;
                    ss << L"Entity "
                        << selected.index
                        << L":"
                        << selected.generation;
                    entityTitle = ss.str();
                }

                RECT entityRc{
                    rc.left + 14,
                    rc.top + 10,
                    rc.right - 14,
                    rc.top + 36
                };
                DrawTextUi(
                    dis->hDC,
                    entityTitle.c_str(),
                    entityRc,
                    c.textPrimary,
                    uiBold_,
                    DT_LEFT | DT_VCENTER
                        | DT_SINGLELINE | DT_END_ELLIPSIS);

                int y =
                    rc.top + 43 - inspectorScrollY_;
                const int inspectorWidth =
                    static_cast<int>(rc.right - rc.left);
                const int labelWidth =
                    (std::max)(95, inspectorWidth * 42 / 100);

                const int listDcState =
                    SaveDC(dis->hDC);
                IntersectClipRect(
                    dis->hDC,
                    rc.left,
                    rc.top + 40,
                    rc.right,
                    (std::max)(
                        rc.top + 41,
                        rc.bottom - 40));

                for (const InspectorComponentView& component :
                     inspectorModel_.Components())
                {
                    if (y >= rc.bottom - 44)
                        break;

                    RECT componentRc{
                        rc.left + 8,
                        y,
                        rc.right - 8,
                        y + 26
                    };
                    RoundBox(
                        dis->hDC,
                        componentRc,
                        c.panelBgAlt,
                        c.border,
                        4);

                    const std::wstring componentName =
                        Utf8ToWide_(
                            component.displayName.c_str());
                    RECT componentText{
                        componentRc.left + 8,
                        componentRc.top,
                        componentRc.right
                            - (component.removable ? 70 : 8),
                        componentRc.bottom
                    };
                    DrawTextUi(
                        dis->hDC,
                        componentName.c_str(),
                        componentText,
                        c.textPrimary,
                        uiBold_,
                        DT_LEFT | DT_VCENTER
                            | DT_SINGLELINE | DT_END_ELLIPSIS);

                    y += 31;

                    for (const InspectorPropertyView& property :
                         component.properties)
                    {
                        if (y >= rc.bottom - 44)
                            break;

                        const wchar_t* presentationLabel =
                            InspectorPropertyPresentationLabel(
                                component.typeId,
                                property);

                        const std::wstring propertyName =
                            presentationLabel
                                ? std::wstring(presentationLabel)
                                : Utf8ToWide_(
                                    property.displayName.c_str());

                        const std::wstring propertyValue =
                            Utf8ToWide_(
                                property.displayValue.c_str());

                        RECT labelRc{
                            rc.left + 14,
                            y,
                            rc.left + labelWidth,
                            y + 24
                        };
                        RECT valueRc{
                            rc.left + labelWidth + 4,
                            y + 1,
                            rc.right - 12,
                            y + 23
                        };

                        if (InspectorPresentationFor(
                                component.typeId,
                                property)
                            != InspectorEditPresentation::AabbMinMaxAxes)
                        {
                            DrawTextUi(
                                dis->hDC,
                                propertyName.c_str(),
                                labelRc,
                                c.textMuted,
                                smallFont_,
                                DT_LEFT | DT_VCENTER
                                    | DT_SINGLELINE | DT_END_ELLIPSIS);
                        }

                        const bool resourcePicker =
                            IsResourcePickerProperty(
                                property);
                        const bool boolToggle =
                            IsBoolToggleProperty(
                                property);
                        const bool enumPicker =
                            IsEnumPickerProperty(
                                engine_
                                    ? &engine_->Reflection()
                                    : nullptr,
                                property);

                        const InspectorEditPresentation presentation =
                            InspectorPresentationFor(
                                component.typeId,
                                property);

                        const uint32_t controlCount =
                            property.editable
                                ? InspectorEditControlCount(
                                    component.typeId,
                                    property)
                                : 1u;

                        if (resourcePicker
                            || boolToggle
                            || enumPicker)
                        {
                            // The child button owns the value field chrome.
                        }
                        else if (presentation
                            == InspectorEditPresentation::AabbMinMaxAxes)
                        {
                            constexpr int kAxisGap = 4;
                            constexpr int kAxisLabelWidth = 11;
                            const int width =
                                static_cast<int>(
                                    valueRc.right - valueRc.left);
                            const int segmentWidth =
                                (std::max)(
                                    24,
                                    (width - kAxisGap * 2) / 3);
                            constexpr const wchar_t* kAxisNames[] = {
                                L"X", L"Y", L"Z"
                            };
                            constexpr const wchar_t* kRowNames[] = {
                                L"Min", L"Max"
                            };

                            for (uint32_t row = 0;
                                 row < 2;
                                 ++row)
                            {
                                const int rowY =
                                    y + static_cast<int>(row) * 27;

                                RECT rowLabel{
                                    rc.left + 14,
                                    rowY,
                                    rc.left + labelWidth,
                                    rowY + 24
                                };

                                std::wstring nestedLabel =
                                    propertyName;
                                nestedLabel += L" ";
                                nestedLabel += kRowNames[row];

                                DrawTextUi(
                                    dis->hDC,
                                    nestedLabel.c_str(),
                                    rowLabel,
                                    c.textMuted,
                                    smallFont_,
                                    DT_LEFT | DT_VCENTER
                                        | DT_SINGLELINE
                                        | DT_END_ELLIPSIS);

                                for (uint32_t axis = 0;
                                     axis < 3;
                                     ++axis)
                                {
                                    const int segmentLeft =
                                        valueRc.left
                                        + static_cast<int>(axis)
                                            * (segmentWidth + kAxisGap);
                                    const int segmentRight =
                                        axis == 2
                                            ? valueRc.right
                                            : segmentLeft + segmentWidth;

                                    RECT axisRc{
                                        segmentLeft,
                                        rowY,
                                        segmentLeft + kAxisLabelWidth,
                                        rowY + 24
                                    };
                                    DrawTextUi(
                                        dis->hDC,
                                        kAxisNames[axis],
                                        axisRc,
                                        c.textMuted,
                                        smallFont_,
                                        DT_CENTER | DT_VCENTER
                                            | DT_SINGLELINE);

                                    RECT fieldRc{
                                        segmentLeft + kAxisLabelWidth,
                                        rowY + 1,
                                        segmentRight,
                                        rowY + 23
                                    };
                                    RoundBox(
                                        dis->hDC,
                                        fieldRc,
                                        c.inputBg,
                                        c.border,
                                        3);
                                }
                            }
                        }
                        else if (controlCount > 1)
                        {
                            constexpr int kAxisGap = 4;
                            constexpr int kAxisLabelWidth = 11;
                            const int width =
                                static_cast<int>(
                                    valueRc.right - valueRc.left);
                            const int segmentWidth =
                                (std::max)(
                                    24,
                                    (width
                                        - kAxisGap
                                            * static_cast<int>(
                                                controlCount - 1))
                                        / static_cast<int>(
                                            controlCount));
                            constexpr const wchar_t* kAxisNames[] = {
                                L"X", L"Y", L"Z", L"W"
                            };

                            for (uint32_t axis = 0;
                                 axis < controlCount;
                                 ++axis)
                            {
                                const int segmentLeft =
                                    valueRc.left
                                    + static_cast<int>(axis)
                                        * (segmentWidth + kAxisGap);
                                const int segmentRight =
                                    axis + 1 == controlCount
                                        ? valueRc.right
                                        : segmentLeft + segmentWidth;

                                RECT axisRc{
                                    segmentLeft,
                                    y,
                                    segmentLeft + kAxisLabelWidth,
                                    y + 24
                                };
                                DrawTextUi(
                                    dis->hDC,
                                    kAxisNames[axis],
                                    axisRc,
                                    c.textMuted,
                                    smallFont_,
                                    DT_CENTER | DT_VCENTER
                                        | DT_SINGLELINE);

                                RECT fieldRc{
                                    segmentLeft + kAxisLabelWidth,
                                    y + 1,
                                    segmentRight,
                                    y + 23
                                };
                                RoundBox(
                                    dis->hDC,
                                    fieldRc,
                                    c.inputBg,
                                    c.border,
                                    3);
                            }
                        }
                        else
                        {
                            RoundBox(
                                dis->hDC,
                                valueRc,
                                c.inputBg,
                                property.editable
                                    ? c.border
                                    : Blend(
                                        c.border,
                                        c.panelBg,
                                        55),
                                3);

                            valueRc.left += 7;
                            valueRc.right -= 5;
                            if (!property.editable)
                            {
                                DrawTextUi(
                                    dis->hDC,
                                    propertyValue.c_str(),
                                    valueRc,
                                    c.textMuted,
                                    uiFont_,
                                    DT_LEFT | DT_VCENTER
                                        | DT_SINGLELINE
                                        | DT_END_ELLIPSIS);
                            }
                        }

                        y += 27 * static_cast<int>(
                            InspectorPropertyRowCount(
                                component.typeId,
                                property));
                    }

                    y += 7;
                }

                RestoreDC(
                    dis->hDC,
                    listDcState);

                RECT scrollTrack{
                    rc.right - 10,
                    rc.top + 42,
                    rc.right,
                    (std::max)(
                        rc.top + 43,
                        rc.bottom - 42)
                };
                DrawSlimThumb(
                    dis->hDC,
                    InspectorContentHeight_(),
                    (std::max)(
                        1,
                        static_cast<int>(
                            scrollTrack.bottom
                            - scrollTrack.top)),
                    inspectorScrollY_,
                    scrollTrack);

                result = TRUE;
                return true;
            }
            if (dis->hwndItem == content_.body || dis->hwndItem == console_.body) { Fill(dis->hDC,dis->rcItem,c.panelBg); result=TRUE; return true; }
            if (dis->hwndItem == buildPlay_.body)
            {
                RECT rc=dis->rcItem; Fill(dis->hDC,rc,c.panelBg); RECT heading{rc.left+15,rc.top+12,rc.right-15,rc.top+34}; DrawTextUi(dis->hDC,L"Play Options",heading,c.textPrimary,uiBold_,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
                struct Row{const wchar_t* label;const wchar_t* value;}rows[]={{L"Play Mode",L"Selected Viewport"},{L"Start Map",L"Current Scene"}}; int y=rc.top+43;
                for(const auto& row:rows){RECT label{rc.left+15,y,rc.left+104,y+29};DrawTextUi(dis->hDC,row.label,label,c.textMuted,smallFont_,DT_LEFT|DT_VCENTER|DT_SINGLELINE);RECT field{rc.left+112,y+1,rc.right-15,y+28};RoundBox(dis->hDC,field,c.inputBg,c.border,5);field.left+=9;DrawTextUi(dis->hDC,row.value,field,c.textPrimary,uiFont_,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);y+=38;}
                RECT vsync{rc.left+15,y,rc.left+104,y+29};DrawTextUi(dis->hDC,L"VSync",vsync,c.textMuted,smallFont_,DT_LEFT|DT_VCENTER|DT_SINGLELINE);RECT toggle{rc.left+112,y+5,rc.left+150,y+24};RoundBox(dis->hDC,toggle,c.accent,c.accent,10);HBRUSH knob=CreateSolidBrush(RGB(238,245,252));HGDIOBJ oldBrush=SelectObject(dis->hDC,knob);HGDIOBJ oldPen=SelectObject(dis->hDC,GetStockObject(NULL_PEN));Ellipse(dis->hDC,toggle.right-17,toggle.top+3,toggle.right-4,toggle.bottom-3);SelectObject(dis->hDC,oldPen);SelectObject(dis->hDC,oldBrush);DeleteObject(knob);RECT enabled{rc.left+160,y,rc.right-15,y+29};DrawTextUi(dis->hDC,L"Enabled",enabled,c.textPrimary,uiFont_,DT_LEFT|DT_VCENTER|DT_SINGLELINE);result=TRUE;return true;
            }
            if (dis->hwndItem == status_)
            {
                RECT rc = dis->rcItem;
                Fill(dis->hDC, rc, c.toolbarBg);
                Line(
                    dis->hDC,
                    0,
                    0,
                    static_cast<int>(rc.right),
                    0,
                    c.border);

                const bool dirty =
                    session_ && session_->SceneDirty();

                RECT stateRc{
                    16,
                    0,
                    112,
                    rc.bottom
                };
                DrawTextUi(
                    dis->hDC,
                    dirty ? L"● Modified" : L"● Ready",
                    stateRc,
                    dirty ? c.warning : c.success,
                    smallFont_,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                uint32_t authoredCount = 0;
                if (engine_)
                {
                    const noc::World& world =
                        engine_->GetWorld();

                    authoredCount =
                        world.AliveCount();

                    if (session_)
                    {
                        const noc::EntityHandle toolCamera =
                            session_->ToolCamera();

                        if (toolCamera.IsValid()
                            && world.IsAlive(toolCamera)
                            && authoredCount > 0)
                        {
                            --authoredCount;
                        }
                    }
                }

                std::wstringstream selectionText;
                selectionText << L"Sel: ";

                if (session_)
                {
                    const noc::EntityHandle selected =
                        session_->SelectedEntity();

                    if (selected.IsValid())
                    {
                        selectionText
                            << selected.index
                            << L":"
                            << selected.generation;
                    }
                    else
                    {
                        selectionText << L"None";
                    }
                }
                else
                {
                    selectionText << L"None";
                }

                RECT selectionRc{
                    120,
                    0,
                    270,
                    rc.bottom
                };
                const std::wstring selection =
                    selectionText.str();
                DrawTextUi(
                    dis->hDC,
                    selection.c_str(),
                    selectionRc,
                    c.textMuted,
                    smallFont_,
                    DT_LEFT | DT_VCENTER
                        | DT_SINGLELINE
                        | DT_END_ELLIPSIS);

                std::wstringstream historyText;
                historyText << L"History: ";

                if (session_)
                {
                    const EditorCommandHistory& history =
                        session_->History();

                    historyText
                        << history.Cursor()
                        << L"/"
                        << history.CommandCount()
                        << L"  "
                        << history.UsedBytes()
                        << L" B";
                }
                else
                {
                    historyText << L"0/0";
                }

                RECT historyRc{
                    278,
                    0,
                    470,
                    rc.bottom
                };
                const std::wstring history =
                    historyText.str();
                DrawTextUi(
                    dis->hDC,
                    history.c_str(),
                    historyRc,
                    c.textMuted,
                    smallFont_,
                    DT_LEFT | DT_VCENTER
                        | DT_SINGLELINE
                        | DT_END_ELLIPSIS);

                const wchar_t* toolName = L"Select";
                const wchar_t* orientationName = L"Local";

                if (session_)
                {
                    switch (session_->ActiveTool())
                    {
                    case EditorTool::Move:
                        toolName = L"Move";
                        break;
                    case EditorTool::Rotate:
                        toolName = L"Rotate";
                        break;
                    case EditorTool::Scale:
                        toolName = L"Scale";
                        break;
                    default:
                        break;
                    }

                    orientationName =
                        session_->Orientation()
                                == TransformOrientation::World
                            ? L"World"
                            : L"Local";
                }

                std::wstringstream toolText;
                toolText
                    << L"Tool: "
                    << toolName
                    << L" / "
                    << orientationName;

                RECT toolRc{
                    478,
                    0,
                    (std::max)(
                        480,
                        static_cast<int>(rc.right) - 360),
                    rc.bottom
                };
                const std::wstring tool =
                    toolText.str();
                DrawTextUi(
                    dis->hDC,
                    tool.c_str(),
                    toolRc,
                    c.textMuted,
                    smallFont_,
                    DT_LEFT | DT_VCENTER
                        | DT_SINGLELINE
                        | DT_END_ELLIPSIS);

                std::wstringstream entityText;
                entityText
                    << L"Authored: "
                    << authoredCount;

                const int entityLeft =
                    (std::max)(
                        0,
                        static_cast<int>(rc.right) - 350);
                RECT entityRc{
                    entityLeft,
                    0,
                    rc.right - 222,
                    rc.bottom
                };
                const std::wstring entities =
                    entityText.str();
                DrawTextUi(
                    dis->hDC,
                    entities.c_str(),
                    entityRc,
                    c.textMuted,
                    smallFont_,
                    DT_LEFT | DT_VCENTER
                        | DT_SINGLELINE
                        | DT_END_ELLIPSIS);

                RECT engineRc{
                    rc.right - 210,
                    0,
                    rc.right - 52,
                    rc.bottom
                };
                DrawTextUi(
                    dis->hDC,
                    L"Nocturne Engine",
                    engineRc,
                    c.textPrimary,
                    smallFont_,
                    DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

                RECT versionRc{
                    rc.right - 48,
                    0,
                    rc.right - 10,
                    rc.bottom
                };
                DrawTextUi(
                    dis->hDC,
                    L"v0.1.0",
                    versionRc,
                    c.textMuted,
                    smallFont_,
                    DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

                result = TRUE;
                return true;
            }
            break;
        }
        }
        return false;
    }

    std::wstring EditorShellV3::ResolveContentRoot_(const std::wstring& configured) const
    {
        namespace fs=std::filesystem; std::error_code ec; fs::path p(configured);
        if(p.is_absolute()&&fs::exists(p,ec))return p.wstring();
        if(fs::exists(p,ec))return fs::absolute(p,ec).wstring();
        wchar_t exe[MAX_PATH]{}; const DWORD n=GetModuleFileNameW(nullptr,exe,MAX_PATH); if(n==0)return configured;
        fs::path cursor=fs::path(exe).parent_path(); for(int i=0;i<7&&!cursor.empty();++i){fs::path candidate=cursor/p;if(fs::exists(candidate,ec))return candidate.wstring();cursor=cursor.parent_path();}
        return configured;
    }

    std::wstring EditorShellV3::Utf8ToWide_(const char* text)
    {
        if(!text||!*text)return{};
        const int n=MultiByteToWideChar(CP_UTF8,0,text,-1,nullptr,0);if(n<=1)return{};
        std::wstring out(static_cast<size_t>(n),L'\0');
        MultiByteToWideChar(CP_UTF8,0,text,-1,out.data(),n);
        out.resize(static_cast<size_t>(n-1));
        return out;
    }

    bool EditorShellV3::WideToUtf8_(
        const wchar_t* text,
        std::string& outText)
    {
        outText.clear();
        if (!text)
            return false;

        const int required =
            WideCharToMultiByte(
                CP_UTF8,
                WC_ERR_INVALID_CHARS,
                text,
                -1,
                nullptr,
                0,
                nullptr,
                nullptr);

        if (required <= 0)
            return false;

        try
        {
            outText.resize(
                static_cast<size_t>(required));
        }
        catch (const std::bad_alloc&)
        {
            return false;
        }

        if (WideCharToMultiByte(
                CP_UTF8,
                WC_ERR_INVALID_CHARS,
                text,
                -1,
                outText.data(),
                required,
                nullptr,
                nullptr) <= 0)
        {
            outText.clear();
            return false;
        }

        outText.resize(
            static_cast<size_t>(required - 1));
        return true;
    }
}
