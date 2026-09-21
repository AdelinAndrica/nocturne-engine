#include "EditorShellV3.h"
#include "EditorShellV3Controls.h"
#include "EditorSession.h"
#include "EditorTheme.h"

#include "Runtime/Engine.h"
#include "Core/Log.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include <CommCtrl.h>
#include <Richedit.h>

namespace nocturne::editor
{
    using namespace shellv3;

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
        auto makeBody = [&](int id = 0) { return CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | SS_OWNERDRAW, 0,0,0,0, hwnd_, id ? reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)) : nullptr, GetModuleHandleW(nullptr), nullptr); };
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
            inspector_.body,
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
}
