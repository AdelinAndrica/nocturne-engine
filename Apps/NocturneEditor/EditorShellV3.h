#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cstdint>
#include <string>
#include <vector>

#include "Platform/Win32/WinWindow.h"

namespace noc { class Engine; }

namespace nocturne::editor
{
    // Phase 13 UI Fidelity Pass 3 shell.
    // Design choice (not directly from the book): editor-only custom Win32/GDI controls
    // provide deterministic Nocturne styling without leaking into runtime game UI.
    class EditorShellV3 final : public noc::platform::IWindowMessageSink
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
            IdContentTable,
            IdContentSearch,
            IdContentListMode,
            IdContentGridMode,
            IdContentSettings,
            IdConsole,
            IdConsoleScroll,
            IdInspector,
            IdPlay,
            IdBuild,
            IdStatus,
            IdViewportPerspective,
            IdViewportLit,
            IdViewportShow,

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
            HWND header = nullptr;
            HWND body = nullptr;
        };

        void CreateChrome_();
        void CreateToolbar_();
        void CreatePanels_();
        void PopulateScene_();
        void PopulateContent_();
        void Layout_(int width, int height);
        void AppendConsole_(const wchar_t* message);
        void HandleCommand_(int id);
        void ShowPopup_(int menuId, HWND anchor);
        void UpdateStatus_();

        std::wstring ResolveContentRoot_(const std::wstring& configured) const;
        static std::wstring Utf8ToWide_(const char* text);

    private:
        noc::Engine* engine_ = nullptr;
        noc::WinWindow* window_ = nullptr;
        HWND hwnd_ = nullptr;

        HFONT uiFont_ = nullptr;
        HFONT uiBold_ = nullptr;
        HFONT smallFont_ = nullptr;
        HFONT consoleFont_ = nullptr;
        HFONT brandFont_ = nullptr;

        HBRUSH windowBrush_ = nullptr;
        HBRUSH editBrush_ = nullptr;

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
        HWND viewportPerspective_ = nullptr;
        HWND viewportLit_ = nullptr;
        HWND viewportShow_ = nullptr;
        HWND contentSearch_ = nullptr;
        HWND contentTree_ = nullptr;
        HWND contentTable_ = nullptr;
        HWND contentListMode_ = nullptr;
        HWND contentGridMode_ = nullptr;
        HWND contentSettings_ = nullptr;
        HWND consoleEdit_ = nullptr;
        HWND consoleScroll_ = nullptr;
        HWND playButton_ = nullptr;
        HWND buildButton_ = nullptr;
        HWND status_ = nullptr;

        int activeToolId_ = IdToolbarSelect;
        std::wstring contentRoot_ = L"Data";
    };
}
