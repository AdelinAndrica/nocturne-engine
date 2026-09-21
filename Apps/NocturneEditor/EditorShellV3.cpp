#include "EditorShellV3.h"
#include "EditorShellV3Controls.h"
#include "EditorSession.h"
#include "EditorTheme.h"
#include "NocturneEditorResource.h"

#include "Core/Log.h"
#include "Runtime/Engine.h"
#include "Runtime/World.h"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <string>

#include <CommCtrl.h>
#include <dwmapi.h>
#include <Richedit.h>

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
