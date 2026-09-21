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

            if (HandleInspectorCommand_(
                    id,
                    notification,
                    source,
                    result))
            {
                return true;
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
            if (DrawInspector_(*dis, result))
                return true;
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
