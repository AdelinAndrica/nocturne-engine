#include "EditorShell.h"

#include <filesystem>
#include <sstream>

#include "Core/Log.h"
#include "Runtime/Engine.h"
#include "Runtime/World.h"

#pragma comment(lib, "Comctl32.lib")

namespace nocturne::editor
{
    namespace
    {
        constexpr int kMenuHeight = 0; // native menu lives outside the client rect
        constexpr int kToolbarHeight = 42;
        constexpr int kStatusHeight = 24;
        constexpr int kGap = 5;
        constexpr int kLeftWidth = 310;
        constexpr int kRightWidth = 350;
        constexpr int kBottomHeight = 285;

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
    }

    bool EditorShell::Init(noc::Engine& engine, noc::WinWindow& window)
    {
        engine_ = &engine;
        window_ = &window;
        hwnd_ = static_cast<HWND>(window.Handle());
        if (!hwnd_)
            return false;

        INITCOMMONCONTROLSEX icc{};
        icc.dwSize = sizeof(icc);
        icc.dwICC = ICC_TREEVIEW_CLASSES | ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES;
        InitCommonControlsEx(&icc);

        uiFont_ = CreateFontW(
            -15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        panelBrush_ = CreateSolidBrush(RGB(241, 241, 241));
        viewportBrush_ = CreateSolidBrush(RGB(46, 49, 54));
        consoleBrush_ = CreateSolidBrush(RGB(28, 30, 33));

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
        AppendConsole_(L"Phase 13: editor framework bootstrap active.");
        AppendConsole_(L"Viewport interaction and gizmos are intentionally deferred to Phase 14.");
        UpdateStatus_();

        NOC_LOG_INFO("Editor", "Nocturne Editor shell initialized");
        return true;
    }

    void EditorShell::Shutdown()
    {
        if (window_)
            window_->SetMessageSink(nullptr);

        if (uiFont_) DeleteObject(uiFont_);
        if (panelBrush_) DeleteObject(panelBrush_);
        if (viewportBrush_) DeleteObject(viewportBrush_);
        if (consoleBrush_) DeleteObject(consoleBrush_);

        uiFont_ = nullptr;
        panelBrush_ = nullptr;
        viewportBrush_ = nullptr;
        consoleBrush_ = nullptr;
        engine_ = nullptr;
        window_ = nullptr;
        hwnd_ = nullptr;
    }

    void EditorShell::CreateMenuBar_()
    {
        HMENU bar = CreateMenu();
        const wchar_t* labels[] = { L"File", L"Edit", L"Window", L"Tools", L"Build", L"Select", L"Actor", L"Help" };
        for (const wchar_t* label : labels)
        {
            HMENU popup = CreatePopupMenu();
            if (wcscmp(label, L"File") == 0)
            {
                AppendMenuW(popup, MF_STRING, IdToolbarNew, L"New");
                AppendMenuW(popup, MF_STRING, IdToolbarOpen, L"Open...");
                AppendMenuW(popup, MF_STRING, IdToolbarSave, L"Save");
                AppendMenuW(popup, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(popup, MF_STRING, IDCANCEL, L"Exit");
            }
            else if (wcscmp(label, L"Build") == 0)
            {
                AppendMenuW(popup, MF_STRING, IdToolbarBuild, L"Build Content");
                AppendMenuW(popup, MF_STRING, IdToolbarPlay, L"Play");
            }
            else
            {
                AppendMenuW(popup, MF_STRING | MF_GRAYED, 0, L"Phase 13 bootstrap");
            }
            AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(popup), label);
        }
        SetMenu(hwnd_, bar);
    }

    void EditorShell::CreateToolbar_()
    {
        const struct { const wchar_t* text; int id; } defs[] = {
            { L"New", IdToolbarNew }, { L"Open", IdToolbarOpen }, { L"Save", IdToolbarSave },
            { L"Undo", IdToolbarUndo }, { L"Redo", IdToolbarRedo }, { L"Select", IdToolbarSelect },
            { L"Move", IdToolbarMove }, { L"Rotate", IdToolbarRotate }, { L"Scale", IdToolbarScale },
            { L"Play", IdToolbarPlay }, { L"Stop", IdToolbarStop }, { L"Build", IdToolbarBuild }
        };

        for (const auto& def : defs)
            toolbarButtons_.push_back(MakeButton_(def.text, def.id));
    }

    void EditorShell::CreatePanels_()
    {
        scene_.title = MakeStatic_(L"  Scene Hierarchy");
        sceneTree_ = MakeTree_(IdSceneTree);
        scene_.body = sceneTree_;

        viewport_.title = MakeStatic_(L"  Viewport");
        viewport_.body = MakeStatic_(
            L"NOCTURNE VIEWPORT\r\n\r\nPhase 14 — rendering viewport, camera navigation, selection and gizmos",
            SS_CENTER | SS_CENTERIMAGE);

        inspector_.title = MakeStatic_(L"  Inspector / Properties");
        inspectorText_ = MakeStatic_(
            L"Nothing Selected\r\n\r\nSelect an object in the scene to inspect its properties.",
            SS_CENTER | SS_CENTERIMAGE);
        inspector_.body = inspectorText_;

        content_.title = MakeStatic_(L"  Content Browser");
        contentTree_ = MakeTree_(IdContentTree);
        contentList_ = MakeList_(IdContentList);
        contentSearch_ = MakeEdit_(ES_AUTOHSCROLL, IdContentSearch);
        SetWindowTextW(contentSearch_, L"Search Assets...");
        content_.body = contentList_;

        console_.title = MakeStatic_(L"  Console / Output");
        consoleEdit_ = MakeEdit_(ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL, IdConsole);
        console_.body = consoleEdit_;

        buildPlay_.title = MakeStatic_(L"  Build / Play");
        buildPlay_.body = MakeStatic_(L"Play Options\r\n\r\nPlay Mode: Selected Viewport\r\nStart Map: Current Scene\r\n\r\nVSync: Enabled");
        playButton_ = MakeButton_(L"Play (F5)", IdPlay);
        buildButton_ = MakeButton_(L"Build", IdBuild);

        status_ = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr,
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
            0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(IdStatus),
            GetModuleHandleW(nullptr), nullptr);
        SetFont(status_, uiFont_);

        const int parts[] = { 450, 760, 980, 1180, -1 };
        SendMessageW(status_, SB_SETPARTS, 5, reinterpret_cast<LPARAM>(parts));
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
    }

