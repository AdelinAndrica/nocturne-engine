#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <array>
#include <cstdint>
#include <cwctype>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "Platform/Win32/WinWindow.h"
#include "EditorInspectorModel.h"
#include "Runtime/Entity.h"

namespace noc { class Engine; }
namespace nocturne::editor { class EditorSession; }

namespace nocturne::editor
{
    enum class InspectorEditPresentation : uint8_t
    {
        Generic = 0,
        Vector2Axis,
        Vector3Axis,
        Vector4Axis,
        EulerDegreesAxis,
        AngleDegrees
    };

    // Phase 13 UI Fidelity Pass 3 shell.
    // Design choice (not directly from the book): editor-only custom Win32/GDI controls
    // provide deterministic Nocturne styling without leaking into runtime game UI.
    class EditorShellV3 final : public noc::platform::IWindowMessageSink
    {
    public:
        bool Init(
            noc::Engine& engine,
            noc::WinWindow& window,
            EditorSession& session);
        void Shutdown();

        bool OnWindowMessage(void* hwnd, uint32_t msg, uintptr_t wParam,
            intptr_t lParam, intptr_t& result) override;

        // Phase 14 integration seam. The shell retains ownership of layout/chrome;
        // the viewport controller receives only the existing insertion surface and
        // read-only tool selection state.
        HWND ViewportBody() const { return viewport_.body; }
        HWND SceneTree() const { return sceneTree_; }
        int ActiveToolId() const { return activeToolId_; }
        void SyncSceneSelection();
        void RefreshInspector();

        // MainLoop message-filter seam: handles editor accelerators before
        // focused child HWNDs consume them.
        [[nodiscard]] bool FilterMessage(const MSG& message);

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
            IdViewportOrientation,

            IdMenuFile = 9001,
            IdMenuEdit,
            IdMenuWindow,
            IdMenuTools,
            IdMenuBuild,
            IdMenuSelect,
            IdMenuActor,
            IdMenuHelp,

            IdActorCreate = 9101,
            IdActorDuplicate,
            IdActorDelete,
            IdActorRename,
            IdSceneRenameEdit,

            IdInspectorAddComponent = 11000,
            IdInspectorEditBase = 12000,
            IdInspectorRemoveBase = 13000,
            IdInspectorResourceBase = 14000,
            IdInspectorBoolBase = 15000,
            IdInspectorEnumBase = 16000
        };

        struct Panel
        {
            HWND header = nullptr;
            HWND body = nullptr;
        };

        struct InspectorEditBinding
        {
            HWND hwnd = nullptr;
            noc::TypeId componentTypeId{};
            noc::PropertyId propertyId{};
            InspectorEditPresentation presentation =
                InspectorEditPresentation::Generic;
            uint8_t axis = 0;
            std::array<noc::PropertyId, 2> nestedPath{};
            uint8_t nestedPathCount = 0;
        };

        struct InspectorComponentActionBinding
        {
            HWND hwnd = nullptr;
            noc::TypeId componentTypeId{};
        };

        struct InspectorResourceBinding
        {
            HWND hwnd = nullptr;
            noc::TypeId componentTypeId{};
            noc::PropertyId propertyId{};
            noc::TypeId resourceConstraint{};
        };

        struct InspectorBoolBinding
        {
            HWND hwnd = nullptr;
            noc::TypeId componentTypeId{};
            noc::PropertyId propertyId{};
        };

        struct InspectorEnumBinding
        {
            HWND hwnd = nullptr;
            noc::TypeId componentTypeId{};
            noc::PropertyId propertyId{};
            noc::TypeId valueTypeId{};
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

        [[nodiscard]] bool ExecuteCreateEntity_();
        [[nodiscard]] bool ExecuteDeleteSelection_();
        [[nodiscard]] bool ExecuteDuplicateSelection_();
        [[nodiscard]] bool ExecuteReparentEntity_(
            noc::EntityHandle child,
            noc::EntityHandle parent);
        void ShowHierarchyContextMenu_(
            POINT screenPoint);
        [[nodiscard]] bool BeginRenameSelection_();
        [[nodiscard]] bool CommitRename_();
        void CancelRename_() noexcept;
        [[nodiscard]] bool HasTextInputFocus_() const;

