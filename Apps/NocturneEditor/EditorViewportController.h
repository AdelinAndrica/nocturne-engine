#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "Core/Math/MathTypes.h"
#include "Runtime/Bounds.h"
#include "Runtime/Entity.h"

namespace noc
{
    class Engine;
    class WinWindow;
    struct RenderableComponent;
    struct TransformComponent;
}

namespace nocturne::editor
{
    class EditorShellV3;

    /**
     * @brief Editor-only 3D viewport interaction bridge built on the runtime Engine/World.
     *
     * EditorViewportController owns editor camera input, picking, selection, hierarchy
     * synchronization, transform-gizmo interaction and the current deterministic
     * validation-scene bridge.
     *
     * @par What it does NOT own
     * It does not own Engine's frame loop, RenderSystem, authoritative Transform/
     * Renderable/Name/Camera state, or EditorShellV3 chrome.
     *
     * @par Typical setup
     * @code
     * EditorViewportController viewport;
     * if (!viewport.PrepareScene(engine)) return false;
     * if (!viewport.Attach(engine, window, shell)) return false;
     *
     * // Once per engine frame, before Engine::Tick():
     * viewport.TickFrame();
     *
     * // During editor shutdown:
     * viewport.Shutdown();
     * @endcode
     *
     * @par Ownership
     * Engine, WinWindow and EditorShellV3 pointers are borrowed. Validation entities and
     * the editor camera are runtime EntityHandle values owned by World.
     *
     * @par Current scope
     * PrepareScene() creates a deterministic Phase-14 validation scene (three cubes,
     * selectable ground and editor camera). This is development scaffolding, not
     * persistent scene serialization.
     *
     * @see EditorShellV3
     * @see noc::World
     * @ingroup editor
     */
    class EditorViewportController final
    {
    public:
        /**
         * @brief Creates the current deterministic validation entities/camera in World.
         *
         * Safe to call again after successful preparation; repeated calls return true
         * without duplicating the scene.
         *
         * @param engine Engine whose authoritative World/Resources are used.
         * @return true when all validation entities and camera state are prepared.
         *
         * @warning This is editor validation scaffolding, not a general scene-loading API.
         */
        bool PrepareScene(noc::Engine& engine);

        /**
         * @brief Connects viewport input/overlay/render-host controls to the editor shell.
         *
         * Attach creates child render-host/overlay HWNDs inside the shell's existing
         * viewport body, subclasses the required controls, lays them out and attaches
         * Engine rendering to the child render host.
         *
         * @param engine Runtime Engine. Borrowed.
         * @param window Editor top-level WinWindow. Borrowed.
         * @param shell Current EditorShellV3. Borrowed.
         * @return true when required native controls and render attachment succeed.
         *
         * @note PrepareScene() is called automatically if required.
         */
        bool Attach(noc::Engine& engine, noc::WinWindow& window, EditorShellV3& shell);

        /**
         * @brief Applies accumulated editor-camera input for the current engine frame.
         *
         * MainLoop's editor callback calls this before Engine::Tick(). It updates the
         * authoritative camera Transform through World, rather than maintaining a
         * separate runtime camera model.
         */
        void TickFrame();

        /**
         * @brief Removes Win32 subclasses/capture and clears controller links/state.
         *
         * Also clears Engine's debug-selection rendering payload.
         *
         * @note Current Shutdown() detaches controller/UI integration; it does not
         * destroy the validation entities created in World by PrepareScene().
         */
        void Shutdown();

    private:
        static constexpr UINT_PTR kSubclassIdBody = 0x1401;
        static constexpr UINT_PTR kSubclassIdTree = 0x1403;
        static constexpr UINT_PTR kSubclassIdRenderHost = 0x1404;
        static constexpr int kValidationObjectCount = 4;

        struct ValidationObject
        {
            noc::EntityHandle handle{};
            bool selectable = true;
        };

        static LRESULT CALLBACK OverlayProc_(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
        static LRESULT CALLBACK BodySubclassProc_(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
            UINT_PTR subclassId, DWORD_PTR refData);
        static LRESULT CALLBACK SceneTreeSubclassProc_(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
            UINT_PTR subclassId, DWORD_PTR refData);
        static LRESULT CALLBACK RenderHostSubclassProc_(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
            UINT_PTR subclassId, DWORD_PTR refData);

        static bool RegisterOverlayClass_();
        void LayoutChildren_();
        void UpdateCamera_();
        void ApplyCameraTransform_();
        void PaintOverlay_(HDC dc, const RECT& rc);
        void PaintSceneHierarchy_(HWND hwnd, HDC dc, const RECT& rc);
        int HierarchyRowFromY_(int y) const;
        int HierarchyObjectIndexFromRow_(int row) const;

        bool Project_(const noc::Vec3& world, POINT& out) const;
        noc::Vec3 MakePickRay_(int x, int y) const;
        bool RayAabb_(const noc::Vec3& origin, const noc::Vec3& dir, const noc::AABB& box, float& outT) const;
        bool RayValidationObject_(int index, const noc::Vec3& origin, const noc::Vec3& dir, float& outT) const;
        noc::AABB ValidationWorldBounds_(int index) const;
        const noc::TransformComponent* ValidationTransform_(int index) const;
        const noc::RenderableComponent* ValidationRenderable_(int index) const;
        int PickValidationObject_(const noc::Vec3& origin, const noc::Vec3& dir) const;

        void SetSelectedIndex_(int index, bool syncTree = true);
        void RefreshDebugSelection_();
        int HitGizmoAxis_(POINT p) const;
        void BeginGizmoDrag_(int axis, POINT mouse);
        void UpdateGizmoDrag_(POINT mouse);
        void EndGizmoDrag_();
        void HandleViewportMouse_(UINT msg, WPARAM wParam, LPARAM lParam);

        int ActiveTool_() const;

    private:
        noc::Engine* engine_ = nullptr;
        noc::WinWindow* window_ = nullptr;
        EditorShellV3* shell_ = nullptr;

        HWND topLevel_ = nullptr;
        HWND body_ = nullptr;
        HWND renderHost_ = nullptr;
        HWND overlay_ = nullptr;
        HWND sceneTree_ = nullptr;

        bool scenePrepared_ = false;
        bool renderAttached_ = false;
        bool cameraCapturing_ = false;
        bool gizmoDragging_ = false;
        bool hierarchyObjectsExpanded_ = true;
        int selectedIndex_ = -1;
        int dragObjectIndex_ = -1;
        int gizmoAxis_ = -1;
        int hierarchySelectedRow_ = 0;
        int hierarchyHoverRow_ = -1;

        POINT lastMouse_{};
        POINT dragStartMouse_{};
        int pendingMouseDx_ = 0;
        int pendingMouseDy_ = 0;

        ValidationObject validationObjects_[kValidationObjectCount]{};
        noc::EntityHandle cameraObject_{};

        noc::Vec3 dragStartT_{};
        noc::Quat dragStartR_ = noc::Quat::Identity();
        noc::Vec3 dragStartS_ = noc::Vec3::One();

        noc::Vec3 cameraPos_{ 0.0f, 1.4f, -6.0f };
        noc::Quat cameraRot_ = noc::Quat::Identity();
        float cameraYaw_ = 0.0f;
        float cameraPitch_ = -0.08f;
        float cameraSpeed_ = 5.0f;
        float fovY_ = 1.0471975512f; // 60 degrees
    };
}
