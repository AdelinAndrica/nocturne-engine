#include "EditorShellV3.h"
#include "EditorShellV3Controls.h"
#include "EditorSession.h"

#include "Runtime/Engine.h"
#include "Runtime/World.h"

#include <algorithm>

namespace nocturne::editor
{
    using namespace shellv3;

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
}
