#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <CommCtrl.h>

#include <cstdint>
#include <string>
#include <vector>

#include "Platform/Win32/WinWindow.h"

namespace noc { class Engine; }

namespace nocturne::editor
{
    // Phase 13 editor shell.
    // Design choice (not directly from the book): native Win32 controls are retained for
    // the bootstrap, but custom drawing and centralized theme tokens remove the default
    // Win32 visual language. True docking remains deferred to a dedicated editor UI layer.
    class EditorShell final : public noc::platform::IWindowMessageSink
    {
    public:
        bool Init(noc::Engine& engine, noc::WinWindow& window);
        void Shutdown();

        bool OnWindowMessage(void* hwnd, uint32_t msg, uintptr_t wParam,
            intptr_t lParam, intptr_t& result) override;

    private:
        enum ControlId : int
        {
            IdToolbarNew = 1001,
            IdToolbarOpen,
            IdToolbarSave,
            IdToolbarUndo,
            IdToolbarRedo,
            IdToolbarSelect,
            IdToolbarMove,
            IdToolbarRotate,
            IdToolbarScale,
            IdToolbarPlay,
            IdToolbarStop,
            IdToolbarBuild,
            IdSceneTree,
            IdContentTree,
            IdContentList,
            IdContentSearch,
            IdConsole,
            IdInspector,
            IdPlay,
            IdBuild,
            IdStatus,

            IdMenuFile = 9001,
            IdMenuEdit,
            IdMenuWindow,
            IdMenuTools,
            IdMenuBuild,
            IdMenuSelect,
            IdMenuActor,
            IdMenuHelp
        };

        struct Panel
        {
            HWND title = nullptr;
            HWND body = nullptr;
        };

        void CreateMenuBar_();
        void CreateToolbar_();
        void CreatePanels_();
        void PopulateSceneTree_();
        void PopulateContentBrowser_();
        void ApplyThemeToCommonControls_();
        void Layout_(int clientW, int clientH);
        void AppendConsole_(const wchar_t* text);
        void UpdateStatus_();
        void HandleCommand_(int id);
        void ShowPopupMenu_(int menuId, HWND anchor);

        void DrawOwnerControl_(DRAWITEMSTRUCT* dis);
        void DrawButton_(DRAWITEMSTRUCT* dis);
        void DrawHeader_(DRAWITEMSTRUCT* dis);
        void DrawViewport_(DRAWITEMSTRUCT* dis);
        void DrawInspector_(DRAWITEMSTRUCT* dis);
        void DrawBuildPlay_(DRAWITEMSTRUCT* dis);
        void DrawStatus_(DRAWITEMSTRUCT* dis);
        void DrawBand_(DRAWITEMSTRUCT* dis);

        HWND MakeOwnerStatic_(const wchar_t* text, int id = 0);
        HWND MakeHeader_(const wchar_t* text);
        HWND MakeButton_(const wchar_t* text, int id);
        HWND MakeEdit_(DWORD extraStyle, int id);
        HWND MakeTree_(int id);
        HWND MakeList_(int id);

        static std::wstring Utf8ToWide_(const char* text);

    private:
        noc::Engine* engine_ = nullptr;
        noc::WinWindow* window_ = nullptr;
        HWND hwnd_ = nullptr;

        HFONT uiFont_ = nullptr;
        HFONT uiFontBold_ = nullptr;
        HFONT titleFont_ = nullptr;
        HFONT consoleFont_ = nullptr;
        HFONT brandFont_ = nullptr;

        HBRUSH windowBrush_ = nullptr;
        HBRUSH panelBrush_ = nullptr;
        HBRUSH panelAltBrush_ = nullptr;
        HBRUSH viewportBrush_ = nullptr;
        HBRUSH consoleBrush_ = nullptr;
        HBRUSH inputBrush_ = nullptr;

        HWND menuBand_ = nullptr;
        HWND toolbarBand_ = nullptr;
        std::vector<HWND> menuButtons_;
        std::vector<HWND> toolbarButtons_;

        HMENU fileMenu_ = nullptr;
        HMENU buildMenu_ = nullptr;

        Panel scene_;
        Panel viewport_;
        Panel inspector_;
        Panel content_;
        Panel console_;
        Panel buildPlay_;

        HWND sceneTree_ = nullptr;
        HWND contentTree_ = nullptr;
        HWND contentList_ = nullptr;
        HWND contentSearch_ = nullptr;
        HWND consoleEdit_ = nullptr;
        HWND inspectorText_ = nullptr;
        HWND playButton_ = nullptr;
        HWND buildButton_ = nullptr;
        HWND status_ = nullptr;

        int activeToolId_ = IdToolbarSelect;
        std::wstring projectName_ = L"Nocturne";
        std::wstring contentRoot_ = L"Data";
    };
}
