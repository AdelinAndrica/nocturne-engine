#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "Core/Math/MathTypes.h"
#include "Runtime/Bounds.h"
#include "Runtime/SceneObject.h"

namespace noc
{
    class Engine;
    class WinWindow;
}

namespace nocturne::editor
{
    class EditorShellV3;

    // Phase 14 editor-only viewport behavior. The runtime still owns frame
    // execution; this controller only provides a child presentation target and
    // tool interactions layered over the existing runtime World/Render systems.
    class EditorViewportController final
    {
    public:
        bool PrepareScene(noc::Engine& engine);
        bool Attach(noc::Engine& engine, noc::WinWindow& window, EditorShellV3& shell);
        void Shutdown();

    private:
        static constexpr UINT_PTR kSubclassIdBody = 0x1401;
        static constexpr UINT_PTR kSubclassIdMain = 0x1402;
        static constexpr UINT_PTR kSubclassIdTree = 0x1403;
        static constexpr UINT_PTR kTimerId = 0x1410;

        static LRESULT CALLBACK OverlayProc_(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
        static LRESULT CALLBACK BodySubclassProc_(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
            UINT_PTR subclassId, DWORD_PTR refData);
        static LRESULT CALLBACK MainSubclassProc_(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
            UINT_PTR subclassId, DWORD_PTR refData);
        static LRESULT CALLBACK SceneTreeSubclassProc_(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
            UINT_PTR subclassId, DWORD_PTR refData);

        static bool RegisterOverlayClass_();
        void LayoutChildren_();
        void UpdateCamera_();
        void ApplyCameraTransform_();
        void PaintOverlay_(HDC dc, const RECT& rc);

        bool Project_(const noc::Vec3& world, POINT& out) const;
        noc::Vec3 MakePickRay_(int x, int y) const;
        bool RayAabb_(const noc::Vec3& origin, const noc::Vec3& dir, const noc::AABB& box, float& outT) const;
        noc::AABB DemoWorldBounds_() const;

        void SetSelected_(bool selected, bool syncTree = true);
        int HitGizmoAxis_(POINT p) const;
        void BeginGizmoDrag_(int axis, POINT mouse);
        void UpdateGizmoDrag_(POINT mouse);
        void EndGizmoDrag_();
        void HandleOverlayMouse_(UINT msg, WPARAM wParam, LPARAM lParam);

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
        bool selected_ = false;
        bool cameraCapturing_ = false;
        bool gizmoDragging_ = false;
        int gizmoAxis_ = -1;

        POINT lastMouse_{};
        POINT dragStartMouse_{};

        noc::SceneObjectHandle demoObject_{};
        noc::SceneObjectHandle cameraObject_{};
        noc::AABB demoLocalBounds_{ noc::Vec3(-1.0f, -1.0f, -0.08f), noc::Vec3(1.0f, 1.0f, 0.08f) };

        noc::Vec3 demoT_{ 0.0f, 0.0f, 6.0f };
        noc::Quat demoR_ = noc::Quat::Identity();
        noc::Vec3 demoS_ = noc::Vec3::One();
        noc::Vec3 dragStartT_{};
        noc::Quat dragStartR_ = noc::Quat::Identity();
        noc::Vec3 dragStartS_ = noc::Vec3::One();

        noc::Vec3 cameraPos_{ 0.0f, 1.25f, -6.0f };
        noc::Quat cameraRot_ = noc::Quat::Identity();
        float cameraYaw_ = 0.0f;
        float cameraPitch_ = -0.08f;
        float cameraSpeed_ = 5.0f;
        float fovY_ = 1.0471975512f; // 60 degrees
    };
}
