#include "EditorViewportController.h"
#include "EditorSession.h"
#include "EditorCommands.h"

#include "EditorShellV3.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <new>

#include <CommCtrl.h>
#include <Windowsx.h>

#include "Core/Clock.h"
#include "Core/Log.h"
#include "Runtime/Engine.h"
#include "Runtime/Components/TransformComponent.h"
#include "Runtime/Reflection/BuiltinTypes.h"
#include "Resources/ResourceManager.h"

namespace nocturne::editor
{
    namespace
    {
        constexpr wchar_t kOverlayClass[] = L"NocturnePhase14ViewportOverlay";
        constexpr COLORREF kOverlayKey = RGB(1, 2, 3);
        constexpr int kViewportControlStrip = 48;
        constexpr int kSelectTool = 1006;
        constexpr int kMoveTool = 1007;
        constexpr int kRotateTool = 1008;
        constexpr int kScaleTool = 1009;
        constexpr int kGizmoRingSegments = 64;
        constexpr float kGizmoTargetPixels = 88.0f;
        constexpr float kGizmoHitRadiusPixels = 9.0f;

        noc::Quat MulQuat(const noc::Quat& a, const noc::Quat& b)
        {
            return {
                a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
                a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
                a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
                a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
            };
        }

        noc::Quat NormalizeQuat(const noc::Quat& q)
        {
            const float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
            if (len <= 1e-6f) return noc::Quat::Identity();
            const float s = 1.0f / len;
            return { q.x * s, q.y * s, q.z * s, q.w * s };
        }

        noc::Quat AxisAngle(const noc::Vec3& axis, float radians)
        {
            const noc::Vec3 n = noc::Normalize(axis);
            const float h = radians * 0.5f;
            const float s = std::sin(h);
            return NormalizeQuat({ n.x * s, n.y * s, n.z * s, std::cos(h) });
        }

        noc::Quat YawPitch(float yaw, float pitch)
        {
            return NormalizeQuat(MulQuat(AxisAngle({ 0,1,0 }, yaw), AxisAngle({ 1,0,0 }, pitch)));
        }

        float PointSegmentDistance(POINT p, POINT a, POINT b)
        {
            const float vx = float(b.x - a.x);
            const float vy = float(b.y - a.y);
            const float wx = float(p.x - a.x);
            const float wy = float(p.y - a.y);
            const float d2 = vx * vx + vy * vy;
            if (d2 <= 1e-4f)
                return std::sqrt(wx * wx + wy * wy);
            const float t = (std::clamp)((wx * vx + wy * vy) / d2, 0.0f, 1.0f);
            const float dx = float(p.x) - (float(a.x) + vx * t);
            const float dy = float(p.y) - (float(a.y) + vy * t);
            return std::sqrt(dx * dx + dy * dy);
        }

        float GizmoWorldLength(HWND renderHost, const noc::Vec3& cameraPos,
            const noc::Quat& cameraRot, float fovY, const noc::Vec3& pivot)
        {
            RECT rc{};
            if (!renderHost || !GetClientRect(renderHost, &rc))
                return 1.5f;
            const float height = float((std::max)(1L, rc.bottom - rc.top));
            const noc::Vec3 forward = noc::Normalize(noc::Rotate(cameraRot, { 0,0,1 }));
            const float depth = (std::max)(0.05f, noc::Dot(pivot - cameraPos, forward));
            const float worldPerPixel = (2.0f * depth * std::tan(fovY * 0.5f)) / height;
            return (std::clamp)(worldPerPixel * kGizmoTargetPixels, 0.35f, 50.0f);
        }

        noc::Vec3 GizmoRingLocalPoint(int axis, float angle)
        {
            const float c = std::cos(angle);
            const float s = std::sin(angle);
            switch (axis)
            {
            case 0: return { 0.0f, c, s };     // YZ ring -> X rotation axis
            case 1: return { s, 0.0f, c };     // ZX ring -> Y rotation axis
            default: return { c, s, 0.0f };    // XY ring -> Z rotation axis
            }
        }

        void GizmoRingBasis(int axis, noc::Vec3& outU, noc::Vec3& outV)
        {
            switch (axis)
            {
            case 0: outU = { 0,1,0 }; outV = { 0,0,1 }; break;
            case 1: outU = { 0,0,1 }; outV = { 1,0,0 }; break;
            default: outU = { 1,0,0 }; outV = { 0,1,0 }; break;
            }
        }

        noc::Vec3 MatrixTranslation(
            const noc::Mat4& matrix)
        {
            return {
                noc::M(matrix, 0, 3),
                noc::M(matrix, 1, 3),
                noc::M(matrix, 2, 3)
            };
        }

        noc::Vec3 MatrixColumn(
            const noc::Mat4& matrix,
            int column)
        {
            return {
                noc::M(matrix, 0, column),
                noc::M(matrix, 1, column),
                noc::M(matrix, 2, column)
            };
        }

        noc::Vec3 ExpandGizmoBasisVector(
            const noc::Vec3 axes[3],
            const noc::Vec3& coefficients)
        {
            return axes[0] * coefficients.x
                + axes[1] * coefficients.y
                + axes[2] * coefficients.z;
        }

        noc::Vec3 GizmoRingWorldVector(
            const noc::Vec3 axes[3],
            int axis,
            float angle)
        {
            return ExpandGizmoBasisVector(
                axes,
                GizmoRingLocalPoint(axis, angle));
        }

        void DrawLine(HDC dc, POINT a, POINT b, COLORREF color, int width = 1)
        {
            HPEN pen = CreatePen(PS_SOLID, width, color);
            HGDIOBJ old = SelectObject(dc, pen);
            MoveToEx(dc, a.x, a.y, nullptr);
            LineTo(dc, b.x, b.y);
            SelectObject(dc, old);
            DeleteObject(pen);
        }

        void DrawArrowHead(HDC dc, POINT start, POINT end, COLORREF color)
        {
            const float dx = float(end.x - start.x);
            const float dy = float(end.y - start.y);
            const float len = std::sqrt(dx * dx + dy * dy);
            if (len < 1.0f) return;
            const float ux = dx / len;
            const float uy = dy / len;
            const float px = -uy;
            const float py = ux;
            const float bx = float(end.x) - ux * 12.0f;
            const float by = float(end.y) - uy * 12.0f;
            POINT points[3] = {
                end,
                { LONG(bx + px * 5.0f), LONG(by + py * 5.0f) },
                { LONG(bx - px * 5.0f), LONG(by - py * 5.0f) }
            };
            HBRUSH brush = CreateSolidBrush(color);
            HGDIOBJ oldBrush = SelectObject(dc, brush);
            HGDIOBJ oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
            Polygon(dc, points, 3);
            SelectObject(dc, oldPen);
            SelectObject(dc, oldBrush);
            DeleteObject(brush);
        }