    void EditorShell::PopulateContentBrowser_()
    {
        TreeView_DeleteAllItems(contentTree_);
        ListView_DeleteAllItems(contentList_);

        HTREEITEM root = InsertTreeItem(contentTree_, TVI_ROOT, L"Content");
        TreeView_Expand(contentTree_, root, TVE_EXPAND);

        namespace fs = std::filesystem;
        std::error_code ec;
        fs::path rootPath(contentRoot_);
        if (!fs::exists(rootPath, ec))
        {
            InsertTreeItem(contentTree_, root, L"<content root unavailable>");
            return;
        }

        int row = 0;
        for (const auto& entry : fs::directory_iterator(rootPath, ec))
        {
            if (ec) break;
            const std::wstring name = entry.path().filename().wstring();
            if (entry.is_directory(ec))
                InsertTreeItem(contentTree_, root, name.c_str());

            LVITEMW item{};
            item.mask = LVIF_TEXT;
            item.iItem = row++;
            item.pszText = const_cast<wchar_t*>(name.c_str());
            ListView_InsertItem(contentList_, &item);
            ListView_SetItemText(contentList_, item.iItem, 1,
                const_cast<wchar_t*>(entry.is_directory(ec) ? L"Folder" : L"Asset"));
        }
    }

    void EditorShell::Layout_(int clientW, int clientH)
    {
        if (clientW <= 0 || clientH <= 0)
            return;

        int x = kGap;
        const int toolbarY = kGap;
        for (HWND button : toolbarButtons_)
        {
            MoveWindow(button, x, toolbarY, 72, 30, TRUE);
            x += 76;
        }

        const int top = kToolbarHeight + kGap;
        const int availableH = clientH - top - kStatusHeight - kGap;
        const int topH = (availableH > kBottomHeight + 140) ? availableH - kBottomHeight : availableH / 2;
        const int bottomY = top + topH + kGap;
        const int bottomH = availableH - topH - kGap;

        const int centerX = kLeftWidth + 2 * kGap;
        const int centerW = clientW - kLeftWidth - kRightWidth - 4 * kGap;
        const int rightX = centerX + centerW + kGap;

        auto placePanel = [](Panel panel, int px, int py, int pw, int ph)
        {
            constexpr int titleH = 28;
            MoveWindow(panel.title, px, py, pw, titleH, TRUE);
            MoveWindow(panel.body, px, py + titleH, pw, (std::max)(0, ph - titleH), TRUE);
        };

        placePanel(scene_, kGap, top, kLeftWidth, topH);
        placePanel(viewport_, centerX, top, centerW, topH);
        placePanel(inspector_, rightX, top, kRightWidth, topH);

        const int bottomLeftW = 520;
        const int bottomRightW = kRightWidth;
        const int bottomCenterX = kGap + bottomLeftW + kGap;
        const int bottomCenterW = clientW - bottomLeftW - bottomRightW - 4 * kGap;
        const int bottomRightX = bottomCenterX + bottomCenterW + kGap;

        placePanel(content_, kGap, bottomY, bottomLeftW, bottomH);
        placePanel(console_, bottomCenterX, bottomY, bottomCenterW, bottomH);
        placePanel(buildPlay_, bottomRightX, bottomY, bottomRightW, bottomH);

        const int contentTop = bottomY + 28;
        const int contentInnerH = bottomH - 28;
        MoveWindow(contentSearch_, kGap + 8, contentTop + 8, bottomLeftW - 16, 25, TRUE);
        MoveWindow(contentTree_, kGap + 8, contentTop + 39, 175, (std::max)(0, contentInnerH - 47), TRUE);
        MoveWindow(contentList_, kGap + 188, contentTop + 39, bottomLeftW - 196, (std::max)(0, contentInnerH - 47), TRUE);

        const int bpX = bottomRightX;
        const int bpY = bottomY + 28;
        MoveWindow(buildPlay_.body, bpX, bpY, bottomRightW, (std::max)(0, bottomH - 82), TRUE);
        MoveWindow(playButton_, bpX + 12, bottomY + bottomH - 48, 150, 36, TRUE);
        MoveWindow(buildButton_, bpX + 172, bottomY + bottomH - 48, 150, 36, TRUE);

        SendMessageW(status_, WM_SIZE, 0, 0);
    }

