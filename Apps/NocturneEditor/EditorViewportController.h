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
    class EditorSession;
    class EditorShellV3;

    // Phase 14 editor-only viewport behavior. Runtime still owns frame execution;
    // this controller owns editor camera/input/selection/gizmo integration only.
    class EditorViewportController final
    {
    public:
        bool PrepareScene(
            noc::Engine& engine,
            EditorSession& session);
        bool Attach(
            noc::Engine& engine,
            noc::WinWindow& window,
            EditorShellV3& shell,
            EditorSession& session);
        void TickFrame();
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

        [[nodiscard]] int ValidationIndexForEntity_(
            noc::EntityHandle entity) const;
        [[nodiscard]] noc::EntityHandle SelectedEntity_() const;
        void SetSelectedEntity_(
            noc::EntityHandle entity,
            bool syncTree = true);
        void SetSelectedValidationIndex_(
            int index,
            bool syncTree = true);
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
        EditorSession* session_ = nullptr;

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
        noc::EntityHandle dragEntity_{};
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
