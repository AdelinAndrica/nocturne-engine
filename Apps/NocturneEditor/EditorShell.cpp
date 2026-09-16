#include "EditorShell.h"
#include "EditorTheme.h"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include <CommCtrl.h>
#include <dwmapi.h>
#include <Richedit.h>

#include "Core/Log.h"
#include "Runtime/Engine.h"
#include "Runtime/World.h"

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Dwmapi.lib")

namespace nocturne::editor
{
    namespace
    {
        void SetFont(HWND hwnd, HFONT font)
        {
            if (hwnd && font)
                SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        }

        COLORREF Blend(COLORREF a, COLORREF b, int bPercent)
        {
            const int aPercent = 100 - bPercent;
            return RGB(
                (GetRValue(a) * aPercent + GetRValue(b) * bPercent) / 100,
                (GetGValue(a) * aPercent + GetGValue(b) * bPercent) / 100,
                (GetBValue(a) * aPercent + GetBValue(b) * bPercent) / 100);
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

        HFONT CreateUiFont(int pixelHeight, int weight, const wchar_t* face)
        {
            return CreateFontW(-pixelHeight, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
        }

        void SetRichTextColor(HWND richEdit, COLORREF color)
        {
            CHARFORMAT2W format{};
            format.cbSize = sizeof(format);
            format.dwMask = CFM_COLOR;
            format.crTextColor = color;
            SendMessageW(richEdit, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&format));
        }

        void RichAppend(HWND richEdit, const std::wstring& text, COLORREF color)
        {
            const LRESULT len = SendMessageW(richEdit, WM_GETTEXTLENGTH, 0, 0);
            SendMessageW(richEdit, EM_SETSEL, len, len);
            SetRichTextColor(richEdit, color);
            SendMessageW(richEdit, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(text.c_str()));
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
        icc.dwICC = ICC_STANDARD_CLASSES;
        InitCommonControlsEx(&icc);

        if (!RegisterEditorControls(GetModuleHandleW(nullptr)))
            return false;

        // Design choice (not directly from the book): use native dark title-bar support
        // while all client-area tooling chrome is painted by Nocturne controls.
        constexpr DWORD kUseImmersiveDarkMode = 20;
        BOOL dark = TRUE;
        DwmSetWindowAttribute(hwnd_, kUseImmersiveDarkMode, &dark, sizeof(dark));

        // Fidelity pass: smaller typography, unchanged hit targets.
        uiFont_ = CreateUiFont(13, FW_NORMAL, L"Segoe UI Variable Text");
        mutedFont_ = CreateUiFont(12, FW_NORMAL, L"Segoe UI Variable Text");
        uiFontBold_ = CreateUiFont(13, FW_SEMIBOLD, L"Segoe UI Variable Text");
        titleFont_ = CreateUiFont(14, FW_SEMIBOLD, L"Segoe UI Variable Text");
        consoleFont_ = CreateUiFont(12, FW_NORMAL, L"Cascadia Mono");
        brandFont_ = CreateUiFont(46, FW_SEMIBOLD, L"Segoe UI Variable Display");

        const auto& c = EditorTheme::Colors();
        windowBrush_ = CreateSolidBrush(c.windowBg);
        panelBrush_ = CreateSolidBrush(c.panelBg);
        consoleBrush_ = CreateSolidBrush(c.inputBg);

        const auto& cfg = engine.Config();
        if (cfg.contentRoot && cfg.contentRoot[0] != '\0')
            contentRoot_ = Utf8ToWide_(cfg.contentRoot);

        CreateMenuBar_();
        CreateToolbar_();
        CreatePanels_();
        PopulateSceneTree_();
        PopulateContentBrowser_();

        window.SetMessageSink(this);

        RECT rc{};
        GetClientRect(hwnd_, &rc);
        Layout_(rc.right - rc.left, rc.bottom - rc.top);

        AppendConsole_(L"Nocturne Editor initialized.");
        AppendConsole_(L"Phase 13: UI Fidelity Pass 2 active.");
        AppendConsole_(L"Custom controls enabled; viewport rendering remains Phase 14 scope.");
        UpdateStatus_();
        NOC_LOG_INFO("Editor", "Nocturne Editor UI Fidelity Pass 2 initialized");
        return true;
    }

    void EditorShell::Shutdown()
    {
        if (window_) window_->SetMessageSink(nullptr);

        if (fileMenu_) DestroyMenu(fileMenu_);
        if (buildMenu_) DestroyMenu(buildMenu_);
        fileMenu_ = nullptr;
        buildMenu_ = nullptr;

        if (uiFont_) DeleteObject(uiFont_);
        if (mutedFont_) DeleteObject(mutedFont_);
        if (uiFontBold_) DeleteObject(uiFontBold_);
        if (titleFont_) DeleteObject(titleFont_);
        if (consoleFont_) DeleteObject(consoleFont_);
        if (brandFont_) DeleteObject(brandFont_);
        if (windowBrush_) DeleteObject(windowBrush_);
        if (panelBrush_) DeleteObject(panelBrush_);
        if (consoleBrush_) DeleteObject(consoleBrush_);

        uiFont_ = mutedFont_ = uiFontBold_ = titleFont_ = consoleFont_ = brandFont_ = nullptr;
        windowBrush_ = panelBrush_ = consoleBrush_ = nullptr;
        engine_ = nullptr;
        window_ = nullptr;
        hwnd_ = nullptr;
    }

    void EditorShell::CreateMenuBar_()
    {
        menuBand_ = MakeOwnerStatic_(L"");

        const struct MenuDef { const wchar_t* text; int id; } defs[] = {
            { L"File", IdMenuFile }, { L"Edit", IdMenuEdit }, { L"Window", IdMenuWindow },
            { L"Tools", IdMenuTools }, { L"Build", IdMenuBuild }, { L"Select", IdMenuSelect },
            { L"Actor", IdMenuActor }, { L"Help", IdMenuHelp }
        };

        for (const auto& def : defs)
        {
            HWND button = CreateEditorButton(hwnd_, def.id, def.text, EditorButtonKind::Menu);
            SetFont(button, uiFont_);
            menuButtons_.push_back(button);
        }

        fileMenu_ = CreatePopupMenu();
        AppendMenuW(fileMenu_, MF_STRING, IdToolbarNew, L"New Scene");
        AppendMenuW(fileMenu_, MF_STRING, IdToolbarOpen, L"Open Scene...");
        AppendMenuW(fileMenu_, MF_STRING, IdToolbarSave, L"Save Scene");
        AppendMenuW(fileMenu_, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(fileMenu_, MF_STRING, IDCANCEL, L"Exit");

        buildMenu_ = CreatePopupMenu();
        AppendMenuW(buildMenu_, MF_STRING, IdToolbarBuild, L"Build Content");
        AppendMenuW(buildMenu_, MF_STRING, IdToolbarPlay, L"Play");
    }

    void EditorShell::CreateToolbar_()
    {
        toolbarBand_ = MakeOwnerStatic_(L"");

        const struct ToolDef { const wchar_t* text; int id; EditorButtonKind kind; } defs[] = {
            { L"＋  New", IdToolbarNew, EditorButtonKind::Neutral },
            { L"Open", IdToolbarOpen, EditorButtonKind::Neutral },
            { L"Save", IdToolbarSave, EditorButtonKind::Neutral },
            { L"Undo", IdToolbarUndo, EditorButtonKind::Neutral },
            { L"Redo", IdToolbarRedo, EditorButtonKind::Neutral },
            { L"Select", IdToolbarSelect, EditorButtonKind::Tool },
            { L"Move", IdToolbarMove, EditorButtonKind::Tool },
            { L"Rotate", IdToolbarRotate, EditorButtonKind::Tool },
            { L"Scale", IdToolbarScale, EditorButtonKind::Tool },
            { L"▶  Play", IdToolbarPlay, EditorButtonKind::Success },
            { L"Stop", IdToolbarStop, EditorButtonKind::Neutral },
            { L"Build", IdToolbarBuild, EditorButtonKind::Neutral }
        };

        for (const auto& def : defs)
        {
            HWND button = CreateEditorButton(hwnd_, def.id, def.text, def.kind);
            SetFont(button, uiFontBold_);
            if (def.id == activeToolId_) SetEditorButtonActive(button, true);
            toolbarButtons_.push_back(button);
        }
    }

    void EditorShell::CreatePanels_()
    {
        scene_.title = MakeHeader_(L"Scene Hierarchy");
        sceneTree_ = CreateEditorTree(hwnd_, IdSceneTree);
        SetFont(sceneTree_, uiFont_);
        scene_.body = sceneTree_;

        viewport_.title = MakeHeader_(L"Viewport");
        viewport_.body = MakeOwnerStatic_(L"");
        viewportPerspective_ = CreateEditorButton(hwnd_, IdViewportPerspective, L"Perspective", EditorButtonKind::Tool);
        viewportLit_ = CreateEditorButton(hwnd_, IdViewportLit, L"Lit", EditorButtonKind::Neutral);
        viewportShow_ = CreateEditorButton(hwnd_, IdViewportShow, L"Show", EditorButtonKind::Neutral);
        SetFont(viewportPerspective_, uiFont_);
        SetFont(viewportLit_, uiFont_);
        SetFont(viewportShow_, uiFont_);
        SetEditorButtonActive(viewportPerspective_, true);

        inspector_.title = MakeHeader_(L"Inspector / Properties");
        inspector_.body = MakeOwnerStatic_(L"", IdInspector);

        content_.title = MakeHeader_(L"Content Browser");
        content_.body = MakeOwnerStatic_(L"");
        contentSearch_ = CreateEditorInput(hwnd_, IdContentSearch, L"Search Assets...");
        SetFont(contentSearch_, uiFont_);
        contentTree_ = CreateEditorTree(hwnd_, IdContentTree);
        SetFont(contentTree_, uiFont_);
        contentList_ = CreateEditorDataTable(hwnd_, IdContentList);
        SetFont(contentList_, uiFont_);

        console_.title = MakeHeader_(L"Console / Output");
        console_.body = MakeOwnerStatic_(L"");

        // Design choice (not directly from the book): RichEdit keeps log text selectable
        // and copyable while custom Nocturne scrolling removes native scrollbar chrome.
        LoadLibraryW(L"Msftedit.dll");
        consoleEdit_ = CreateWindowExW(0, MSFTEDIT_CLASS, L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL |
            ES_READONLY | ES_NOHIDESEL,
            0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(IdConsole),
            GetModuleHandleW(nullptr), nullptr);
        SetFont(consoleEdit_, consoleFont_);
        SendMessageW(consoleEdit_, EM_SETBKGNDCOLOR, 0, EditorTheme::Colors().inputBg);
        SendMessageW(consoleEdit_, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(8, 8));

        consoleScroll_ = CreateEditorScrollBar(hwnd_, IdConsoleScroll);
        AttachEditorConsoleScrollSync(consoleEdit_, consoleScroll_, 18);

        buildPlay_.title = MakeHeader_(L"Build / Play");
        buildPlay_.body = MakeOwnerStatic_(L"");
        playButton_ = CreateEditorButton(hwnd_, IdPlay, L"▶  Play (F5)", EditorButtonKind::Primary);
        buildButton_ = CreateEditorButton(hwnd_, IdBuild, L"Build", EditorButtonKind::Neutral);
        SetFont(playButton_, uiFontBold_);
        SetFont(buildButton_, uiFontBold_);

        status_ = MakeOwnerStatic_(L"", IdStatus);
    }

    void EditorShell::PopulateSceneTree_()
    {
        EditorTreeClear(sceneTree_);
        EditorTreeAddItem(sceneTree_, L"Scene (Runtime World)", 0, true, true);

        const uint32_t alive = engine_ ? engine_->GetWorld().AliveCount() : 0;
        std::wstringstream count;
        count << L"Runtime Objects: " << alive;
        EditorTreeAddItem(sceneTree_, count.str(), 1, false);
        EditorTreeAddItem(sceneTree_, L"Main Camera", 1, false);
        EditorTreeAddItem(sceneTree_, L"Environment", 1, false);
    }

    void EditorShell::PopulateContentBrowser_()
    {
        EditorTreeClear(contentTree_);
        EditorDataTableClear(contentList_);
        EditorTreeAddItem(contentTree_, L"Content", 0, true, true);

        namespace fs = std::filesystem;
        std::error_code ec;
        const fs::path rootPath(contentRoot_);
        if (!fs::exists(rootPath, ec))
        {
            EditorTreeAddItem(contentTree_, L"Content root unavailable", 1, false);
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

        for (const auto& entry : entries)
        {
            const std::wstring name = entry.path().filename().wstring();
            const bool isDir = entry.is_directory(ec);
            if (isDir) EditorTreeAddItem(contentTree_, name, 1, false);
            const std::wstring type = AssetTypeForPath(entry.path(), isDir);
            EditorDataTableAddRow(contentList_, name, type);
        }
    }

    void EditorShell::Layout_(int clientW, int clientH)
    {
        if (clientW <= 0 || clientH <= 0) return;
        const auto& m = EditorTheme::Metrics();

        MoveWindow(menuBand_, 0, 0, clientW, m.menuHeight, TRUE);
        int menuX = 12;
        const int menuWidths[] = { 42, 42, 62, 48, 48, 54, 48, 44 };
        for (size_t i = 0; i < menuButtons_.size(); ++i)
        {
            const int w = i < std::size(menuWidths) ? menuWidths[i] : 48;
            MoveWindow(menuButtons_[i], menuX, 2, w, m.menuHeight - 4, TRUE);
            menuX += w + 2;
        }

        MoveWindow(toolbarBand_, 0, m.menuHeight, clientW, m.toolbarHeight, TRUE);
        int x = 14;
        const int toolbarY = m.menuHeight + (m.toolbarHeight - m.buttonHeight) / 2;
        const int widths[] = { 84, 78, 76, 78, 78, 86, 78, 84, 78, 90, 76, 80 };
        for (size_t i = 0; i < toolbarButtons_.size(); ++i)
        {
            const int w = i < std::size(widths) ? widths[i] : 80;
            MoveWindow(toolbarButtons_[i], x, toolbarY, w, m.buttonHeight, TRUE);
            x += w + 6;
            if (i == 2 || i == 4 || i == 8) x += 10;
        }

        const int top = m.menuHeight + m.toolbarHeight + m.gap;
        const int statusY = (std::max)(top, clientH - m.statusHeight);
        const int availableH = (std::max)(0, statusY - top - m.gap);
        const int bottomH = (std::clamp)(availableH * 34 / 100, 215, 292);
        const int topH = (std::max)(140, availableH - bottomH - m.gap);
        const int bottomY = top + topH + m.gap;
        const int actualBottomH = (std::max)(0, statusY - bottomY);

        const int leftW = (std::clamp)(clientW * 21 / 100, 270, 350);
        const int rightW = (std::clamp)(clientW * 23 / 100, 300, 370);
        const int centerX = m.gap + leftW + m.gap;
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

        const int viewportBodyY = top + m.panelHeaderHeight;
        MoveWindow(viewportPerspective_, centerX + 16, viewportBodyY + 12, 94, 32, TRUE);
        MoveWindow(viewportLit_, centerX + 116, viewportBodyY + 12, 52, 32, TRUE);
        MoveWindow(viewportShow_, centerX + 174, viewportBodyY + 12, 62, 32, TRUE);

        const int bottomLeftW = (std::clamp)(clientW * 32 / 100, 360, 510);
        const int bottomRightW = rightW;
        const int bottomCenterX = m.gap + bottomLeftW + m.gap;
        const int bottomCenterW = (std::max)(260, clientW - bottomLeftW - bottomRightW - 4 * m.gap);
        const int bottomRightX = bottomCenterX + bottomCenterW + m.gap;

        placePanel(content_, m.gap, bottomY, bottomLeftW, actualBottomH);
        placePanel(console_, bottomCenterX, bottomY, bottomCenterW, actualBottomH);
        placePanel(buildPlay_, bottomRightX, bottomY, bottomRightW, actualBottomH);

        const int contentBodyY = bottomY + m.panelHeaderHeight;
        const int contentBodyH = (std::max)(0, actualBottomH - m.panelHeaderHeight);
        const int contentPad = m.innerPadding;
        const int searchH = 32;
        MoveWindow(contentSearch_, m.gap + contentPad, contentBodyY + contentPad,
            (std::max)(0, bottomLeftW - 2 * contentPad), searchH, TRUE);

        const int browserY = contentBodyY + contentPad + searchH + 8;
        const int browserH = (std::max)(0, contentBodyH - (2 * contentPad + searchH + 8));
        const int treeW = (std::clamp)(bottomLeftW * 35 / 100, 125, 170);
        MoveWindow(contentTree_, m.gap + contentPad, browserY, treeW, browserH, TRUE);
        MoveWindow(contentList_, m.gap + contentPad + treeW + 8, browserY,
            (std::max)(0, bottomLeftW - 2 * contentPad - treeW - 8), browserH, TRUE);

        const int consoleBodyY = bottomY + m.panelHeaderHeight;
        const int consoleBodyH = (std::max)(0, actualBottomH - m.panelHeaderHeight);
        const int scrollW = m.scrollbarWidth;
        MoveWindow(consoleEdit_, bottomCenterX + 8, consoleBodyY + 7,
            (std::max)(0, bottomCenterW - 8 - 7 - scrollW - 4),
            (std::max)(0, consoleBodyH - 14), TRUE);
        MoveWindow(consoleScroll_, bottomCenterX + bottomCenterW - scrollW - 7,
            consoleBodyY + 8, scrollW, (std::max)(0, consoleBodyH - 16), TRUE);
        SyncConsoleScroll_();

        const int buildBodyY = bottomY + m.panelHeaderHeight;
        const int buttonY = bottomY + actualBottomH - m.buttonHeight - 10;
        const int actionGap = 10;
        const int actionW = (std::max)(100, (bottomRightW - 2 * 12 - actionGap) / 2);
        MoveWindow(playButton_, bottomRightX + 12, buttonY, actionW, m.buttonHeight, TRUE);
        MoveWindow(buildButton_, bottomRightX + 12 + actionW + actionGap, buttonY,
            actionW, m.buttonHeight, TRUE);

        MoveWindow(status_, 0, statusY, clientW, m.statusHeight, TRUE);
        InvalidateRect(buildPlay_.body, nullptr, FALSE);
        InvalidateRect(viewport_.body, nullptr, FALSE);
        InvalidateRect(status_, nullptr, FALSE);
    }

    void EditorShell::AppendConsole_(const wchar_t* text)
    {
        if (!consoleEdit_ || !text) return;

        SendMessageW(consoleEdit_, EM_SETREADONLY, FALSE, 0);
        SYSTEMTIME st{};
        GetLocalTime(&st);
        wchar_t timestamp[32]{};
        swprintf_s(timestamp, L"[%02u:%02u:%02u] ", st.wHour, st.wMinute, st.wSecond);

        const auto& c = EditorTheme::Colors();
        RichAppend(consoleEdit_, timestamp, c.textMuted);
        RichAppend(consoleEdit_, L"[Editor] ", c.accent);

        std::wstring message(text);
        std::wstring lower = message;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
        COLORREF messageColor = c.textPrimary;
        if (lower.find(L"error") != std::wstring::npos || lower.find(L"failed") != std::wstring::npos)
            messageColor = c.danger;
        else if (lower.find(L"warning") != std::wstring::npos)
            messageColor = c.warning;

        RichAppend(consoleEdit_, message + L"\r\n", messageColor);
        SendMessageW(consoleEdit_, EM_SETREADONLY, TRUE, 0);
        SendMessageW(consoleEdit_, EM_SCROLLCARET, 0, 0);
        SyncConsoleScroll_();
    }

    void EditorShell::SyncConsoleScroll_()
    {
        if (consoleEdit_ && consoleScroll_)
            SyncEditorConsoleScroll(consoleEdit_, consoleScroll_, 18);
    }

    void EditorShell::UpdateStatus_()
    {
        if (status_) InvalidateRect(status_, nullptr, FALSE);
    }

    void EditorShell::HandleCommand_(int id)
    {
        switch (id)
        {
        case IdToolbarNew:
            AppendConsole_(L"New Scene requested. Scene authoring is scheduled for Phase 16.");
            break;
        case IdToolbarOpen:
            AppendConsole_(L"Open Scene requested. Scene serialization is scheduled for Phase 17.");
            break;
        case IdToolbarSave:
            AppendConsole_(L"Save requested. Serialization is intentionally deferred to Phase 17.");
            break;
        case IdToolbarUndo:
        case IdToolbarRedo:
            AppendConsole_(L"Undo/Redo received; edit history arrives with scene editing.");
            break;
        case IdToolbarSelect:
        case IdToolbarMove:
        case IdToolbarRotate:
        case IdToolbarScale:
            activeToolId_ = id;
            for (HWND button : toolbarButtons_)
            {
                const int buttonId = GetDlgCtrlID(button);
                if (buttonId >= IdToolbarSelect && buttonId <= IdToolbarScale)
                    SetEditorButtonActive(button, buttonId == activeToolId_);
            }
            AppendConsole_(L"Viewport tool selected. Interactive gizmos remain Phase 14 scope.");
            break;
        case IdToolbarPlay:
        case IdPlay:
            AppendConsole_(L"Play requested. Full Play-In-Editor bridge remains Phase 27 scope.");
            break;
        case IdToolbarStop:
            AppendConsole_(L"Stop requested.");
            break;
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
        case IdViewportPerspective:
        case IdViewportLit:
        case IdViewportShow:
            AppendConsole_(L"Viewport display control selected; rendering behavior remains Phase 14 scope.");
            break;
        default:
            break;
        }
        PopulateSceneTree_();
        UpdateStatus_();
    }

    void EditorShell::ShowPopupMenu_(int menuId, HWND anchor)
    {
        HMENU menu = nullptr;
        if (menuId == IdMenuFile) menu = fileMenu_;
        else if (menuId == IdMenuBuild) menu = buildMenu_;

        if (!menu)
        {
            menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, L"Phase 13 tooling shell");
        }

        RECT rc{};
        GetWindowRect(anchor, &rc);
        const int command = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
            rc.left, rc.bottom + 2, hwnd_, nullptr);
        if (command != 0) HandleCommand_(command);

        if (menu != fileMenu_ && menu != buildMenu_) DestroyMenu(menu);
    }

    bool EditorShell::OnWindowMessage(void* hwnd, uint32_t msg, uintptr_t wParam,
        intptr_t lParam, intptr_t& result)
    {
        const HWND native = static_cast<HWND>(hwnd);
        switch (msg)
        {
        case WM_SIZE:
            Layout_(LOWORD(lParam), HIWORD(lParam));
            result = 0;
            return false;

        case WM_ERASEBKGND:
        {
            RECT rc{};
            GetClientRect(native, &rc);
            FillRect(reinterpret_cast<HDC>(wParam), &rc, windowBrush_);
            result = 1;
            return true;
        }

        case WM_DRAWITEM:
            DrawOwnerControl_(reinterpret_cast<DRAWITEMSTRUCT*>(lParam));
            result = TRUE;
            return true;

        case WM_COMMAND:
        {
            const int id = LOWORD(wParam);
            HWND source = reinterpret_cast<HWND>(lParam);
            if (id >= IdMenuFile && id <= IdMenuHelp)
            {
                ShowPopupMenu_(id, source);
                result = 0;
                return true;
            }
            if (id == IDCANCEL)
            {
                if (window_) window_->RequestQuit();
                result = 0;
                return true;
            }
            if (id >= IdToolbarNew && id <= IdToolbarBuild || id == IdPlay || id == IdBuild ||
                id == IdViewportPerspective || id == IdViewportLit || id == IdViewportShow)
            {
                HandleCommand_(id);
                result = 0;
                return true;
            }
            break;
        }

        case WM_NOC_EDITOR_SCROLL:
            if (static_cast<int>(wParam) == IdConsoleScroll && consoleEdit_)
            {
                const int target = static_cast<int>(lParam);
                const int current = static_cast<int>(SendMessageW(consoleEdit_, EM_GETFIRSTVISIBLELINE, 0, 0));
                SendMessageW(consoleEdit_, EM_LINESCROLL, 0, target - current);
                SyncConsoleScroll_();
                result = 0;
                return true;
            }
            break;

        case WM_NOC_EDITOR_SCROLL_SYNC:
            SyncConsoleScroll_();
            result = 0;
            return true;

        case WM_CTLCOLOREDIT:
            if (reinterpret_cast<HWND>(lParam) == consoleEdit_)
            {
                HDC dc = reinterpret_cast<HDC>(wParam);
                const auto& c = EditorTheme::Colors();
                SetTextColor(dc, c.textPrimary);
                SetBkColor(dc, c.inputBg);
                result = reinterpret_cast<intptr_t>(consoleBrush_);
                return true;
            }
            break;
        }
        return false;
    }

    HWND EditorShell::MakeOwnerStatic_(const wchar_t* text, int id)
    {
        HWND h = CreateWindowExW(0, L"STATIC", text,
            WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
            0, 0, 0, 0, hwnd_, id ? reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)) : nullptr,
            GetModuleHandleW(nullptr), nullptr);
        SetFont(h, uiFont_);
        return h;
    }