    void EditorShell::AppendConsole_(const wchar_t* text)
    {
        if (!consoleEdit_) return;
        const int len = GetWindowTextLengthW(consoleEdit_);
        SendMessageW(consoleEdit_, EM_SETSEL, len, len);
        std::wstring line = L"[Editor] ";
        line += text;
        line += L"\r\n";
        SendMessageW(consoleEdit_, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(line.c_str()));
    }

    void EditorShell::UpdateStatus_()
    {
        if (!status_) return;
        SendMessageW(status_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(L"Ready"));
        SendMessageW(status_, SB_SETTEXTW, 1, reinterpret_cast<LPARAM>(L"No Issues"));
        SendMessageW(status_, SB_SETTEXTW, 2, reinterpret_cast<LPARAM>(L"Branch: phase-13-editor-framework"));

        std::wstringstream objects;
        objects << L"Objects: " << (engine_ ? engine_->GetWorld().AliveCount() : 0);
        const std::wstring objectText = objects.str();
        SendMessageW(status_, SB_SETTEXTW, 3, reinterpret_cast<LPARAM>(objectText.c_str()));
        SendMessageW(status_, SB_SETTEXTW, 4, reinterpret_cast<LPARAM>(L"Nocturne Engine"));
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
            AppendConsole_(L"Undo/Redo command received; edit history arrives with scene editing.");
            break;
        case IdToolbarSelect:
        case IdToolbarMove:
        case IdToolbarRotate:
        case IdToolbarScale:
            AppendConsole_(L"Viewport tool selected. Interactive gizmos are Phase 14 scope.");
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
        case IDCANCEL:
            if (window_) window_->RequestQuit();
            break;
        default:
            break;
        }
        UpdateStatus_();
    }

    bool EditorShell::OnWindowMessage(void*, uint32_t msg, uintptr_t wParam,
        intptr_t lParam, intptr_t& result)
    {
        switch (msg)
        {
        case WM_SIZE:
            Layout_(LOWORD(lParam), HIWORD(lParam));
            return false; // WinWindow still updates its cached client size.

        case WM_COMMAND:
            HandleCommand_(LOWORD(wParam));
            result = 0;
            return true;

        case WM_KEYDOWN:
            if (wParam == VK_F5)
            {
                HandleCommand_(IdPlay);
                result = 0;
                return true;
            }
            return false;

        case WM_CTLCOLORSTATIC:
        {
            HDC dc = reinterpret_cast<HDC>(wParam);
            HWND ctl = reinterpret_cast<HWND>(lParam);
            if (ctl == viewport_.body)
            {
                SetTextColor(dc, RGB(220, 220, 220));
                SetBkColor(dc, RGB(46, 49, 54));
                result = reinterpret_cast<intptr_t>(viewportBrush_);
                return true;
            }
            SetTextColor(dc, RGB(32, 32, 32));
            SetBkColor(dc, RGB(241, 241, 241));
            result = reinterpret_cast<intptr_t>(panelBrush_);
            return true;
        }

        case WM_CTLCOLOREDIT:
            if (reinterpret_cast<HWND>(lParam) == consoleEdit_)
            {
                HDC dc = reinterpret_cast<HDC>(wParam);
                SetTextColor(dc, RGB(220, 220, 220));
                SetBkColor(dc, RGB(28, 30, 33));
                result = reinterpret_cast<intptr_t>(consoleBrush_);
                return true;
            }
            return false;

        default:
            return false;
        }
    }

    HWND EditorShell::MakeStatic_(const wchar_t* text, DWORD style)
    {
        HWND h = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", text,
            WS_CHILD | WS_VISIBLE | style,
            0, 0, 0, 0, hwnd_, nullptr, GetModuleHandleW(nullptr), nullptr);
        SetFont(h, uiFont_);
        return h;
    }

    HWND EditorShell::MakeButton_(const wchar_t* text, int id)
    {
        HWND h = CreateWindowExW(0, L"BUTTON", text,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            GetModuleHandleW(nullptr), nullptr);
        SetFont(h, uiFont_);
        return h;
    }

    HWND EditorShell::MakeEdit_(DWORD extraStyle, int id)
    {
        HWND h = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | extraStyle,
            0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            GetModuleHandleW(nullptr), nullptr);
        SetFont(h, uiFont_);
        return h;
    }

    HWND EditorShell::MakeTree_(int id)
    {
        HWND h = CreateWindowExW(WS_EX_CLIENTEDGE, WC_TREEVIEWW, L"",
            WS_CHILD | WS_VISIBLE | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
            0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            GetModuleHandleW(nullptr), nullptr);
        SetFont(h, uiFont_);
        return h;
    }

    HWND EditorShell::MakeList_(int id)
    {
        HWND h = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            GetModuleHandleW(nullptr), nullptr);
        SetFont(h, uiFont_);
        ListView_SetExtendedListViewStyle(h, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

        LVCOLUMNW col{};
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.cx = 190;
        col.pszText = const_cast<wchar_t*>(L"Asset");
        ListView_InsertColumn(h, 0, &col);
        col.cx = 100;
        col.pszText = const_cast<wchar_t*>(L"Type");
        ListView_InsertColumn(h, 1, &col);
        return h;
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
