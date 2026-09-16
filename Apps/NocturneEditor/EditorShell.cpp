#include "EditorShell.h"
#include "EditorTheme.h"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <vector>

#include <dwmapi.h>

#include "Core/Log.h"
#include "Runtime/Engine.h"
#include "Runtime/World.h"

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Dwmapi.lib")

namespace nocturne::editor
{
    namespace
    {
        constexpr int kLeftWidth = 350;
        constexpr int kRightWidth = 370;
        constexpr int kBottomHeight = 310;

        void SetFont(HWND hwnd, HFONT font)
        {
            if (hwnd && font)
                SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        }

        HTREEITEM InsertTreeItem(HWND tree, HTREEITEM parent, const wchar_t* text)
        {
            TVINSERTSTRUCTW item{};
            item.hParent = parent;
            item.hInsertAfter = TVI_LAST;
            item.item.mask = TVIF_TEXT;
            item.item.pszText = const_cast<wchar_t*>(text);
            return TreeView_InsertItem(tree, &item);
        }

        void Fill(HDC dc, const RECT& rc, COLORREF color)
        {
            HBRUSH brush = CreateSolidBrush(color);
            FillRect(dc, &rc, brush);
            DeleteObject(brush);
        }

        void RoundBox(HDC dc, RECT rc, COLORREF fill, COLORREF border, int radius)
        {
            HBRUSH brush = CreateSolidBrush(fill);
            HPEN pen = CreatePen(PS_SOLID, 1, border);
            HGDIOBJ oldBrush = SelectObject(dc, brush);
            HGDIOBJ oldPen = SelectObject(dc, pen);
            RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
            SelectObject(dc, oldBrush);
            SelectObject(dc, oldPen);
            DeleteObject(brush);
            DeleteObject(pen);
        }

        void Text(HDC dc, const wchar_t* text, RECT rc, COLORREF color, HFONT font, UINT flags)
        {
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, color);
            HGDIOBJ oldFont = nullptr;
            if (font) oldFont = SelectObject(dc, font);
            DrawTextW(dc, text, -1, &rc, flags);
            if (oldFont) SelectObject(dc, oldFont);
        }