        static LRESULT CALLBACK RenameEditSubclassProc_(
            HWND hwnd,
            UINT message,
            WPARAM wParam,
            LPARAM lParam,
            UINT_PTR subclassId,
            DWORD_PTR refData);

        void DestroyInspectorControls_() noexcept;
        [[nodiscard]] bool RebuildInspectorControls_();
        void LayoutInspectorControls_();
        [[nodiscard]] int InspectorContentHeight_() const noexcept;
        void ClampInspectorScroll_() noexcept;
        void SyncInspectorControlValues_();
        [[nodiscard]] bool SyncInspectorBindingValue_(
            const InspectorEditBinding& binding);
        [[nodiscard]] bool CommitInspectorEdit_(HWND source);
        void CancelInspectorEdit_(HWND source);

        [[nodiscard]] bool ExecuteAddComponent_(
            noc::TypeId componentTypeId);
        [[nodiscard]] bool ExecuteRemoveComponent_(
            noc::TypeId componentTypeId);
        void ShowAddComponentPopup_();
        [[nodiscard]] InspectorEditBinding* FindInspectorEdit_(
            HWND source) noexcept;
        [[nodiscard]] const InspectorEditBinding* FindInspectorEdit_(
            HWND source) const noexcept;
        [[nodiscard]] InspectorComponentActionBinding*
        FindInspectorRemoveButton_(HWND source) noexcept;
        [[nodiscard]] InspectorResourceBinding*
        FindInspectorResourceButton_(HWND source) noexcept;
        [[nodiscard]] bool SyncInspectorResourceValue_(
            const InspectorResourceBinding& binding);
        void ShowResourcePicker_(
            InspectorResourceBinding& binding);
        [[nodiscard]] InspectorBoolBinding*
        FindInspectorBoolButton_(HWND source) noexcept;
        [[nodiscard]] bool SyncInspectorBoolValue_(
            const InspectorBoolBinding& binding);
        void ToggleInspectorBool_(
            InspectorBoolBinding& binding);
        [[nodiscard]] InspectorEnumBinding*
        FindInspectorEnumButton_(HWND source) noexcept;
        [[nodiscard]] bool SyncInspectorEnumValue_(
            const InspectorEnumBinding& binding);
        void ShowInspectorEnumPopup_(
            InspectorEnumBinding& binding);

        static LRESULT CALLBACK InspectorBodySubclassProc_(
            HWND hwnd,
            UINT message,
            WPARAM wParam,
            LPARAM lParam,
            UINT_PTR subclassId,
            DWORD_PTR refData);

        static LRESULT CALLBACK InspectorEditSubclassProc_(
            HWND hwnd,
            UINT message,
            WPARAM wParam,
            LPARAM lParam,
            UINT_PTR subclassId,
            DWORD_PTR refData);

        std::wstring ResolveContentRoot_(const std::wstring& configured) const;
        static std::wstring Utf8ToWide_(const char* text);
        static bool WideToUtf8_(
            const wchar_t* text,
            std::string& outText);

    private:
        noc::Engine* engine_ = nullptr;
        noc::WinWindow* window_ = nullptr;
        EditorSession* session_ = nullptr;
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
        HMENU actorMenu_ = nullptr;

        Panel scene_;
        Panel viewport_;
        Panel inspector_;
        Panel content_;
        Panel console_;
        Panel buildPlay_;

        HWND sceneTree_ = nullptr;
        HWND renameEdit_ = nullptr;
        noc::EntityHandle renameEntity_{};
        bool renameEnding_ = false;

        HWND viewportPerspective_ = nullptr;
        HWND viewportLit_ = nullptr;
        HWND viewportShow_ = nullptr;
        HWND viewportOrientation_ = nullptr;
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

        EditorInspectorModel inspectorModel_;
        std::vector<InspectorEditBinding> inspectorEdits_;
        std::vector<InspectorComponentActionBinding>
            inspectorRemoveButtons_;
        std::vector<InspectorResourceBinding>
            inspectorResourceButtons_;
        std::vector<InspectorBoolBinding>
            inspectorBoolButtons_;
        std::vector<InspectorEnumBinding>
            inspectorEnumButtons_;
        HWND inspectorAddComponent_ = nullptr;
        bool inspectorControlsRefreshing_ = false;
        int inspectorScrollY_ = 0;

        int activeToolId_ = IdToolbarSelect;
        std::wstring contentRoot_ = L"Data";
    };
}