        void DrawScaleHandle(HDC dc, POINT center, COLORREF color)
        {
            RECT rc{ center.x - 5, center.y - 5, center.x + 6, center.y + 6 };
            HBRUSH brush = CreateSolidBrush(color);
            HGDIOBJ oldBrush = SelectObject(dc, brush);
            HGDIOBJ oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
            Rectangle(dc, rc.left, rc.top, rc.right, rc.bottom);
            SelectObject(dc, oldPen);
            SelectObject(dc, oldBrush);
            DeleteObject(brush);
        }

    }

    bool EditorViewportController::PrepareScene(
        noc::Engine& engine,
        EditorSession& session)
    {
        if (scenePrepared_)
            return true;

        engine_ = &engine;
        session_ = &session;
        auto& world = engine.GetWorld();
        const noc::ResourceHandle logicalMesh = engine.Resources().RequestBinary("Meshes/triangle.nmsh");

        auto createBootstrapObject = [&](
            const char* name,
            const noc::Vec3& translation,
            const noc::Quat& rotation,
            const noc::Vec3& scale) -> noc::EntityHandle
        {
            const noc::EntityHandle entity = world.CreateObject();
            if (!entity.IsValid())
                return noc::EntityHandle::Invalid();

            const noc::AABB localBounds{
                noc::Vec3(-1.0f, -1.0f, -1.0f),
                noc::Vec3(1.0f, 1.0f, 1.0f)
            };

            if (!world.AddName(entity, name)
                || !world.SetLocalTRS(
                    entity,
                    translation,
                    rotation,
                    scale)
                || !world.SetRenderable(
                    entity,
                    logicalMesh,
                    localBounds))
            {
                (void)world.DestroyEntity(entity);
                return noc::EntityHandle::Invalid();
            }

            return entity;
        };

        // Design choice (not directly from the book): retain the deterministic
        // Phase 14 demo scene as startup content only. No editor subsystem stores
        // these handles as authoring identity; hierarchy and picking discover
        // authored entities directly from World.
        noc::EntityHandle bootstrap[4]{
            createBootstrapObject(
                "Cube_A",
                { 0.0f, -0.15f, 6.0f },
                noc::Quat::Identity(),
                { 1.0f, 1.0f, 1.0f }),
            createBootstrapObject(
                "Cube_B",
                { -2.5f, -0.10f, 9.0f },
                AxisAngle({ 0,1,0 }, 0.38f),
                { 0.8f, 1.05f, 0.8f }),
            createBootstrapObject(
                "Cube_C",
                { 2.4f, -0.35f, 11.0f },
                AxisAngle({ 0,1,0 }, -0.52f),
                { 1.1f, 0.8f, 1.1f }),
            createBootstrapObject(
                "Ground",
                { 0.0f, -1.25f, 9.0f },
                noc::Quat::Identity(),
                { 6.5f, 0.10f, 7.5f })
        };

        bool bootstrapOk = true;
        for (const noc::EntityHandle entity : bootstrap)
            bootstrapOk = bootstrapOk && entity.IsValid();

        if (!bootstrapOk)
        {
            for (const noc::EntityHandle entity : bootstrap)
            {
                if (entity.IsValid() && world.IsAlive(entity))
                    (void)world.DestroyEntity(entity);
            }

            NOC_LOG_ERROR(
                "Editor",
                "%s",
                "Failed to create Phase 14 bootstrap scene objects");
            return false;
        }

        cameraObject_ = world.CreateObject();
        if (!cameraObject_.IsValid())
        {
            NOC_LOG_ERROR("Editor", "Failed to create Phase 14 editor camera object");
            return false;
        }

        cameraRot_ = YawPitch(cameraYaw_, cameraPitch_);

        if (!world.AddName(cameraObject_, "Main Camera")
            || !world.SetLocalTRS(cameraObject_, cameraPos_, cameraRot_, noc::Vec3::One())
            || !world.SetCameraParams(fovY_, 16.0f / 9.0f, 0.05f, 500.0f)
            || !world.SetCameraFromObject(cameraObject_))
        {
            NOC_LOG_ERROR("Editor", "%s", "Failed to initialize Phase 14 editor camera components");
            world.DestroyObject(cameraObject_);
            cameraObject_ = noc::EntityHandle::Invalid();
            return false;
        }

        world.Update();

        if (!session.SetToolCamera(cameraObject_))
        {
            NOC_LOG_ERROR("Editor", "%s", "Failed to register editor camera as tool-owned");
            return false;
        }

        scenePrepared_ = true;
        NOC_LOG_INFO("Editor", "Phase 14 3D validation scene prepared (3 cubes + selectable ground + editor camera)");
        return true;
    }

    bool EditorViewportController::RegisterOverlayClass_()
    {
        HINSTANCE instance = GetModuleHandleW(nullptr);
        WNDCLASSEXW existing{};
        existing.cbSize = sizeof(existing);
        if (GetClassInfoExW(instance, kOverlayClass, &existing))
            return true;

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = &EditorViewportController::OverlayProc_;
        wc.hInstance = instance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = kOverlayClass;
        return RegisterClassExW(&wc) != 0;
    }

