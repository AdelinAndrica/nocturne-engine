#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cstdint>
#include <cwctype>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "Platform/Win32/WinWindow.h"

namespace noc { class Engine; }

namespace nocturne::editor
{
    /**
     * @brief Current native Win32 shell for the Nocturne Editor.
     *
     * EditorShellV3 owns editor chrome/layout and native UI controls: menu/toolbar,
     * Scene Hierarchy, viewport insertion surface, Inspector, Content Browser,
     * Console, Build/Play and status UI.
     *
     * @par When to use
     * Create one shell for the editor's top-level window after Engine and WinWindow
     * exist. Let the shell own presentation; connect 3D viewport behavior through
     * EditorViewportController.
     *
     * @par Do not use for
     * EditorShellV3 is not runtime World authority, not the engine frame loop and not
     * gameplay UI. Never use the historical EditorShell/EditorControls path as the
     * baseline for new editor work.
     *
     * @par Ownership
     * The application owns EditorShellV3. It stores borrowed pointers to Engine and
     * WinWindow. The shell owns the native UI resources/controls it creates and releases
     * them during Shutdown().
     *
     * @par Runtime boundary
     * World/entity state remains in Engine::GetWorld(). The shell may display or issue
     * authoring commands but must not introduce a second authoritative scene model.
     *
     * @see EditorViewportController
     * @see EditorTheme
     * @ingroup editor
     */
    class EditorShellV3 final : public noc::platform::IWindowMessageSink
    {
    public:
        /**
         * @brief Creates/initializes editor chrome against the existing engine window.
         *
         * @param engine Existing initialized/runtime Engine. Borrowed.
         * @param window Existing editor top-level WinWindow. Borrowed.
         * @return true when shell initialization succeeds.
         */
        bool Init(noc::Engine& engine, noc::WinWindow& window);

        /**
         * @brief Destroys shell-owned UI resources and detaches presentation state.
         *
         * Call before destroying the top-level editor window/engine dependencies.
         */
        void Shutdown();

        /**
         * @brief Receives top-level Win32 messages through IWindowMessageSink.
         *
         * @param hwnd Message target.
         * @param msg Win32 message identifier.
         * @param wParam Win32 WPARAM value.
         * @param lParam Win32 LPARAM value.
         * @param result Receives a handled-message result.
         * @return true when the shell handled the message; false to continue normal
         * window processing.
         */
        bool OnWindowMessage(void* hwnd, uint32_t msg, uintptr_t wParam,
            intptr_t lParam, intptr_t& result) override;

        /**
         * @brief Returns the existing viewport body HWND used as controller insertion surface.
         *
         * @return Borrowed HWND owned by EditorShellV3.
         * @warning Do not destroy this HWND from EditorViewportController.
         */
        HWND ViewportBody() const { return viewport_.body; }

        /**
         * @brief Returns the Scene Hierarchy control HWND used by viewport/selection integration.
         * @return Borrowed HWND owned by EditorShellV3.
         */
        HWND SceneTree() const { return sceneTree_; }

        /**
         * @brief Returns the currently selected editor tool command ID.
         *
         * EditorViewportController reads this to choose select/move/rotate/scale behavior.
         */
        int ActiveToolId() const { return activeToolId_; }

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
        HFONT menuFont_ = nullptr;
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