        std::wstring AssetTypeForPath(const std::filesystem::path& path, bool directory)
        {
            if (directory) return L"Folder";
            std::wstring ext = path.extension().wstring();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
            if (ext == L".obj" || ext == L".nmsh") return L"Mesh";
            if (ext == L".bmp" || ext == L".png" || ext == L".jpg" || ext == L".jpeg" || ext == L".ntx") return L"Texture";
            if (ext == L".txt") return L"Text";
            if (ext == L".json") return L"Metadata";
            if (ext == L".nmat") return L"Material";
            return L"Asset";
        }
    }

    bool EditorShell::Init(noc::Engine& engine, noc::WinWindow& window)
    {
        engine_ = &engine;
        window_ = &window;
        hwnd_ = static_cast<HWND>(window.Handle());
        if (!hwnd_) return false;

        INITCOMMONCONTROLSEX icc{};
        icc.dwSize = sizeof(icc);
        icc.dwICC = ICC_TREEVIEW_CLASSES | ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES;
        InitCommonControlsEx(&icc);

        constexpr DWORD kUseImmersiveDarkMode = 20;
        BOOL dark = TRUE;
        DwmSetWindowAttribute(hwnd_, kUseImmersiveDarkMode, &dark, sizeof(dark));

        uiFont_ = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Text");
        uiFontBold_ = CreateFontW(-16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Text");
        titleFont_ = CreateFontW(-17, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Text");
        consoleFont_ = CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Cascadia Mono");
        brandFont_ = CreateFontW(-56, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Display");

        const auto& c = EditorTheme::Colors();
        windowBrush_ = CreateSolidBrush(c.windowBg);
        panelBrush_ = CreateSolidBrush(c.panelBg);
        panelAltBrush_ = CreateSolidBrush(c.panelBgAlt);
        viewportBrush_ = CreateSolidBrush(c.viewportBg);
        consoleBrush_ = CreateSolidBrush(c.inputBg);
        inputBrush_ = CreateSolidBrush(c.inputBg);

        const auto& cfg = engine.Config();
        if (cfg.contentRoot && cfg.contentRoot[0] != '\0')
            contentRoot_ = Utf8ToWide_(cfg.contentRoot);

        CreateMenuBar_();
        CreateToolbar_();
        CreatePanels_();
        ApplyThemeToCommonControls_();
        PopulateSceneTree_();
        PopulateContentBrowser_();
        window.SetMessageSink(this);

        RECT rc{};
        GetClientRect(hwnd_, &rc);
        Layout_(rc.right - rc.left, rc.bottom - rc.top);

        AppendConsole_(L"Nocturne Editor initialized.");
        AppendConsole_(L"Phase 13: editor framework bootstrap active.");
        AppendConsole_(L"Modern editor theme enabled; viewport interaction remains Phase 14 scope.");
        UpdateStatus_();
        RedrawWindow(hwnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_ERASE);

        NOC_LOG_INFO("Editor", "Nocturne Editor shell initialized");
        return true;
    }

    void EditorShell::Shutdown()
    {
        if (window_) window_->SetMessageSink(nullptr);
        if (fileMenu_) DestroyMenu(fileMenu_);
        if (buildMenu_) DestroyMenu(buildMenu_);
        if (uiFont_) DeleteObject(uiFont_);
        if (uiFontBold_) DeleteObject(uiFontBold_);
        if (titleFont_) DeleteObject(titleFont_);
        if (consoleFont_) DeleteObject(consoleFont_);
        if (brandFont_) DeleteObject(brandFont_);
        if (windowBrush_) DeleteObject(windowBrush_);
        if (panelBrush_) DeleteObject(panelBrush_);
        if (panelAltBrush_) DeleteObject(panelAltBrush_);
        if (viewportBrush_) DeleteObject(viewportBrush_);
        if (consoleBrush_) DeleteObject(consoleBrush_);
        if (inputBrush_) DeleteObject(inputBrush_);
        fileMenu_ = nullptr;
        buildMenu_ = nullptr;
        uiFont_ = nullptr;
        uiFontBold_ = nullptr;
        titleFont_ = nullptr;
        consoleFont_ = nullptr;
        brandFont_ = nullptr;
        windowBrush_ = nullptr;
        panelBrush_ = nullptr;
        panelAltBrush_ = nullptr;
        viewportBrush_ = nullptr;
        consoleBrush_ = nullptr;
        inputBrush_ = nullptr;
        engine_ = nullptr;
        window_ = nullptr;
        hwnd_ = nullptr;
    }

    void EditorShell::CreateMenuBar_()
    {
        SetMenu(hwnd_, nullptr);
        menuBand_ = MakeOwnerStatic_(L"");
        const struct { const wchar_t* text; int id; } defs[] = {
            { L"File", IdMenuFile }, { L"Edit", IdMenuEdit }, { L"Window", IdMenuWindow },
            { L"Tools", IdMenuTools }, { L"Build", IdMenuBuild }, { L"Select", IdMenuSelect },
            { L"Actor", IdMenuActor }, { L"Help", IdMenuHelp }
        };
        for (const auto& def : defs) menuButtons_.push_back(MakeButton_(def.text, def.id));

        fileMenu_ = CreatePopupMenu();
        AppendMenuW(fileMenu_, MF_STRING, IdToolbarNew, L"New Scene");
        AppendMenuW(fileMenu_, MF_STRING, IdToolbarOpen, L"Open...");
        AppendMenuW(fileMenu_, MF_STRING, IdToolbarSave, L"Save");
        AppendMenuW(fileMenu_, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(fileMenu_, MF_STRING, IDCANCEL, L"Exit");

        buildMenu_ = CreatePopupMenu();
        AppendMenuW(buildMenu_, MF_STRING, IdToolbarBuild, L"Build Content");
        AppendMenuW(buildMenu_, MF_STRING, IdToolbarPlay, L"Play");
    }

    void EditorShell::CreateToolbar_()
    {
        toolbarBand_ = MakeOwnerStatic_(L"");
        const struct { const wchar_t* text; int id; } defs[] = {
            { L"＋  New", IdToolbarNew }, { L"▰  Open", IdToolbarOpen }, { L"▣  Save", IdToolbarSave },
            { L"↶  Undo", IdToolbarUndo }, { L"↷  Redo", IdToolbarRedo }, { L"➤  Select", IdToolbarSelect },
            { L"✥  Move", IdToolbarMove }, { L"⟳  Rotate", IdToolbarRotate }, { L"◲  Scale", IdToolbarScale },
            { L"▶  Play", IdToolbarPlay }, { L"■  Stop", IdToolbarStop }, { L"◆  Build", IdToolbarBuild }
        };
        for (const auto& def : defs) toolbarButtons_.push_back(MakeButton_(def.text, def.id));
    }

    void EditorShell::CreatePanels_()
    {
        scene_.title = MakeHeader_(L"▦  Scene Hierarchy");
        sceneTree_ = MakeTree_(IdSceneTree);
        scene_.body = sceneTree_;

        viewport_.title = MakeHeader_(L"▣  Viewport");
        viewport_.body = MakeOwnerStatic_(L"", IdInspector + 100);

        inspector_.title = MakeHeader_(L"☷  Inspector / Properties");
        inspectorText_ = MakeOwnerStatic_(L"", IdInspector);
        inspector_.body = inspectorText_;

        content_.title = MakeHeader_(L"▱  Content Browser");
        contentTree_ = MakeTree_(IdContentTree);
        contentList_ = MakeList_(IdContentList);
        contentSearch_ = MakeEdit_(ES_AUTOHSCROLL, IdContentSearch);
        content_.body = contentList_;

        console_.title = MakeHeader_(L">_  Console / Output");
        consoleEdit_ = MakeEdit_(ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL, IdConsole);
        SetFont(consoleEdit_, consoleFont_ ? consoleFont_ : uiFont_);
        console_.body = consoleEdit_;

        buildPlay_.title = MakeHeader_(L"▶  Build / Play");
        buildPlay_.body = MakeOwnerStatic_(L"", IdBuild + 100);
        playButton_ = MakeButton_(L"▶  Play (F5)", IdPlay);
        buildButton_ = MakeButton_(L"◆  Build", IdBuild);

        status_ = MakeOwnerStatic_(L"", IdStatus);
    }

    void EditorShell::ApplyThemeToCommonControls_()
    {
        const auto& c = EditorTheme::Colors();
        for (HWND tree : { sceneTree_, contentTree_ })
        {
            TreeView_SetBkColor(tree, c.panelBg);
            TreeView_SetTextColor(tree, c.textPrimary);
            TreeView_SetLineColor(tree, c.border);
            SetFont(tree, uiFont_);
        }
        ListView_SetBkColor(contentList_, c.panelBg);
        ListView_SetTextBkColor(contentList_, c.panelBg);
        ListView_SetTextColor(contentList_, c.textPrimary);
        ListView_SetExtendedListViewStyle(contentList_, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        SetFont(contentList_, uiFont_);
        SetWindowTextW(contentSearch_, L"");
#ifdef EM_SETCUEBANNER
        SendMessageW(contentSearch_, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Search Assets..."));
#endif
        SendMessageW(contentSearch_, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(10, 10));
        SetFont(contentSearch_, uiFont_);
    }

    void EditorShell::PopulateSceneTree_()
    {
        TreeView_DeleteAllItems(sceneTree_);
        HTREEITEM root = InsertTreeItem(sceneTree_, TVI_ROOT, L"Scene (Runtime World)");
        const uint32_t alive = engine_ ? engine_->GetWorld().AliveCount() : 0;
        std::wstringstream count;
        count << L"Runtime Objects: " << alive;
        InsertTreeItem(sceneTree_, root, count.str().c_str());
        InsertTreeItem(sceneTree_, root, L"Main Camera");
        InsertTreeItem(sceneTree_, root, L"Environment");
        TreeView_Expand(sceneTree_, root, TVE_EXPAND);
        TreeView_SelectItem(sceneTree_, root);
    }

    void EditorShell::PopulateContentBrowser_()
    {
        TreeView_DeleteAllItems(contentTree_);
        ListView_DeleteAllItems(contentList_);
        HTREEITEM root = InsertTreeItem(contentTree_, TVI_ROOT, L"Content");
        TreeView_Expand(contentTree_, root, TVE_EXPAND);
        TreeView_SelectItem(contentTree_, root);

        namespace fs = std::filesystem;
        std::error_code ec;
        const fs::path rootPath(contentRoot_);
        if (!fs::exists(rootPath, ec))
        {
            InsertTreeItem(contentTree_, root, L"<content root unavailable>");
            return;
        }

        std::vector<fs::directory_entry> entries;
        for (const auto& entry : fs::directory_iterator(rootPath, ec))
        {
            if (ec) break;
            entries.push_back(entry);
        }
        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b)
        {
            return a.path().filename().wstring() < b.path().filename().wstring();
        });

        int row = 0;
        for (const auto& entry : entries)
        {
            const std::wstring name = entry.path().filename().wstring();
            const bool isDir = entry.is_directory(ec);
            if (isDir) InsertTreeItem(contentTree_, root, name.c_str());
            LVITEMW item{};
            item.mask = LVIF_TEXT;
            item.iItem = row++;
            item.pszText = const_cast<wchar_t*>(name.c_str());
            ListView_InsertItem(contentList_, &item);
            const std::wstring type = AssetTypeForPath(entry.path(), isDir);
            ListView_SetItemText(contentList_, item.iItem, 1, const_cast<wchar_t*>(type.c_str()));
        }
    }

    void EditorShell::Layout_(int clientW, int clientH)
    {
        if (clientW <= 0 || clientH <= 0) return;
        const auto& m = EditorTheme::Metrics();

        MoveWindow(menuBand_, 0, 0, clientW, m.menuHeight, TRUE);
        MoveWindow(toolbarBand_, 0, m.menuHeight, clientW, m.toolbarHeight, TRUE);

        int menuX = 10;
        const int menuWidths[] = { 50, 50, 72, 58, 58, 62, 58, 52 };
        for (size_t i = 0; i < menuButtons_.size(); ++i)
        {
            MoveWindow(menuButtons_[i], menuX, 2, menuWidths[i], m.menuHeight - 4, TRUE);
            menuX += menuWidths[i] + 2;
        }

        int x = 12;
        const int toolbarY = m.menuHeight + 10;
        for (HWND button : toolbarButtons_)
        {
            const int id = GetDlgCtrlID(button);
            if (id == IdToolbarUndo || id == IdToolbarSelect || id == IdToolbarPlay) x += 10;
            MoveWindow(button, x, toolbarY, 88, m.buttonHeight, TRUE);
            x += 94;
        }

        const int top = m.menuHeight + m.toolbarHeight + m.gap;
        const int statusY = (std::max)(top, clientH - m.statusHeight);
        MoveWindow(status_, 0, statusY, clientW, m.statusHeight, TRUE);

        const int availableH = (std::max)(0, statusY - top - m.gap);
        const int desiredBottom = (std::min)(kBottomHeight, availableH / 2);
        const int topH = (std::max)(180, availableH - desiredBottom - m.gap);
        const int bottomY = top + topH + m.gap;
        const int bottomH = (std::max)(0, statusY - bottomY - m.gap);

        const int leftW = (std::min)(kLeftWidth, (std::max)(260, clientW / 4));
        const int rightW = (std::min)(kRightWidth, (std::max)(280, clientW / 4));
        const int centerX = leftW + 2 * m.gap;
        const int centerW = (std::max)(260, clientW - leftW - rightW - 4 * m.gap);
        const int rightX = centerX + centerW + m.gap;

        auto placePanel = [&](Panel panel, int px, int py, int pw, int ph)
        {
            MoveWindow(panel.title, px, py, (std::max)(0, pw), m.panelHeaderHeight, TRUE);
            MoveWindow(panel.body, px, py + m.panelHeaderHeight, (std::max)(0, pw),
                (std::max)(0, ph - m.panelHeaderHeight), TRUE);
        };

        placePanel(scene_, m.gap, top, leftW, topH);
        placePanel(viewport_, centerX, top, centerW, topH);
        placePanel(inspector_, rightX, top, rightW, topH);

        const int bottomLeftW = (std::min)(500, (std::max)(360, clientW / 3));
        const int bottomRightW = rightW;
        const int bottomCenterX = m.gap + bottomLeftW + m.gap;
        const int bottomCenterW = (std::max)(260, clientW - bottomLeftW - bottomRightW - 4 * m.gap);
        const int bottomRightX = bottomCenterX + bottomCenterW + m.gap;

        placePanel(content_, m.gap, bottomY, bottomLeftW, bottomH);
        placePanel(console_, bottomCenterX, bottomY, bottomCenterW, bottomH);
        placePanel(buildPlay_, bottomRightX, bottomY, bottomRightW, bottomH);

        const int contentTop = bottomY + m.panelHeaderHeight;
        const int contentInnerH = bottomH - m.panelHeaderHeight;
        MoveWindow(contentSearch_, m.gap + 10, contentTop + 10, bottomLeftW - 20, 32, TRUE);
        MoveWindow(contentTree_, m.gap + 10, contentTop + 50, 175, (std::max)(0, contentInnerH - 60), TRUE);
        MoveWindow(contentList_, m.gap + 193, contentTop + 50, bottomLeftW - 203,
            (std::max)(0, contentInnerH - 60), TRUE);

        MoveWindow(buildPlay_.body, bottomRightX, bottomY + m.panelHeaderHeight, bottomRightW,
            (std::max)(0, bottomH - m.panelHeaderHeight - 62), TRUE);
        MoveWindow(playButton_, bottomRightX + 12, bottomY + bottomH - 50,
            (bottomRightW - 36) / 2, 40, TRUE);
        MoveWindow(buildButton_, bottomRightX + 24 + (bottomRightW - 36) / 2,
            bottomY + bottomH - 50, (bottomRightW - 36) / 2, 40, TRUE);
    }

    void EditorShell::AppendConsole_(const wchar_t* text)
    {
        if (!consoleEdit_) return;
        SYSTEMTIME st{};
        GetLocalTime(&st);
        wchar_t time[32]{};
        swprintf_s(time, L"[%02u:%02u:%02u] [Editor] ", st.wHour, st.wMinute, st.wSecond);
        const int len = GetWindowTextLengthW(consoleEdit_);
        SendMessageW(consoleEdit_, EM_SETSEL, len, len);
        std::wstring line = time;
        line += text;
        line += L"\r\n";
        SendMessageW(consoleEdit_, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(line.c_str()));
        SendMessageW(consoleEdit_, EM_SCROLLCARET, 0, 0);
    }

    void EditorShell::UpdateStatus_()
    {
        if (status_) InvalidateRect(status_, nullptr, FALSE);
    }

    void EditorShell::HandleCommand_(int id)
    {
        switch (id)
        {
        case IdToolbarNew: AppendConsole_(L"New Scene requested. Scene authoring is scheduled for Phase 16."); break;
        case IdToolbarOpen: AppendConsole_(L"Open Scene requested. Scene serialization is scheduled for Phase 17."); break;
        case IdToolbarSave: AppendConsole_(L"Save requested. Serialization is intentionally deferred to Phase 17."); break;
        case IdToolbarUndo:
        case IdToolbarRedo: AppendConsole_(L"Undo/Redo received; edit history arrives with scene editing."); break;
        case IdToolbarSelect:
        case IdToolbarMove:
        case IdToolbarRotate:
        case IdToolbarScale:
            activeToolId_ = id;
            for (HWND button : toolbarButtons_) InvalidateRect(button, nullptr, FALSE);
            AppendConsole_(L"Viewport tool selected. Interactive gizmos are Phase 14 scope.");
            break;
        case IdToolbarPlay:
        case IdPlay: AppendConsole_(L"Play requested. Full Play-In-Editor bridge remains Phase 27 scope."); break;
        case IdToolbarStop: AppendConsole_(L"Stop requested."); break;
        case IdToolbarBuild:
        case IdBuild:
            if (engine_)
            {
                AppendConsole_(L"Running asset import pass through the existing Phase 11 pipeline...");
                const bool ok = engine_->Assets().ImportAll();
                AppendConsole_(ok ? L"Asset import completed." : L"Asset import completed with errors. Check Log.txt.");
                PopulateContentBrowser_();
            }
            break;
        case IDCANCEL:
            if (window_) window_->RequestQuit();
            break;
        default: break;
        }
        UpdateStatus_();
    }

    void EditorShell::ShowPopupMenu_(int menuId, HWND anchor)
    {
        HMENU menu = nullptr;
        if (menuId == IdMenuFile) menu = fileMenu_;
        if (menuId == IdMenuBuild) menu = buildMenu_;
        if (!menu)
        {
            AppendConsole_(L"Menu section is present for the Phase 13 shell; commands arrive with later editor phases.");
            return;
        }
        RECT rc{};
        GetWindowRect(anchor, &rc);
        const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
            rc.left, rc.bottom, 0, hwnd_, nullptr);
        if (command != 0) HandleCommand_(static_cast<int>(command));
    }

    bool EditorShell::OnWindowMessage(void*, uint32_t msg, uintptr_t wParam,
        intptr_t lParam, intptr_t& result)
    {
        switch (msg)
        {
        case WM_SIZE:
            Layout_(LOWORD(lParam), HIWORD(lParam));
            return false;
        case WM_ERASEBKGND:
        {
            RECT rc{};
            GetClientRect(hwnd_, &rc);
            FillRect(reinterpret_cast<HDC>(wParam), &rc, windowBrush_);
            result = 1;
            return true;
        }
        case WM_COMMAND:
        {
            const int id = LOWORD(wParam);
            if (id >= IdMenuFile && id <= IdMenuHelp)
            {
                ShowPopupMenu_(id, reinterpret_cast<HWND>(lParam));
                result = 0;
                return true;
            }
            if (id != IdContentSearch)
            {
                HandleCommand_(id);
                result = 0;
                return true;
            }
            return false;
        }
        case WM_KEYDOWN:
            if (wParam == VK_F5)
            {
                HandleCommand_(IdPlay);
                result = 0;
                return true;
            }
            return false;
        case WM_DRAWITEM:
            DrawOwnerControl_(reinterpret_cast<DRAWITEMSTRUCT*>(lParam));
            result = TRUE;
            return true;
        case WM_CTLCOLOREDIT:
        {
            HDC dc = reinterpret_cast<HDC>(wParam);
            HWND ctl = reinterpret_cast<HWND>(lParam);
            if (ctl == contentSearch_)
            {
                const auto& c = EditorTheme::Colors();
                SetTextColor(dc, c.textPrimary);
                SetBkColor(dc, c.inputBg);
                result = reinterpret_cast<intptr_t>(inputBrush_);
                return true;
            }
            return false;
        }
        case WM_CTLCOLORSTATIC:
        {
            HDC dc = reinterpret_cast<HDC>(wParam);
            HWND ctl = reinterpret_cast<HWND>(lParam);
            const auto& c = EditorTheme::Colors();
            if (ctl == consoleEdit_)
            {
                SetTextColor(dc, c.textPrimary);
                SetBkColor(dc, c.inputBg);
                result = reinterpret_cast<intptr_t>(consoleBrush_);
                return true;
            }
            SetTextColor(dc, c.textPrimary);
            SetBkColor(dc, c.panelBg);
            result = reinterpret_cast<intptr_t>(panelBrush_);
            return true;
        }
        case WM_NOTIFY:
        {
            auto* hdr = reinterpret_cast<NMHDR*>(lParam);
            const auto& c = EditorTheme::Colors();
            if ((hdr->hwndFrom == sceneTree_ || hdr->hwndFrom == contentTree_) && hdr->code == NM_CUSTOMDRAW)
            {
                auto* cd = reinterpret_cast<NMTVCUSTOMDRAW*>(lParam);
                if (cd->nmcd.dwDrawStage == CDDS_PREPAINT)
                {
                    result = CDRF_NOTIFYITEMDRAW;
                    return true;
                }
                if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
                {
                    const bool selected = (cd->nmcd.uItemState & CDIS_SELECTED) != 0;
                    cd->clrText = c.textPrimary;
                    cd->clrTextBk = selected ? RGB(14, 101, 183) : c.panelBg;
                    result = CDRF_NEWFONT;
                    return true;
                }
            }
            if (hdr->hwndFrom == contentList_ && hdr->code == NM_CUSTOMDRAW)
            {
                auto* cd = reinterpret_cast<NMLVCUSTOMDRAW*>(lParam);
                if (cd->nmcd.dwDrawStage == CDDS_PREPAINT)
                {
                    result = CDRF_NOTIFYITEMDRAW;
                    return true;
                }
                if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
                {
                    const bool selected = (cd->nmcd.uItemState & CDIS_SELECTED) != 0;
                    cd->clrText = c.textPrimary;
                    cd->clrTextBk = selected ? RGB(14, 101, 183) : c.panelBg;
                    result = CDRF_NEWFONT;
                    return true;
                }
            }
            return false;
        }
        default:
            return false;
        }
    }

    void EditorShell::DrawOwnerControl_(DRAWITEMSTRUCT* dis)
    {
        if (!dis) return;
        if (dis->hwndItem == menuBand_ || dis->hwndItem == toolbarBand_) { DrawBand_(dis); return; }
        if (dis->hwndItem == scene_.title || dis->hwndItem == viewport_.title ||
            dis->hwndItem == inspector_.title || dis->hwndItem == content_.title ||
            dis->hwndItem == console_.title || dis->hwndItem == buildPlay_.title)
        {
            DrawHeader_(dis);
            return;
        }
        if (dis->hwndItem == viewport_.body) { DrawViewport_(dis); return; }
        if (dis->hwndItem == inspector_.body) { DrawInspector_(dis); return; }
        if (dis->hwndItem == buildPlay_.body) { DrawBuildPlay_(dis); return; }
        if (dis->hwndItem == status_) { DrawStatus_(dis); return; }
        if (dis->CtlType == ODT_BUTTON) { DrawButton_(dis); return; }
        Fill(dis->hDC, dis->rcItem, EditorTheme::Colors().panelBg);
    }

    void EditorShell::DrawButton_(DRAWITEMSTRUCT* dis)
    {
        const auto& c = EditorTheme::Colors();
        const auto& m = EditorTheme::Metrics();
        const int id = static_cast<int>(dis->CtlID);
        const bool menu = id >= IdMenuFile && id <= IdMenuHelp;
        const bool active = id == activeToolId_;
        const bool pressed = (dis->itemState & ODS_SELECTED) != 0;
        const bool hot = (dis->itemState & ODS_HOTLIGHT) != 0;
        COLORREF fillColor = menu ? c.toolbarBg : c.buttonBg;
        COLORREF borderColor = menu ? c.toolbarBg : c.border;
        COLORREF textColor = c.textPrimary;
        if (active) { fillColor = RGB(13, 74, 128); borderColor = c.accent; }
        if (id == IdPlay) { fillColor = c.accent; borderColor = c.accentHover; }
        if (id == IdToolbarPlay) textColor = c.success;
        if (pressed || hot) fillColor = menu ? c.buttonHover : (active ? RGB(17, 88, 150) : c.buttonHover);
        if ((dis->itemState & ODS_DISABLED) != 0) textColor = c.textMuted;

        RECT rc = dis->rcItem;
        InflateRect(&rc, -1, -1);
        if (menu) Fill(dis->hDC, rc, fillColor);
        else RoundBox(dis->hDC, rc, fillColor, borderColor, m.buttonRadius);

        wchar_t label[128]{};
        GetWindowTextW(dis->hwndItem, label, static_cast<int>(std::size(label)));
        Text(dis->hDC, label, rc, textColor, uiFontBold_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    void EditorShell::DrawHeader_(DRAWITEMSTRUCT* dis)
    {
        const auto& c = EditorTheme::Colors();
        Fill(dis->hDC, dis->rcItem, c.panelBgAlt);
        RECT line = dis->rcItem;
        line.top = line.bottom - 1;
        Fill(dis->hDC, line, c.border);
        wchar_t title[128]{};
        GetWindowTextW(dis->hwndItem, title, static_cast<int>(std::size(title)));
        RECT textRc = dis->rcItem;
        textRc.left += 12;
        Text(dis->hDC, title, textRc, c.textPrimary, titleFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    void EditorShell::DrawBand_(DRAWITEMSTRUCT* dis)
    {
        const auto& c = EditorTheme::Colors();
        Fill(dis->hDC, dis->rcItem, c.toolbarBg);
        RECT line = dis->rcItem;
        line.top = line.bottom - 1;
        Fill(dis->hDC, line, c.border);
    }

    void EditorShell::DrawViewport_(DRAWITEMSTRUCT* dis)
    {
        const auto& c = EditorTheme::Colors();
        RECT rc = dis->rcItem;
        Fill(dis->hDC, rc, c.viewportBg);
        const int w = rc.right - rc.left;
        const int h = rc.bottom - rc.top;
        const int horizon = rc.top + h / 3;
        const int cx = rc.left + w / 2;

        HPEN gridPen = CreatePen(PS_SOLID, 1, RGB(28, 43, 58));
        HGDIOBJ oldPen = SelectObject(dis->hDC, gridPen);
        for (int i = -8; i <= 8; ++i)
        {
            MoveToEx(dis->hDC, cx, horizon, nullptr);
            LineTo(dis->hDC, cx + i * (w / 12), rc.bottom);
        }
        for (int i = 1; i <= 12; ++i)
        {
            const float t = static_cast<float>(i) / 12.0f;
            const int y = horizon + static_cast<int>((h - (h / 3)) * t * t);
            MoveToEx(dis->hDC, rc.left, y, nullptr);
            LineTo(dis->hDC, rc.right, y);
        }
        SelectObject(dis->hDC, oldPen);
        DeleteObject(gridPen);

        RECT pill{ rc.left + 16, rc.top + 14, rc.left + 120, rc.top + 46 };
        RoundBox(dis->hDC, pill, RGB(13, 61, 103), c.accent, 8);
        Text(dis->hDC, L"Perspective", pill, c.textPrimary, uiFontBold_, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        OffsetRect(&pill, 112, 0);
        pill.right = pill.left + 56;
        RoundBox(dis->hDC, pill, c.buttonBg, c.border, 8);
        Text(dis->hDC, L"Lit", pill, c.textPrimary, uiFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        OffsetRect(&pill, 64, 0);
        pill.right = pill.left + 70;
        RoundBox(dis->hDC, pill, c.buttonBg, c.border, 8);
        Text(dis->hDC, L"Show", pill, c.textPrimary, uiFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        RECT brandRc{ rc.left, rc.top + h / 2 - 100, rc.right, rc.top + h / 2 - 30 };
        Text(dis->hDC, L"N", brandRc, RGB(70, 91, 116), brandFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        RECT titleRc{ rc.left + 40, rc.top + h / 2 - 20, rc.right - 40, rc.top + h / 2 + 15 };
        Text(dis->hDC, L"NOCTURNE VIEWPORT", titleRc, c.textPrimary, titleFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        RECT subRc{ rc.left + 60, titleRc.bottom, rc.right - 60, titleRc.bottom + 52 };
        Text(dis->hDC, L"Phase 14 — rendering viewport, camera navigation, selection and gizmos",
            subRc, c.textMuted, uiFont_, DT_CENTER | DT_TOP | DT_WORDBREAK);

        const int ax = rc.left + 32;
        const int ay = rc.bottom - 36;
        HPEN xPen = CreatePen(PS_SOLID, 2, RGB(235, 73, 73));
        HPEN yPen = CreatePen(PS_SOLID, 2, RGB(78, 218, 102));
        HPEN zPen = CreatePen(PS_SOLID, 2, RGB(42, 139, 255));
        oldPen = SelectObject(dis->hDC, xPen);
        MoveToEx(dis->hDC, ax, ay, nullptr); LineTo(dis->hDC, ax + 34, ay);
        SelectObject(dis->hDC, yPen);
        MoveToEx(dis->hDC, ax, ay, nullptr); LineTo(dis->hDC, ax, ay - 34);
        SelectObject(dis->hDC, zPen);
        Ellipse(dis->hDC, ax - 3, ay - 3, ax + 4, ay + 4);
        SelectObject(dis->hDC, oldPen);
        DeleteObject(xPen); DeleteObject(yPen); DeleteObject(zPen);
        RECT xRc{ ax + 36, ay - 10, ax + 55, ay + 10 };
        Text(dis->hDC, L"X", xRc, RGB(235, 73, 73), uiFontBold_, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT yRc{ ax - 8, ay - 55, ax + 12, ay - 35 };
        Text(dis->hDC, L"Y", yRc, RGB(78, 218, 102), uiFontBold_, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        RECT zRc{ ax - 20, ay + 2, ax, ay + 22 };
        Text(dis->hDC, L"Z", zRc, RGB(42, 139, 255), uiFontBold_, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        RECT badge{ rc.right - 130, rc.bottom - 48, rc.right - 16, rc.bottom - 16 };
        RoundBox(dis->hDC, badge, c.buttonBg, c.border, 8);
        Text(dis->hDC, L"Grid: 1.0 m", badge, c.textPrimary, uiFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    void EditorShell::DrawInspector_(DRAWITEMSTRUCT* dis)
    {
        const auto& c = EditorTheme::Colors();
        RECT rc = dis->rcItem;
        Fill(dis->hDC, rc, c.panelBg);
        RECT title{ rc.left + 24, rc.top + 70, rc.right - 24, rc.top + 105 };
        Text(dis->hDC, L"No object selected", title, c.textPrimary, titleFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        RECT sub{ rc.left + 28, title.bottom + 8, rc.right - 28, title.bottom + 62 };
        Text(dis->hDC, L"Select an object in the scene to inspect its properties.", sub,
            c.textMuted, uiFont_, DT_CENTER | DT_TOP | DT_WORDBREAK);
        RECT divider{ rc.left + 20, sub.bottom + 20, rc.right - 20, sub.bottom + 21 };
        Fill(dis->hDC, divider, c.border);
        RECT tipsTitle{ rc.left + 24, divider.bottom + 18, rc.right - 24, divider.bottom + 46 };
        Text(dis->hDC, L"Tips", tipsTitle, c.textPrimary, uiFontBold_, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT tips{ rc.left + 30, tipsTitle.bottom + 8, rc.right - 24, rc.bottom - 20 };
        Text(dis->hDC,
            L"•  Select an object in Scene Hierarchy or click in the viewport.\r\n\r\n"
            L"•  Use Select / Move / Rotate / Scale from the toolbar.\r\n\r\n"
            L"•  Inspector components and editable properties arrive with scene editing.",
            tips, c.textMuted, uiFont_, DT_LEFT | DT_TOP | DT_WORDBREAK);
    }

    void EditorShell::DrawBuildPlay_(DRAWITEMSTRUCT* dis)
    {
        const auto& c = EditorTheme::Colors();
        RECT rc = dis->rcItem;
        Fill(dis->hDC, rc, c.panelBg);
        RECT heading{ rc.left + 16, rc.top + 12, rc.right - 16, rc.top + 40 };
        Text(dis->hDC, L"Play Options", heading, c.textPrimary, uiFontBold_, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        auto field = [&](int y, const wchar_t* label, const wchar_t* value)
        {
            RECT labelRc{ rc.left + 16, y, rc.left + 120, y + 32 };
            Text(dis->hDC, label, labelRc, c.textMuted, uiFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            RECT box{ rc.left + 130, y, rc.right - 16, y + 32 };
            RoundBox(dis->hDC, box, c.inputBg, c.border, 7);
            RECT valueRc = box; valueRc.left += 10; valueRc.right -= 10;
            Text(dis->hDC, value, valueRc, c.textPrimary, uiFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        };

        field(rc.top + 54, L"Play Mode", L"Selected Viewport");
        field(rc.top + 96, L"Start Map", L"Current Scene");
        RECT vsLabel{ rc.left + 16, rc.top + 142, rc.left + 120, rc.top + 174 };
        Text(dis->hDC, L"VSync", vsLabel, c.textMuted, uiFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT toggle{ rc.left + 132, rc.top + 147, rc.left + 178, rc.top + 171 };
        RoundBox(dis->hDC, toggle, c.accent, c.accent, 18);
        HBRUSH knob = CreateSolidBrush(RGB(245, 248, 252));
        HGDIOBJ oldBrush = SelectObject(dis->hDC, knob);
        HPEN nullPen = CreatePen(PS_NULL, 0, RGB(0, 0, 0));
        HGDIOBJ oldPen = SelectObject(dis->hDC, nullPen);
        Ellipse(dis->hDC, toggle.right - 21, toggle.top + 3, toggle.right - 3, toggle.bottom - 3);
        SelectObject(dis->hDC, oldBrush); SelectObject(dis->hDC, oldPen);
        DeleteObject(knob); DeleteObject(nullPen);
        RECT enabled{ toggle.right + 10, toggle.top - 2, rc.right - 16, toggle.bottom + 2 };
        Text(dis->hDC, L"Enabled", enabled, c.textPrimary, uiFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    void EditorShell::DrawStatus_(DRAWITEMSTRUCT* dis)
    {
        const auto& c = EditorTheme::Colors();
        RECT rc = dis->rcItem;
        Fill(dis->hDC, rc, c.panelBgAlt);
        RECT topLine = rc; topLine.bottom = topLine.top + 1;
        Fill(dis->hDC, topLine, c.border);
        const int w = rc.right - rc.left;
        const int cuts[] = { 0, static_cast<int>(w * 0.28f), static_cast<int>(w * 0.48f),
            static_cast<int>(w * 0.72f), static_cast<int>(w * 0.84f), w };
        for (int i = 1; i < 5; ++i)
        {
            RECT sep{ cuts[i], rc.top + 7, cuts[i] + 1, rc.bottom - 7 };
            Fill(dis->hDC, sep, c.border);
        }
        auto segment = [&](int index, const wchar_t* label, COLORREF color)
        {
            RECT textRc{ cuts[index] + 18, rc.top, cuts[index + 1] - 10, rc.bottom };
            Text(dis->hDC, label, textRc, color, uiFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        };
        segment(0, L"●  Ready", c.success);
        segment(1, L"✓  No Issues", c.success);
        segment(2, L"⌘  Branch: phase-13-editor-framework", c.textMuted);
        std::wstringstream objects;
        objects << L"◇  Objects: " << (engine_ ? engine_->GetWorld().AliveCount() : 0);
        const std::wstring objectText = objects.str();
        segment(3, objectText.c_str(), c.textMuted);
        segment(4, L"N  Nocturne Engine  v0.1.0", c.textPrimary);
    }

    HWND EditorShell::MakeOwnerStatic_(const wchar_t* text, int id)
    {
        HWND h = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
            0, 0, 0, 0, hwnd_, id ? reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)) : nullptr,
            GetModuleHandleW(nullptr), nullptr);
        SetFont(h, uiFont_);
        return h;
    }

    HWND EditorShell::MakeHeader_(const wchar_t* text)
    {
        return MakeOwnerStatic_(text);
    }

    HWND EditorShell::MakeButton_(const wchar_t* text, int id)
    {
        HWND h = CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            GetModuleHandleW(nullptr), nullptr);
        SetFont(h, uiFontBold_);
        return h;
    }

    HWND EditorShell::MakeEdit_(DWORD extraStyle, int id)
    {
        HWND h = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | extraStyle,
            0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            GetModuleHandleW(nullptr), nullptr);
        SetFont(h, uiFont_);
        return h;
    }

    HWND EditorShell::MakeTree_(int id)
    {
        HWND h = CreateWindowExW(0, WC_TREEVIEWW, L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | TVS_HASBUTTONS | TVS_HASLINES |
            TVS_LINESATROOT | TVS_SHOWSELALWAYS | TVS_FULLROWSELECT,
            0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            GetModuleHandleW(nullptr), nullptr);
        SetFont(h, uiFont_);
        return h;
    }

    HWND EditorShell::MakeList_(int id)
    {
        HWND h = CreateWindowExW(0, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            GetModuleHandleW(nullptr), nullptr);
        SetFont(h, uiFont_);
        LVCOLUMNW col{};
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.cx = 210;
        col.pszText = const_cast<wchar_t*>(L"Asset");
        ListView_InsertColumn(h, 0, &col);
        col.cx = 110;
        col.pszText = const_cast<wchar_t*>(L"Type");
        ListView_InsertColumn(h, 1, &col);
        return h;
    }

    std::wstring EditorShell::Utf8ToWide_(const char* text)
    {
        if (!text || !*text) return {};
        const int size = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
        if (size <= 1) return {};
        std::wstring out(static_cast<size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text, -1, out.data(), size);
        if (!out.empty() && out.back() == L'\0') out.pop_back();
        return out;
    }
}