    bool EditorViewportController::Attach(
        noc::Engine& engine,
        noc::WinWindow& window,
        EditorShellV3& shell,
        EditorSession& session)
    {
        if (!scenePrepared_ && !PrepareScene(engine, session))
            return false;
        if (!RegisterOverlayClass_())
        {
            NOC_LOG_ERROR("Editor", "Phase 14 overlay class registration failed (err=%lu)", GetLastError());
            return false;
        }

        engine_ = &engine;
        window_ = &window;
        shell_ = &shell;
        session_ = &session;
        observedSessionVersion_ = session.StateVersion();
        topLevel_ = static_cast<HWND>(window.Handle());
        body_ = shell.ViewportBody();
        if (!topLevel_ || !body_)
        {
            NOC_LOG_ERROR(
                "Editor",
                "Viewport attach missing HWND (top=%p body=%p)",
                topLevel_,
                body_);
            return false;
        }

        if (!SetWindowSubclass(body_, &EditorViewportController::BodySubclassProc_, kSubclassIdBody,
            reinterpret_cast<DWORD_PTR>(this)))
        {
            NOC_LOG_ERROR("Editor", "Phase 14 viewport body subclass failed");
            return false;
        }

        // STATIC controls require SS_NOTIFY to participate reliably in mouse
        // interaction. The subclass also returns HTCLIENT explicitly below.
        renderHost_ = CreateWindowExW(0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP | SS_NOTIFY,
            0, 0, 1, 1, body_, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!renderHost_)
        {
            NOC_LOG_ERROR("Editor", "Phase 14 render host creation failed (err=%lu)", GetLastError());
            return false;
        }
        if (!SetWindowSubclass(renderHost_, &EditorViewportController::RenderHostSubclassProc_, kSubclassIdRenderHost,
            reinterpret_cast<DWORD_PTR>(this)))
        {
            NOC_LOG_ERROR("Editor", "Phase 14 render-host input subclass failed");
            return false;
        }

        overlay_ = CreateWindowExW(WS_EX_LAYERED, kOverlayClass, L"",
            WS_CHILD | WS_VISIBLE,
            0, 0, 1, 1, body_, nullptr, GetModuleHandleW(nullptr), this);
        if (!overlay_)
        {
            NOC_LOG_ERROR("Editor", "Phase 14 overlay child creation failed (err=%lu)", GetLastError());
            return false;
        }

        if (!SetLayeredWindowAttributes(overlay_, kOverlayKey, 0, LWA_COLORKEY))
            NOC_LOG_WARN("Editor", "Phase 14 overlay color-key setup failed (err=%lu)", GetLastError());
        SetWindowPos(overlay_, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

        LayoutChildren_();
        if (!renderAttached_)
        {
            RECT rc{};
            GetClientRect(body_, &rc);
            NOC_LOG_ERROR("Editor", "Phase 14 DX12 viewport target attach failed (body=%ldx%ld)", rc.right - rc.left, rc.bottom - rc.top);
            return false;
        }

        NOC_LOG_INFO("Editor", "Phase 14 viewport controller attached; frame-synchronized camera input active");
        return true;
    }

    void EditorViewportController::TickFrame()
    {
        if (session_)
        {
            session_->ValidateSelection();

            const uint64_t version =
                session_->StateVersion();
            if (version != observedSessionVersion_)
            {
                observedSessionVersion_ = version;
                RefreshDebugSelection_();

                if (overlay_)
                    InvalidateRect(
                        overlay_,
                        nullptr,
                        FALSE);
            }
        }

        UpdateCamera_();
    }

    void EditorViewportController::Shutdown()
    {
        if (engine_)
            engine_->ClearDebugSelectionBounds();
        if (body_)
            RemoveWindowSubclass(body_, &EditorViewportController::BodySubclassProc_, kSubclassIdBody);
        if (renderHost_)
            RemoveWindowSubclass(renderHost_, &EditorViewportController::RenderHostSubclassProc_, kSubclassIdRenderHost);
        if (GetCapture() == renderHost_)
            ReleaseCapture();

        cameraCapturing_ = false;
        gizmoDragging_ = false;
        pendingMouseDx_ = 0;
        pendingMouseDy_ = 0;
        dragEntity_ = noc::EntityHandle::Invalid();
        dragStartParent_ = noc::EntityHandle::Invalid();
        dragWorldOrientation_ = false;
        gizmoAxis_ = -1;
        session_ = nullptr;
        shell_ = nullptr;
        window_ = nullptr;
        topLevel_ = nullptr;
        body_ = nullptr;
        overlay_ = nullptr;
        renderHost_ = nullptr;
        engine_ = nullptr;
    }

    void EditorViewportController::LayoutChildren_()
    {
        if (!body_ || !renderHost_ || !overlay_ || !engine_)
            return;

        RECT rc{};
        GetClientRect(body_, &rc);
        const int width = (std::max)(0L, rc.right - rc.left);
        const int height = (std::max)(0L, rc.bottom - rc.top - kViewportControlStrip);

        MoveWindow(renderHost_, 0, kViewportControlStrip, width, height, TRUE);
        MoveWindow(overlay_, 0, kViewportControlStrip, width, height, TRUE);
        SetWindowPos(overlay_, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

        if (!renderAttached_)
        {
            if (width > 0 && height > 0)
                renderAttached_ = engine_->AttachRenderWindow(renderHost_, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
        }
        else if (!engine_->ResizeRenderWindow(static_cast<uint32_t>(width), static_cast<uint32_t>(height)))
        {
            NOC_LOG_WARN("Editor", "Viewport resize request failed (%dx%d)", width, height);
        }

        if (width > 0 && height > 0)
        {
            if (!engine_->GetWorld().SetCameraParams(
                    fovY_,
                    float(width) / float(height),
                    0.05f,
                    500.0f))
            {
                NOC_LOG_WARN(
                    "Editor",
                    "Viewport camera aspect update rejected (%dx%d)",
                    width,
                    height);
            }
        }
        InvalidateRect(overlay_, nullptr, FALSE);
    }

    void EditorViewportController::ApplyCameraTransform_()
    {
        if (!engine_ || !cameraObject_.IsValid()) return;
        cameraRot_ = YawPitch(cameraYaw_, cameraPitch_);
        if (!engine_->GetWorld().SetLocalTRS(
                cameraObject_,
                cameraPos_,
                cameraRot_,
                noc::Vec3::One()))
        {
            NOC_LOG_WARN("Editor", "%s", "Editor camera transform update rejected");
        }
    }

    void EditorViewportController::UpdateCamera_()
    {
        if (!cameraCapturing_ || !engine_)
        {
            pendingMouseDx_ = 0;
            pendingMouseDy_ = 0;
            return;
        }

        bool changed = false;
        if (pendingMouseDx_ != 0 || pendingMouseDy_ != 0)
        {
            cameraYaw_ += float(pendingMouseDx_) * 0.004f;
            cameraPitch_ = (std::clamp)(cameraPitch_ + float(pendingMouseDy_) * 0.004f, -1.45f, 1.45f);
            pendingMouseDx_ = 0;
            pendingMouseDy_ = 0;
            cameraRot_ = YawPitch(cameraYaw_, cameraPitch_);
            changed = true;
        }

        float dt = static_cast<float>(noc::GetTime().DeltaSeconds());
        if (!(dt > 0.0f) || dt > 0.05f) dt = 0.016f;
        const noc::Vec3 forward = noc::Normalize(noc::Rotate(cameraRot_, { 0,0,1 }));
        const noc::Vec3 right = noc::Normalize(noc::Rotate(cameraRot_, { 1,0,0 }));
        const noc::Vec3 up{ 0,1,0 };
        noc::Vec3 move = noc::Vec3::Zero();
        if (GetAsyncKeyState('W') & 0x8000) move = move + forward;
        if (GetAsyncKeyState('S') & 0x8000) move = move - forward;
        if (GetAsyncKeyState('D') & 0x8000) move = move + right;
        if (GetAsyncKeyState('A') & 0x8000) move = move - right;
        if (GetAsyncKeyState('E') & 0x8000) move = move + up;
        if (GetAsyncKeyState('Q') & 0x8000) move = move - up;

        if (noc::LengthSq(move) > 1e-5f)
        {
            const float fast = (GetAsyncKeyState(VK_SHIFT) & 0x8000) ? 3.0f : 1.0f;
            cameraPos_ = cameraPos_ + noc::Normalize(move) * (cameraSpeed_ * fast * dt);
            changed = true;
        }

        if (!changed)
            return;

        if (!engine_->GetWorld().SetLocalTRS(
                cameraObject_,
                cameraPos_,
                cameraRot_,
                noc::Vec3::One()))
        {
            NOC_LOG_WARN("Editor", "%s", "Interactive editor camera transform update rejected");
            return;
        }

        // Only the GDI gizmo depends on camera projection. Selection bounds and
        // grid are already part of the DX12 frame, so avoid invalidating the
        // layered overlay for every camera frame unless a gizmo is visible.
        const int tool = ActiveTool_();
        if (overlay_ && SelectedEntity_().IsValid() &&
            (tool == kMoveTool || tool == kRotateTool || tool == kScaleTool))
        {
            InvalidateRect(overlay_, nullptr, FALSE);
        }
    }

    bool EditorViewportController::Project_(const noc::Vec3& world, POINT& out) const
    {
        if (!renderHost_) return false;
        RECT rc{}; GetClientRect(renderHost_, &rc);
        const float w = float(rc.right - rc.left), h = float(rc.bottom - rc.top);
        if (w <= 1.0f || h <= 1.0f) return false;

        const noc::Vec3 right = noc::Normalize(noc::Rotate(cameraRot_, { 1,0,0 }));
        const noc::Vec3 up = noc::Normalize(noc::Rotate(cameraRot_, { 0,1,0 }));
        const noc::Vec3 forward = noc::Normalize(noc::Rotate(cameraRot_, { 0,0,1 }));
        const noc::Vec3 d = world - cameraPos_;
        const float vx = noc::Dot(d, right);
        const float vy = noc::Dot(d, up);
        const float vz = noc::Dot(d, forward);
        if (vz <= 0.05f) return false;

        const float yScale = 1.0f / std::tan(fovY_ * 0.5f);
        const float xScale = yScale / (w / h);
        const float ndcX = vx * xScale / vz;
        const float ndcY = vy * yScale / vz;
        out.x = LONG((ndcX * 0.5f + 0.5f) * w);
        out.y = LONG((0.5f - ndcY * 0.5f) * h);
        return true;
    }

    noc::Vec3 EditorViewportController::MakePickRay_(int x, int y) const
    {
        RECT rc{}; GetClientRect(renderHost_, &rc);
        const float w = float((std::max)(1L, rc.right - rc.left));
        const float h = float((std::max)(1L, rc.bottom - rc.top));
        const float ndcX = 2.0f * float(x) / w - 1.0f;
        const float ndcY = 1.0f - 2.0f * float(y) / h;
        const float yScale = 1.0f / std::tan(fovY_ * 0.5f);
        const float xScale = yScale / (w / h);
        const noc::Vec3 viewDir = noc::Normalize({ ndcX / xScale, ndcY / yScale, 1.0f });
        return noc::Normalize(noc::Rotate(cameraRot_, viewDir));
    }

    bool EditorViewportController::RayAabb_(const noc::Vec3& origin, const noc::Vec3& dir,
        const noc::AABB& box, float& outT) const
    {
        float tmin = 0.0f;
        float tmax = 1.0e30f;
        const float o[3] = { origin.x, origin.y, origin.z };
        const float d[3] = { dir.x, dir.y, dir.z };
        const float mn[3] = { box.min.x, box.min.y, box.min.z };
        const float mx[3] = { box.max.x, box.max.y, box.max.z };
        for (int i = 0; i < 3; ++i)
        {
            if (std::fabs(d[i]) < 1e-6f)
            {
                if (o[i] < mn[i] || o[i] > mx[i]) return false;
                continue;
            }
            float a = (mn[i] - o[i]) / d[i];
            float b = (mx[i] - o[i]) / d[i];
            if (a > b) std::swap(a, b);
            tmin = (std::max)(tmin, a);
            tmax = (std::min)(tmax, b);
            if (tmin > tmax) return false;
        }
        outT = tmin;
        return tmax >= 0.0f;
    }

    noc::EntityHandle EditorViewportController::PickWorldEntity_(
        const noc::Vec3& origin,
        const noc::Vec3& dir) const
    {
        if (!engine_ || !session_)
            return noc::EntityHandle::Invalid();

        const noc::World& world = engine_->GetWorld();
        noc::EntityHandle nearest = noc::EntityHandle::Invalid();
        float nearestT = 1.0e30f;

        for (uint32_t i = 0; i < world.EntityCapacity(); ++i)
        {
            const noc::EntityHandle entity = world.EntityAtIndex(i);
            if (!entity.IsValid()
                || session_->IsToolOwned(entity)
                || !world.HasTransform(entity))
            {
                continue;
            }

            const noc::RenderableComponent* renderable =
                world.GetRenderable(entity);
            if (!renderable || !renderable->enabled)
                continue;

            float t = 0.0f;
            if (RayAabb_(
                    origin,
                    dir,
                    renderable->worldBounds,
                    t)
                && t >= 0.0f
                && t < nearestT)
            {
                nearestT = t;
                nearest = entity;
            }
        }

        return nearest;
    }

    noc::EntityHandle EditorViewportController::SelectedEntity_() const
    {
        return session_
            ? session_->SelectedEntity()
            : noc::EntityHandle::Invalid();
    }

    void EditorViewportController::RefreshDebugSelection_()
    {
        if (!engine_)
            return;

        const noc::EntityHandle selected = SelectedEntity_();
        if (!selected.IsValid())
        {
            engine_->ClearDebugSelectionBounds();
            return;
        }

        const noc::RenderableComponent* renderable =
            engine_->GetWorld().GetRenderable(selected);
        if (!renderable)
        {
            engine_->ClearDebugSelectionBounds();
            return;
        }

        engine_->SetDebugSelection(
            renderable->localBounds,
            engine_->GetWorld().GetWorldMatrix(selected));
    }

    void EditorViewportController::SetSelectedEntity_(
        noc::EntityHandle entity,
        bool syncTree)
    {
        if (!session_)
            return;

        if (!entity.IsValid())
            session_->ClearSelection();
        else if (!session_->SetSelection(entity))
            return;

        gizmoAxis_ = -1;
        RefreshDebugSelection_();

        if (shell_)
        {
            if (syncTree)
                shell_->SyncSceneSelection();
            shell_->RefreshInspector();
        }

        if (overlay_)
            InvalidateRect(overlay_, nullptr, FALSE);
    }

    int EditorViewportController::ActiveTool_() const
    {
        return shell_ ? shell_->ActiveToolId() : kSelectTool;
    }

    bool EditorViewportController::BuildGizmoFrame_(
        noc::EntityHandle entity,
        bool worldOrientation,
        noc::Vec3& outPivot,
        noc::Vec3 outAxes[3]) const
    {
        if (!engine_
            || !outAxes
            || !entity.IsValid()
            || !engine_->GetWorld().IsAlive(entity)
            || !engine_->GetWorld().HasTransform(entity))
        {
            return false;
        }

        const noc::Mat4 world =
            engine_->GetWorld().GetWorldMatrix(entity);

        outPivot = MatrixTranslation(world);

        if (worldOrientation)
        {
            outAxes[0] = { 1.0f, 0.0f, 0.0f };
            outAxes[1] = { 0.0f, 1.0f, 0.0f };
            outAxes[2] = { 0.0f, 0.0f, 1.0f };
            return true;
        }

        for (int axis = 0; axis < 3; ++axis)
        {
            const noc::Vec3 column =
                MatrixColumn(world, axis);

            if (!noc::IsFiniteMath(column)
                || noc::LengthSq(column) <= 1.0e-10f)
            {
                return false;
            }

            outAxes[axis] =
                noc::Normalize(column);
        }

        return true;
    }

    bool EditorViewportController::WorldToLocalTRS_(
        noc::EntityHandle entity,
        const noc::Mat4& desiredWorld,
        noc::Vec3& outTranslation,
        noc::Quat& outRotation,
        noc::Vec3& outScale) const
    {
        if (!engine_
            || !entity.IsValid()
            || !engine_->GetWorld().IsAlive(entity)
            || !engine_->GetWorld().HasTransform(entity))
        {
            return false;
        }

        noc::Mat4 desiredLocal =
            desiredWorld;

        const noc::EntityHandle parent =
            engine_->GetWorld().ParentOf(entity);

        if (parent.IsValid())
        {
            const noc::Mat4 parentWorld =
                engine_->GetWorld().GetWorldMatrix(parent);
            noc::Mat4 inverseParent{};

            if (!noc::TryInverseAffine(
                    parentWorld,
                    inverseParent))
            {
                return false;
            }

            desiredLocal =
                noc::Mul(
                    inverseParent,
                    desiredWorld);
        }

        return noc::TryDecomposeTRS(
            desiredLocal,
            outTranslation,
            outRotation,
            outScale);
    }

    int EditorViewportController::HitGizmoAxis_(POINT p) const
    {
        const noc::EntityHandle selected =
            SelectedEntity_();
        if (!selected.IsValid())
            return -1;

        const int tool = ActiveTool_();
        if (tool != kMoveTool
            && tool != kRotateTool
            && tool != kScaleTool)
        {
            return -1;
        }

        const bool worldOrientation =
            tool != kScaleTool
            && session_
            && session_->Orientation()
                == TransformOrientation::World;

        noc::Vec3 pivot{};
        noc::Vec3 axes[3]{};
        if (!BuildGizmoFrame_(
                selected,
                worldOrientation,
                pivot,
                axes))
        {
            return -1;
        }

        POINT center{};
        if (!Project_(pivot, center))
            return -1;

        const float gizmoLength =
            GizmoWorldLength(
                renderHost_,
                cameraPos_,
                cameraRot_,
                fovY_,
                pivot);

        float best = kGizmoHitRadiusPixels;
        int bestAxis = -1;

        if (tool == kRotateTool)
        {
            constexpr float kTwoPi =
                6.28318530718f;

            for (int axis = 0; axis < 3; ++axis)
            {
                POINT previous{};
                bool previousValid = false;

                for (int segment = 0;
                     segment <= kGizmoRingSegments;
                     ++segment)
                {
                    const float angle =
                        kTwoPi
                        * float(segment)
                        / float(kGizmoRingSegments);

                    POINT current{};
                    const bool currentValid =
                        Project_(
                            pivot
                                + GizmoRingWorldVector(
                                    axes,
                                    axis,
                                    angle)
                                    * gizmoLength,
                            current);

                    if (currentValid
                        && previousValid)
                    {
                        const float distance =
                            PointSegmentDistance(
                                p,
                                previous,
                                current);

                        if (distance < best)
                        {
                            best = distance;
                            bestAxis = axis;
                        }
                    }

                    previous = current;
                    previousValid = currentValid;
                }
            }

            return bestAxis;
        }

        for (int axis = 0; axis < 3; ++axis)
        {
            POINT end{};
            if (!Project_(
                    pivot
                        + axes[axis] * gizmoLength,
                    end))
            {
                continue;
            }

            const float distance =
                PointSegmentDistance(
                    p,
                    center,
                    end);

            if (distance < best)
            {
                best = distance;
                bestAxis = axis;
            }
        }

        return bestAxis;
    }

    void EditorViewportController::BeginGizmoDrag_(
        int axis,
        POINT mouse)
    {
        const noc::EntityHandle selected =
            SelectedEntity_();

        if (axis < 0
            || axis > 2
            || !selected.IsValid()
            || !engine_)
        {
            return;
        }

        const noc::TransformComponent* transform =
            engine_->GetWorld().GetTransform(selected);
        if (!transform)
            return;

        const int tool =
            ActiveTool_();

        const bool worldOrientation =
            tool != kScaleTool
            && session_
            && session_->Orientation()
                == TransformOrientation::World;

        noc::Vec3 pivot{};
        noc::Vec3 axes[3]{};

        if (!BuildGizmoFrame_(
                selected,
                worldOrientation,
                pivot,
                axes))
        {
            return;
        }

        dragToolId_ = tool;
        dragEntity_ = selected;
        dragStartParent_ =
            engine_->GetWorld().ParentOf(selected);
        dragStartMouse_ = mouse;

        dragStartT_ =
            transform->localTranslation;
        dragStartR_ =
            transform->localRotation;
        dragStartS_ =
            transform->localScale;

        dragStartWorld_ =
            engine_->GetWorld().GetWorldMatrix(selected);
        dragStartPivot_ = pivot;
        dragStartAxes_[0] = axes[0];
        dragStartAxes_[1] = axes[1];
        dragStartAxes_[2] = axes[2];
        dragWorldOrientation_ =
            worldOrientation;

        gizmoDragging_ = true;
        gizmoAxis_ = axis;

        SetCapture(renderHost_);

        if (overlay_)
            InvalidateRect(
                overlay_,
                nullptr,
                FALSE);
    }

    void EditorViewportController::UpdateGizmoDrag_(
        POINT mouse)
    {
        if (!gizmoDragging_
            || !engine_
            || gizmoAxis_ < 0
            || !dragEntity_.IsValid()
            || !engine_->GetWorld().IsAlive(
                dragEntity_)
            || engine_->GetWorld().ParentOf(
                dragEntity_) != dragStartParent_)
        {
            return;
        }

        noc::Vec3 newTranslation =
            dragStartT_;
        noc::Quat newRotation =
            dragStartR_;
        noc::Vec3 newScale =
            dragStartS_;

        const noc::Vec3 unit[3] = {
            { 1,0,0 },
            { 0,1,0 },
            { 0,0,1 }
        };

        const noc::Vec3 axisWorld =
            dragStartAxes_[gizmoAxis_];

        const float gizmoLength =
            GizmoWorldLength(
                renderHost_,
                cameraPos_,
                cameraRot_,
                fovY_,
                dragStartPivot_);

        const int tool =
            dragToolId_;

        if (tool == kRotateTool)
        {
            POINT center{};
            if (!Project_(
                    dragStartPivot_,
                    center))
            {
                return;
            }

            const float sx =
                float(dragStartMouse_.x - center.x);
            const float sy =
                float(dragStartMouse_.y - center.y);
            const float cx =
                float(mouse.x - center.x);
            const float cy =
                float(mouse.y - center.y);

            const float startLenSq =
                sx * sx + sy * sy;
            const float currentLenSq =
                cx * cx + cy * cy;

            if (startLenSq < 16.0f
                || currentLenSq < 16.0f)
            {
                return;
            }

            float angle =
                std::atan2(
                    sx * cy - sy * cx,
                    sx * cx + sy * cy);

            noc::Vec3 localU{};
            noc::Vec3 localV{};
            GizmoRingBasis(
                gizmoAxis_,
                localU,
                localV);

            const noc::Vec3 worldU =
                ExpandGizmoBasisVector(
                    dragStartAxes_,
                    localU);
            const noc::Vec3 worldV =
                ExpandGizmoBasisVector(
                    dragStartAxes_,
                    localV);

            POINT uScreen{};
            POINT vScreen{};

            if (Project_(
                    dragStartPivot_
                        + worldU * gizmoLength,
                    uScreen)
                && Project_(
                    dragStartPivot_
                        + worldV * gizmoLength,
                    vScreen))
            {
                const float ux =
                    float(uScreen.x - center.x);
                const float uy =
                    float(uScreen.y - center.y);
                const float vx =
                    float(vScreen.x - center.x);
                const float vy =
                    float(vScreen.y - center.y);

                if (ux * vy - uy * vx < 0.0f)
                    angle = -angle;
            }

            if (dragWorldOrientation_)
            {
                const noc::Quat delta =
                    AxisAngle(
                        axisWorld,
                        angle);

                noc::Mat4 desiredWorld =
                    dragStartWorld_;

                for (int column = 0;
                     column < 3;
                     ++column)
                {
                    const noc::Vec3 rotated =
                        noc::Rotate(
                            delta,
                            MatrixColumn(
                                dragStartWorld_,
                                column));

                    noc::M(
                        desiredWorld,
                        0,
                        column) = rotated.x;
                    noc::M(
                        desiredWorld,
                        1,
                        column) = rotated.y;
                    noc::M(
                        desiredWorld,
                        2,
                        column) = rotated.z;
                }

                if (!WorldToLocalTRS_(
                        dragEntity_,
                        desiredWorld,
                        newTranslation,
                        newRotation,
                        newScale))
                {
                    return;
                }
            }
            else
            {
                newRotation =
                    NormalizeQuat(
                        MulQuat(
                            dragStartR_,
                            AxisAngle(
                                unit[gizmoAxis_],
                                angle)));
            }
        }
        else
        {
            POINT startScreen{};
            POINT axisScreen{};

            if (!Project_(
                    dragStartPivot_,
                    startScreen)
                || !Project_(
                    dragStartPivot_
                        + axisWorld * gizmoLength,
                    axisScreen))
            {
                return;
            }

            const float vx =
                float(
                    axisScreen.x
                    - startScreen.x);
            const float vy =
                float(
                    axisScreen.y
                    - startScreen.y);
            const float length =
                std::sqrt(vx * vx + vy * vy);

            if (length < 1.0f)
                return;

            const float dx =
                float(
                    mouse.x
                    - dragStartMouse_.x);
            const float dy =
                float(
                    mouse.y
                    - dragStartMouse_.y);

            const float signedPixels =
                (dx * vx + dy * vy)
                / length;

            if (tool == kMoveTool)
            {
                const float worldDelta =
                    (signedPixels / length)
                    * gizmoLength;

                noc::Mat4 desiredWorld =
                    dragStartWorld_;

                const noc::Vec3 desiredPosition =
                    dragStartPivot_
                    + axisWorld * worldDelta;

                noc::M(
                    desiredWorld,
                    0,
                    3) = desiredPosition.x;
                noc::M(
                    desiredWorld,
                    1,
                    3) = desiredPosition.y;
                noc::M(
                    desiredWorld,
                    2,
                    3) = desiredPosition.z;

                if (!WorldToLocalTRS_(
                        dragEntity_,
                        desiredWorld,
                        newTranslation,
                        newRotation,
                        newScale))
                {
                    return;
                }
            }
            else if (tool == kScaleTool)
            {
                float* component =
                    gizmoAxis_ == 0
                        ? &newScale.x
                        : (gizmoAxis_ == 1
                            ? &newScale.y
                            : &newScale.z);

                const float start =
                    gizmoAxis_ == 0
                        ? dragStartS_.x
                        : (gizmoAxis_ == 1
                            ? dragStartS_.y
                            : dragStartS_.z);

                const float reference =
                    (std::max)(
                        std::fabs(start),
                        0.25f);

                *component =
                    (std::max)(
                        0.05f,
                        start
                            + (signedPixels / length)
                                * reference);
            }
            else
            {
                return;
            }
        }

        if (!engine_->GetWorld().SetLocalTRS(
                dragEntity_,
                newTranslation,
                newRotation,
                newScale))
        {
            NOC_LOG_WARN(
                "Editor",
                "Gizmo transform update rejected (entity=%u:%u)",
                dragEntity_.index,
                dragEntity_.generation);
            return;
        }

        engine_->GetWorld().Update();
        RefreshDebugSelection_();

        if (overlay_)
            InvalidateRect(
                overlay_,
                nullptr,
                FALSE);
    }

    void EditorViewportController::EndGizmoDrag_()
    {
        if (!gizmoDragging_)
            return;

        const noc::EntityHandle entity =
            dragEntity_;

        gizmoDragging_ = false;
        gizmoAxis_ = -1;
        dragToolId_ = 0;
        dragEntity_ =
            noc::EntityHandle::Invalid();

        if (GetCapture() == renderHost_)
            ReleaseCapture();

        bool recorded = false;
        bool changed = false;

        if (engine_
            && session_
            && entity.IsValid()
            && engine_->GetWorld().IsAlive(entity))
        {
            const noc::TransformComponent* finalTransform =
                engine_->GetWorld().GetTransform(entity);

            if (finalTransform)
            {
                changed =
                    dragStartT_.x
                        != finalTransform->localTranslation.x
                    || dragStartT_.y
                        != finalTransform->localTranslation.y
                    || dragStartT_.z
                        != finalTransform->localTranslation.z
                    || dragStartR_.x
                        != finalTransform->localRotation.x
                    || dragStartR_.y
                        != finalTransform->localRotation.y
                    || dragStartR_.z
                        != finalTransform->localRotation.z
                    || dragStartR_.w
                        != finalTransform->localRotation.w
                    || dragStartS_.x
                        != finalTransform->localScale.x
                    || dragStartS_.y
                        != finalTransform->localScale.y
                    || dragStartS_.z
                        != finalTransform->localScale.z;

                if (changed)
                {
                    auto context =
                        session_->CommandContext();

                    try
                    {
                        auto command =
                            std::make_unique<
                                SetTransformTRSCommand>();

                        if (command->InitExplicit(
                                context,
                                entity,
                                dragStartT_,
                                dragStartR_,
                                dragStartS_,
                                finalTransform->localTranslation,
                                finalTransform->localRotation,
                                finalTransform->localScale))
                        {
                            recorded =
                                session_->History().RecordExecuted(
                                    context,
                                    std::move(command));
                        }
                    }
                    catch (const std::bad_alloc&)
                    {
                        recorded = false;
                    }

                    if (!recorded)
                    {
                        (void)engine_->GetWorld().SetLocalTRS(
                            entity,
                            dragStartT_,
                            dragStartR_,
                            dragStartS_);
                        engine_->GetWorld().Update();
                    }
                    else
                    {
                        session_->SetSceneDirty();
                    }
                }
            }
        }

        dragStartParent_ =
            noc::EntityHandle::Invalid();
        dragWorldOrientation_ = false;

        RefreshDebugSelection_();

        if (shell_)
            shell_->RefreshInspector();

        if (overlay_)
            InvalidateRect(
                overlay_,
                nullptr,
                FALSE);
    }

    void EditorViewportController::CancelGizmoDrag_()
    {
        if (!gizmoDragging_)
            return;

        const noc::EntityHandle entity =
            dragEntity_;

        gizmoDragging_ = false;
        gizmoAxis_ = -1;
        dragToolId_ = 0;
        dragEntity_ =
            noc::EntityHandle::Invalid();

        if (GetCapture() == renderHost_)
            ReleaseCapture();

        if (engine_
            && entity.IsValid()
            && engine_->GetWorld().IsAlive(entity)
            && engine_->GetWorld().HasTransform(entity))
        {
            (void)engine_->GetWorld().SetLocalTRS(
                entity,
                dragStartT_,
                dragStartR_,
                dragStartS_);
            engine_->GetWorld().Update();
        }

        dragStartParent_ =
            noc::EntityHandle::Invalid();
        dragWorldOrientation_ = false;

        RefreshDebugSelection_();

        if (shell_)
            shell_->RefreshInspector();

        if (overlay_)
            InvalidateRect(
                overlay_,
                nullptr,
                FALSE);
    }

    void EditorViewportController::HandleViewportMouse_(UINT msg, WPARAM wParam, LPARAM lParam)
    {
        const POINT mouse{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        switch (msg)
        {
        case WM_RBUTTONDOWN:
            SetFocus(renderHost_);
            cameraCapturing_ = true;
            lastMouse_ = mouse;
            pendingMouseDx_ = 0;
            pendingMouseDy_ = 0;
            if (gizmoAxis_ != -1)
            {
                gizmoAxis_ = -1;
                if (overlay_) InvalidateRect(overlay_, nullptr, FALSE);
            }
            SetCapture(renderHost_);
            break;
        case WM_RBUTTONUP:
            cameraCapturing_ = false;
            pendingMouseDx_ = 0;
            pendingMouseDy_ = 0;
            if (GetCapture() == renderHost_) ReleaseCapture();
            break;
        case WM_LBUTTONDOWN:
        {
            SetFocus(renderHost_);
            const int tool = ActiveTool_();
            if (SelectedEntity_().IsValid()
                && (tool == kMoveTool
                    || tool == kRotateTool
                    || tool == kScaleTool))
            {
                const int axis = HitGizmoAxis_(mouse);
                if (axis >= 0) { BeginGizmoDrag_(axis, mouse); break; }
            }
            SetSelectedEntity_(
                PickWorldEntity_(
                    cameraPos_,
                    MakePickRay_(mouse.x, mouse.y)));
            break;
        }
        case WM_LBUTTONUP:
            EndGizmoDrag_();
            break;
        case WM_MOUSEMOVE:
            if (cameraCapturing_)
            {
                pendingMouseDx_ += mouse.x - lastMouse_.x;
                pendingMouseDy_ += mouse.y - lastMouse_.y;
                lastMouse_ = mouse;
            }
            else if (gizmoDragging_)
            {
                UpdateGizmoDrag_(mouse);
            }
            else
            {
                const int tool = ActiveTool_();
                const bool transformTool = tool == kMoveTool || tool == kRotateTool || tool == kScaleTool;
                const int hoverAxis = transformTool ? HitGizmoAxis_(mouse) : -1;
                if (hoverAxis != gizmoAxis_)
                {
                    gizmoAxis_ = hoverAxis;
                    if (overlay_) InvalidateRect(overlay_, nullptr, FALSE);
                }
                TRACKMOUSEEVENT t{ sizeof(t), TME_LEAVE, renderHost_, 0 };
                TrackMouseEvent(&t);
            }
            break;
        case WM_MOUSELEAVE:
            if (!cameraCapturing_ && !gizmoDragging_)
            {
                // -2 forces the first mouse move after a toolbar change to repaint
                // the overlay even when no axis is under the cursor.
                gizmoAxis_ = -2;
                if (overlay_) InvalidateRect(overlay_, nullptr, FALSE);
            }
            break;
        case WM_MOUSEWHEEL:
            cameraSpeed_ = (std::clamp)(cameraSpeed_ + float(GET_WHEEL_DELTA_WPARAM(wParam)) / 120.0f, 1.0f, 30.0f); break;
        case WM_CAPTURECHANGED:
            cameraCapturing_ = false;
            pendingMouseDx_ = 0;
            pendingMouseDy_ = 0;
            EndGizmoDrag_();
            break;
        }
    }

    void EditorViewportController::PaintOverlay_(
        HDC dc,
        const RECT& rc)
    {
        HBRUSH keyBrush =
            CreateSolidBrush(kOverlayKey);
        FillRect(dc, &rc, keyBrush);
        DeleteObject(keyBrush);

        const noc::EntityHandle selected =
            SelectedEntity_();
        if (!selected.IsValid())
            return;

        const int tool =
            ActiveTool_();
        if (tool != kMoveTool
            && tool != kRotateTool
            && tool != kScaleTool)
        {
            return;
        }

        const bool worldOrientation =
            tool != kScaleTool
            && session_
            && session_->Orientation()
                == TransformOrientation::World;

        noc::Vec3 pivot{};
        noc::Vec3 axes[3]{};

        if (!BuildGizmoFrame_(
                selected,
                worldOrientation,
                pivot,
                axes))
        {
            return;
        }

        const COLORREF colors[3] = {
            RGB(224,75,75),
            RGB(74,207,112),
            RGB(73,139,239)
        };
        constexpr COLORREF highlight =
            RGB(255,236,130);

        const float gizmoLength =
            GizmoWorldLength(
                renderHost_,
                cameraPos_,
                cameraRot_,
                fovY_,
                pivot);

        POINT center{};
        if (!Project_(pivot, center))
            return;

        if (tool == kRotateTool)
        {
            constexpr float kTwoPi =
                6.28318530718f;

            for (int axis = 0;
                 axis < 3;
                 ++axis)
            {
                const bool active =
                    gizmoAxis_ == axis;
                const COLORREF color =
                    active
                        ? highlight
                        : colors[axis];

                HPEN pen =
                    CreatePen(
                        PS_SOLID,
                        active ? 4 : 3,
                        color);
                HGDIOBJ oldPen =
                    SelectObject(dc, pen);

                POINT previous{};
                bool previousValid = false;

                for (int segment = 0;
                     segment <= kGizmoRingSegments;
                     ++segment)
                {
                    const float angle =
                        kTwoPi
                        * float(segment)
                        / float(kGizmoRingSegments);

                    POINT current{};
                    const bool currentValid =
                        Project_(
                            pivot
                                + GizmoRingWorldVector(
                                    axes,
                                    axis,
                                    angle)
                                    * gizmoLength,
                            current);

                    if (currentValid
                        && previousValid)
                    {
                        MoveToEx(
                            dc,
                            previous.x,
                            previous.y,
                            nullptr);
                        LineTo(
                            dc,
                            current.x,
                            current.y);
                    }

                    previous = current;
                    previousValid = currentValid;
                }

                SelectObject(dc, oldPen);
                DeleteObject(pen);
            }

            return;
        }

        for (int axis = 0;
             axis < 3;
             ++axis)
        {
            POINT end{};
            if (!Project_(
                    pivot
                        + axes[axis] * gizmoLength,
                    end))
            {
                continue;
            }

            const bool active =
                gizmoAxis_ == axis;
            const COLORREF color =
                active
                    ? highlight
                    : colors[axis];

            DrawLine(
                dc,
                center,
                end,
                color,
                active ? 4 : 3);

            if (tool == kMoveTool)
            {
                DrawArrowHead(
                    dc,
                    center,
                    end,
                    color);
            }
            else
            {
                DrawScaleHandle(
                    dc,
                    end,
                    color);
            }
        }
    }

    LRESULT CALLBACK EditorViewportController::OverlayProc_(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        auto* self = reinterpret_cast<EditorViewportController*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (msg == WM_NCCREATE)
        {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            self = static_cast<EditorViewportController*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            return TRUE;
        }
        if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);

        switch (msg)
        {
        case WM_NCHITTEST:
            return HTTRANSPARENT;
        case WM_ERASEBKGND: return 1;
        case WM_PAINT:
        {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(hwnd, &ps);
            RECT rc{}; GetClientRect(hwnd, &rc);
            self->PaintOverlay_(dc, rc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    LRESULT CALLBACK EditorViewportController::BodySubclassProc_(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
        UINT_PTR subclassId, DWORD_PTR refData)
    {
        (void)subclassId;
        auto* self = reinterpret_cast<EditorViewportController*>(refData);
        const LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
        if (self && msg == WM_SIZE)
            self->LayoutChildren_();
        return result;
    }

    LRESULT CALLBACK EditorViewportController::RenderHostSubclassProc_(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
        UINT_PTR subclassId, DWORD_PTR refData)
    {
        (void)subclassId;
        auto* self = reinterpret_cast<EditorViewportController*>(refData);
        if (!self)
            return DefSubclassProc(hwnd, msg, wParam, lParam);

        switch (msg)
        {
        case WM_NCHITTEST:
            // Do not let the STATIC control default to transparent hit-testing.
            // This HWND is the authoritative Phase 14 viewport input surface.
            return HTCLIENT;
        case WM_RBUTTONDOWN: case WM_RBUTTONUP:
        case WM_LBUTTONDOWN: case WM_LBUTTONUP:
        case WM_MOUSEMOVE: case WM_MOUSELEAVE: case WM_MOUSEWHEEL: case WM_CAPTURECHANGED:
            self->HandleViewportMouse_(msg, wParam, lParam);
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE
                && self->gizmoDragging_)
            {
                self->CancelGizmoDrag_();
                return 0;
            }
            break;
        case WM_SETFOCUS:
            if (self->overlay_) InvalidateRect(self->overlay_, nullptr, FALSE);
            return DefSubclassProc(hwnd, msg, wParam, lParam);
        case WM_KILLFOCUS:
            self->cameraCapturing_ = false;
            self->pendingMouseDx_ = 0;
            self->pendingMouseDy_ = 0;
            self->EndGizmoDrag_();
            return 0;
        }
        return DefSubclassProc(hwnd, msg, wParam, lParam);
    }

}
