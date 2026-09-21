#include "EditorShellV3.h"
#include "EditorShellV3Controls.h"
#include "EditorSession.h"
#include "EditorCommands.h"

#include "Core/Log.h"
#include "Runtime/Engine.h"
#include "Runtime/World.h"
#include "Resources/Typed/MeshResource.h"

#include <algorithm>
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

}