    HWND EditorShell::MakeHeader_(const wchar_t* text)
    {
        HWND h = MakeOwnerStatic_(text);
        SetFont(h, titleFont_);
        return h;
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

        Fill(dis->hDC, dis->rcItem, EditorTheme::Colors().panelBg);
    }

    void EditorShell::DrawHeader_(DRAWITEMSTRUCT* dis)
    {
        const auto& c = EditorTheme::Colors();
        Fill(dis->hDC, dis->rcItem, c.panelBgAlt);

        RECT dot{ 12, (dis->rcItem.bottom - 6) / 2, 18, (dis->rcItem.bottom - 6) / 2 + 6 };
        HBRUSH dotBrush = CreateSolidBrush(Blend(c.accent, c.textMuted, 18));
        FillRect(dis->hDC, &dot, dotBrush);
        DeleteObject(dotBrush);

        wchar_t title[128]{};
        GetWindowTextW(dis->hwndItem, title, static_cast<int>(std::size(title)));
        RECT textRc = dis->rcItem;
        textRc.left = 26;
        Text(dis->hDC, title, textRc, c.textPrimary, titleFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        HPEN pen = CreatePen(PS_SOLID, 1, c.border);
        HGDIOBJ oldPen = SelectObject(dis->hDC, pen);
        MoveToEx(dis->hDC, 0, dis->rcItem.bottom - 1, nullptr);
        LineTo(dis->hDC, dis->rcItem.right, dis->rcItem.bottom - 1);
        SelectObject(dis->hDC, oldPen);
        DeleteObject(pen);
    }

    void EditorShell::DrawViewport_(DRAWITEMSTRUCT* dis)
    {
        const auto& c = EditorTheme::Colors();
        RECT rc = dis->rcItem;
        Fill(dis->hDC, rc, c.viewportBg);

        const COLORREF grid = Blend(c.viewportBg, c.border, 42);
        HPEN pen = CreatePen(PS_SOLID, 1, grid);
        HGDIOBJ oldPen = SelectObject(dis->hDC, pen);

        const int horizon = rc.top + (rc.bottom - rc.top) * 40 / 100;
        for (int y = horizon; y < rc.bottom; y += 24)
        {
            MoveToEx(dis->hDC, rc.left, y, nullptr);
            LineTo(dis->hDC, rc.right, y);
        }
        const int cx = (rc.left + rc.right) / 2;
        for (int x = -10; x <= 10; ++x)
        {
            const int bottomX = cx + x * 58;
            MoveToEx(dis->hDC, cx, horizon, nullptr);
            LineTo(dis->hDC, bottomX, rc.bottom);
        }
        SelectObject(dis->hDC, oldPen);
        DeleteObject(pen);

        RECT brandRc{ rc.left, horizon - 36, rc.right, horizon + 22 };
        Text(dis->hDC, L"N", brandRc, Blend(c.viewportBg, c.textMuted, 38), brandFont_,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        RECT titleRc{ rc.left, horizon + 22, rc.right, horizon + 48 };
        Text(dis->hDC, L"NOCTURNE VIEWPORT", titleRc, c.textPrimary, uiFontBold_,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        RECT subRc{ rc.left + 40, horizon + 45, rc.right - 40, horizon + 69 };
        Text(dis->hDC, L"Phase 14 — rendering viewport, camera navigation, selection and gizmos",
            subRc, c.textMuted, mutedFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        const int axisX = rc.left + 34;
        const int axisY = rc.bottom - 34;
        HPEN xPen = CreatePen(PS_SOLID, 2, RGB(230, 73, 78));
        HPEN yPen = CreatePen(PS_SOLID, 2, RGB(94, 211, 111));
        oldPen = SelectObject(dis->hDC, xPen);
        MoveToEx(dis->hDC, axisX, axisY, nullptr); LineTo(dis->hDC, axisX + 31, axisY);
        SelectObject(dis->hDC, yPen);
        MoveToEx(dis->hDC, axisX, axisY, nullptr); LineTo(dis->hDC, axisX, axisY - 31);
        SelectObject(dis->hDC, oldPen);
        DeleteObject(xPen); DeleteObject(yPen);
        RECT xLabel{ axisX + 34, axisY - 8, axisX + 50, axisY + 10 };
        RECT yLabel{ axisX - 5, axisY - 48, axisX + 10, axisY - 31 };
        Text(dis->hDC, L"X", xLabel, RGB(230, 73, 78), uiFontBold_, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        Text(dis->hDC, L"Y", yLabel, RGB(94, 211, 111), uiFontBold_, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        RECT badge{ rc.right - 120, rc.bottom - 42, rc.right - 16, rc.bottom - 12 };
        RoundBox(dis->hDC, badge, Blend(c.viewportBg, c.panelBgAlt, 60), c.border, 6);
        Text(dis->hDC, L"Grid: 1.0 m", badge, c.textMuted, mutedFont_,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    void EditorShell::DrawInspector_(DRAWITEMSTRUCT* dis)
    {
        const auto& c = EditorTheme::Colors();
        RECT rc = dis->rcItem;
        Fill(dis->hDC, rc, c.panelBg);

        RECT title{ rc.left + 18, rc.top + 56, rc.right - 18, rc.top + 82 };
        Text(dis->hDC, L"No object selected", title, c.textPrimary, uiFontBold_,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        RECT helper{ rc.left + 24, rc.top + 86, rc.right - 24, rc.top + 132 };
        Text(dis->hDC, L"Select an object in the scene to inspect its properties.", helper,
            c.textMuted, mutedFont_, DT_CENTER | DT_WORDBREAK);

        HPEN pen = CreatePen(PS_SOLID, 1, c.border);
        HGDIOBJ oldPen = SelectObject(dis->hDC, pen);
        MoveToEx(dis->hDC, rc.left + 18, rc.top + 154, nullptr);
        LineTo(dis->hDC, rc.right - 18, rc.top + 154);
        SelectObject(dis->hDC, oldPen);
        DeleteObject(pen);

        RECT tipsTitle{ rc.left + 20, rc.top + 170, rc.right - 20, rc.top + 196 };
        Text(dis->hDC, L"Tips", tipsTitle, c.textPrimary, uiFontBold_, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        const wchar_t* tips[] = {
            L"•  Select an object in Scene Hierarchy or click in the viewport.",
            L"•  Use Select / Move / Rotate / Scale from the toolbar.",
            L"•  Inspector components arrive with scene editing."
        };
        int y = rc.top + 203;
        for (const wchar_t* tip : tips)
        {
            RECT tr{ rc.left + 22, y, rc.right - 18, y + 44 };
            Text(dis->hDC, tip, tr, c.textMuted, mutedFont_, DT_LEFT | DT_WORDBREAK);
            y += 56;
        }
    }

    void EditorShell::DrawBuildPlay_(DRAWITEMSTRUCT* dis)
    {
        const auto& c = EditorTheme::Colors();
        RECT rc = dis->rcItem;
        Fill(dis->hDC, rc, c.panelBg);

        RECT heading{ rc.left + 16, rc.top + 12, rc.right - 16, rc.top + 36 };
        Text(dis->hDC, L"Play Options", heading, c.textPrimary, uiFontBold_,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        struct Row { const wchar_t* label; const wchar_t* value; } rows[] = {
            { L"Play Mode", L"Selected Viewport" },
            { L"Start Map", L"Current Scene" }
        };

        int y = rc.top + 46;
        for (const auto& row : rows)
        {
            RECT label{ rc.left + 16, y, rc.left + 108, y + 31 };
            Text(dis->hDC, row.label, label, c.textMuted, mutedFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            RECT field{ rc.left + 118, y + 1, rc.right - 16, y + 30 };
            RoundBox(dis->hDC, field, c.inputBg, c.border, EditorTheme::Metrics().inputRadius);
            field.left += 10;
            Text(dis->hDC, row.value, field, c.textPrimary, uiFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            y += 40;
        }

        RECT vsyncLabel{ rc.left + 16, y + 2, rc.left + 108, y + 31 };
        Text(dis->hDC, L"VSync", vsyncLabel, c.textMuted, mutedFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT toggle{ rc.left + 118, y + 6, rc.left + 160, y + 26 };
        RoundBox(dis->hDC, toggle, c.accent, c.accent, 10);
        HBRUSH knob = CreateSolidBrush(RGB(235, 243, 252));
        HGDIOBJ oldBrush = SelectObject(dis->hDC, knob);
        HGDIOBJ oldPen = SelectObject(dis->hDC, GetStockObject(NULL_PEN));
        Ellipse(dis->hDC, toggle.right - 18, toggle.top + 3, toggle.right - 4, toggle.bottom - 3);
        SelectObject(dis->hDC, oldPen);
        SelectObject(dis->hDC, oldBrush);
        DeleteObject(knob);
        RECT enabled{ rc.left + 170, y + 1, rc.right - 16, y + 31 };
        Text(dis->hDC, L"Enabled", enabled, c.textPrimary, uiFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    void EditorShell::DrawStatus_(DRAWITEMSTRUCT* dis)
    {
        const auto& c = EditorTheme::Colors();
        RECT rc = dis->rcItem;
        Fill(dis->hDC, rc, c.toolbarBg);

        HPEN pen = CreatePen(PS_SOLID, 1, c.border);
        HGDIOBJ oldPen = SelectObject(dis->hDC, pen);
        MoveToEx(dis->hDC, 0, 0, nullptr); LineTo(dis->hDC, rc.right, 0);
        SelectObject(dis->hDC, oldPen);
        DeleteObject(pen);

        const int objects = engine_ ? static_cast<int>(engine_->GetWorld().AliveCount()) : 0;
        std::wstringstream objectText;
        objectText << L"Objects: " << objects;

        RECT ready{ 18, 0, 145, rc.bottom };
        Text(dis->hDC, L"●  Ready", ready, c.success, mutedFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT issues{ 155, 0, 290, rc.bottom };
        Text(dis->hDC, L"✓  No Issues", issues, c.success, mutedFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT branch{ 305, 0, rc.right - 410, rc.bottom };
        Text(dis->hDC, L"Branch: phase-13-editor-framework", branch, c.textMuted, mutedFont_,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        RECT objectsRc{ (std::max)(rc.left, rc.right - 390), 0, rc.right - 250, rc.bottom };
        const std::wstring objectsString = objectText.str();
        Text(dis->hDC, objectsString.c_str(), objectsRc, c.textMuted, mutedFont_,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT engineRc{ rc.right - 235, 0, rc.right - 62, rc.bottom };
        Text(dis->hDC, L"Nocturne Engine", engineRc, c.textPrimary, mutedFont_,
            DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        RECT versionRc{ rc.right - 56, 0, rc.right - 12, rc.bottom };
        Text(dis->hDC, L"v0.1.0", versionRc, c.textMuted, mutedFont_,
            DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }

    void EditorShell::DrawBand_(DRAWITEMSTRUCT* dis)
    {
        const auto& c = EditorTheme::Colors();
        Fill(dis->hDC, dis->rcItem, c.toolbarBg);
        HPEN pen = CreatePen(PS_SOLID, 1, c.border);
        HGDIOBJ oldPen = SelectObject(dis->hDC, pen);
        MoveToEx(dis->hDC, 0, dis->rcItem.bottom - 1, nullptr);
        LineTo(dis->hDC, dis->rcItem.right, dis->rcItem.bottom - 1);
        SelectObject(dis->hDC, oldPen);
        DeleteObject(pen);
    }

    std::wstring EditorShell::Utf8ToWide_(const char* text)
    {
        if (!text || !*text) return {};
        const int size = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
        if (size <= 1) return {};
        std::wstring out(static_cast<size_t>(size - 1), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text, -1, out.data(), size);
        return out;
    }
}
