#include "EditorViewportController.h"

#include "EditorShellV3.h"
#include "EditorTheme.h"
#include "EditorIconRenderer.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <CommCtrl.h>
#include <Windowsx.h>

#include "Core/Clock.h"
#include "Core/Log.h"
#include "Runtime/Engine.h"
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
        constexpr int kHierarchyRowHeight = 24;

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

        void DrawLine(HDC dc, POINT a, POINT b, COLORREF color, int width = 1)
        {
            HPEN pen = CreatePen(PS_SOLID, width, color);
            HGDIOBJ old = SelectObject(dc, pen);
            MoveToEx(dc, a.x, a.y, nullptr);
            LineTo(dc, b.x, b.y);
            SelectObject(dc, old);
            DeleteObject(pen);
        }

        COLORREF BlendColor(COLORREF a, COLORREF b, int bPercent)
        {
            const int aPercent = 100 - bPercent;
            return RGB(
                (GetRValue(a) * aPercent + GetRValue(b) * bPercent) / 100,
                (GetGValue(a) * aPercent + GetGValue(b) * bPercent) / 100,
                (GetBValue(a) * aPercent + GetBValue(b) * bPercent) / 100);
        }

        void FillColor(HDC dc, const RECT& rc, COLORREF color)
        {
            HBRUSH brush = CreateSolidBrush(color);
            FillRect(dc, &rc, brush);
            DeleteObject(brush);
        }

        void DrawHierarchyText(HDC dc, const wchar_t* text, RECT rc, COLORREF color, HFONT font)
        {
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, color);
            HGDIOBJ oldFont = font ? SelectObject(dc, font) : nullptr;
            DrawTextW(dc, text, -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            if (oldFont) SelectObject(dc, oldFont);
        }
    }

    bool EditorViewportController::PrepareScene(noc::Engine& engine)
    {
        if (scenePrepared_)
            return true;

        engine_ = &engine;
        auto& world = engine.GetWorld();
        const noc::ResourceHandle logicalMesh = engine.Resources().RequestBinary("Meshes/triangle.nmsh");

        auto createValidationObject = [&](int index, const noc::Vec3& t, const noc::Quat& r,
            const noc::Vec3& s, bool selectable) -> bool
        {
            ValidationObject& object = validationObjects_[index];
            object.handle = world.CreateObject();
            if (!object.handle.IsValid())
                return false;
            object.t = t;
            object.r = r;
            object.s = s;
            object.selectable = selectable;
            object.localBounds = { noc::Vec3(-1.0f, -1.0f, -1.0f), noc::Vec3(1.0f, 1.0f, 1.0f) };
            world.SetLocalTRS(object.handle, object.t, object.r, object.s);
            world.SetRenderable(object.handle, logicalMesh, object.localBounds);
            return true;
        };

        // Design choice (not directly from the book): deterministic Phase 14
        // viewport scene. Ground is selectable now because it is exposed as a real
        // hierarchy object alongside the three validation cubes.
        if (!createValidationObject(0, { 0.0f, -0.15f, 6.0f }, noc::Quat::Identity(), { 1.0f, 1.0f, 1.0f }, true) ||
            !createValidationObject(1, { -2.5f, -0.10f, 9.0f }, AxisAngle({ 0,1,0 }, 0.38f), { 0.8f, 1.05f, 0.8f }, true) ||
            !createValidationObject(2, { 2.4f, -0.35f, 11.0f }, AxisAngle({ 0,1,0 }, -0.52f), { 1.1f, 0.8f, 1.1f }, true) ||
            !createValidationObject(3, { 0.0f, -1.25f, 9.0f }, noc::Quat::Identity(), { 6.5f, 0.10f, 7.5f }, true))
        {
            NOC_LOG_ERROR("Editor", "Failed to create Phase 14 validation scene objects");
            return false;
        }

        cameraObject_ = world.CreateObject();
        if (!cameraObject_.IsValid())
        {
            NOC_LOG_ERROR("Editor", "Failed to create Phase 14 editor camera object");
            return false;
        }

        cameraRot_ = YawPitch(cameraYaw_, cameraPitch_);
        world.SetLocalTRS(cameraObject_, cameraPos_, cameraRot_, noc::Vec3::One());
        world.SetCameraParams(fovY_, 16.0f / 9.0f, 0.05f, 500.0f);
        world.SetCameraFromObject(cameraObject_);
        world.Update();

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

    bool EditorViewportController::Attach(noc::Engine& engine, noc::WinWindow& window, EditorShellV3& shell)
    {
        if (!scenePrepared_ && !PrepareScene(engine))
            return false;
        if (!RegisterOverlayClass_())
        {
            NOC_LOG_ERROR("Editor", "Phase 14 overlay class registration failed (err=%lu)", GetLastError());
            return false;
        }

        engine_ = &engine;
        window_ = &window;
        shell_ = &shell;
        topLevel_ = static_cast<HWND>(window.Handle());
        body_ = shell.ViewportBody();
        sceneTree_ = shell.SceneTree();
        if (!topLevel_ || !body_ || !sceneTree_)
        {
            NOC_LOG_ERROR("Editor", "Phase 14 viewport attach missing HWND (top=%p body=%p tree=%p)", topLevel_, body_, sceneTree_);
            return false;
        }

        if (!SetWindowSubclass(body_, &EditorViewportController::BodySubclassProc_, kSubclassIdBody,
            reinterpret_cast<DWORD_PTR>(this)))
        {
            NOC_LOG_ERROR("Editor", "Phase 14 viewport body subclass failed");
            return false;
        }
        SetWindowSubclass(topLevel_, &EditorViewportController::MainSubclassProc_, kSubclassIdMain,
            reinterpret_cast<DWORD_PTR>(this));
        SetWindowSubclass(sceneTree_, &EditorViewportController::SceneTreeSubclassProc_, kSubclassIdTree,
            reinterpret_cast<DWORD_PTR>(this));

        renderHost_ = CreateWindowExW(0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
            0, 0, 1, 1, body_, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!renderHost_)
        {
            NOC_LOG_ERROR("Editor", "Phase 14 render host creation failed (err=%lu)", GetLastError());
            return false;
        }

        overlay_ = CreateWindowExW(WS_EX_LAYERED, kOverlayClass, L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
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

        hierarchySelectedRow_ = 0;
        InvalidateRect(sceneTree_, nullptr, FALSE);
        SetTimer(topLevel_, kTimerId, 16, nullptr);
        NOC_LOG_INFO("Editor", "Phase 14 viewport controller attached; explicit scene hierarchy bridge active");
        return true;
    }

    void EditorViewportController::Shutdown()
    {
        if (topLevel_)
        {
            KillTimer(topLevel_, kTimerId);
            RemoveWindowSubclass(topLevel_, &EditorViewportController::MainSubclassProc_, kSubclassIdMain);
        }
        if (body_)
            RemoveWindowSubclass(body_, &EditorViewportController::BodySubclassProc_, kSubclassIdBody);
        if (sceneTree_)
            RemoveWindowSubclass(sceneTree_, &EditorViewportController::SceneTreeSubclassProc_, kSubclassIdTree);
        if (GetCapture() == overlay_)
            ReleaseCapture();

        cameraCapturing_ = false;
        gizmoDragging_ = false;
        selectedIndex_ = -1;
        dragObjectIndex_ = -1;
        hierarchySelectedRow_ = 0;
        hierarchyHoverRow_ = -1;
        shell_ = nullptr;
        window_ = nullptr;
        topLevel_ = nullptr;
        body_ = nullptr;
        overlay_ = nullptr;
        renderHost_ = nullptr;
        sceneTree_ = nullptr;
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
            engine_->GetWorld().SetCameraParams(fovY_, float(width) / float(height), 0.05f, 500.0f);
        InvalidateRect(overlay_, nullptr, FALSE);
    }

    void EditorViewportController::ApplyCameraTransform_()
    {
        if (!engine_ || !cameraObject_.IsValid()) return;
        cameraRot_ = YawPitch(cameraYaw_, cameraPitch_);
        engine_->GetWorld().SetLocalTRS(cameraObject_, cameraPos_, cameraRot_, noc::Vec3::One());
    }

    void EditorViewportController::UpdateCamera_()
    {
        if (!cameraCapturing_ || !engine_)
            return;

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
            ApplyCameraTransform_();
        }
        InvalidateRect(overlay_, nullptr, FALSE);
    }

    bool EditorViewportController::Project_(const noc::Vec3& world, POINT& out) const
    {
        if (!overlay_) return false;
        RECT rc{}; GetClientRect(overlay_, &rc);
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
        RECT rc{}; GetClientRect(overlay_, &rc);
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

    noc::AABB EditorViewportController::ValidationWorldBounds_(int index) const
    {
        if (index < 0 || index >= kValidationObjectCount)
            return {};
        const ValidationObject& object = validationObjects_[index];
        return noc::TransformAabb(object.localBounds, noc::TRS(object.t, object.r, object.s));
    }

    int EditorViewportController::PickValidationObject_(const noc::Vec3& origin, const noc::Vec3& dir) const
    {
        int nearestIndex = -1;
        float nearestT = 1.0e30f;
        for (int i = 0; i < kValidationObjectCount; ++i)
        {
            const ValidationObject& object = validationObjects_[i];
            if (!object.selectable || !object.handle.IsValid()) continue;
            float t = 0.0f;
            if (RayAabb_(origin, dir, ValidationWorldBounds_(i), t) && t >= 0.0f && t < nearestT)
            {
                nearestT = t;
                nearestIndex = i;
            }
        }
        return nearestIndex;
    }

    void EditorViewportController::SetSelectedIndex_(int index, bool syncTree)
    {
        if (index < 0 || index >= kValidationObjectCount || !validationObjects_[index].selectable)
            index = -1;
        selectedIndex_ = index;

        if (syncTree)
        {
            if (selectedIndex_ >= 0)
            {
                hierarchyObjectsExpanded_ = true;
                hierarchySelectedRow_ = 2 + selectedIndex_;
            }
            else
            {
                hierarchySelectedRow_ = 0;
            }
            if (sceneTree_) InvalidateRect(sceneTree_, nullptr, FALSE);
        }
        if (overlay_) InvalidateRect(overlay_, nullptr, FALSE);
    }

    int EditorViewportController::ActiveTool_() const
    {
        return shell_ ? shell_->ActiveToolId() : kSelectTool;
    }

    int EditorViewportController::HitGizmoAxis_(POINT p) const
    {
        if (selectedIndex_ < 0) return -1;
        const ValidationObject& object = validationObjects_[selectedIndex_];
        POINT center{};
        if (!Project_(object.t, center)) return -1;
        const noc::Vec3 axes[3] = { {1,0,0},{0,1,0},{0,0,1} };
        float best = 9.0f;
        int bestAxis = -1;
        for (int i = 0; i < 3; ++i)
        {
            POINT end{};
            const noc::Vec3 axis = noc::Rotate(object.r, axes[i]);
            if (!Project_(object.t + axis * 1.5f, end)) continue;
            const float dist = PointSegmentDistance(p, center, end);
            if (dist < best) { best = dist; bestAxis = i; }
        }
        return bestAxis;
    }

    void EditorViewportController::BeginGizmoDrag_(int axis, POINT mouse)
    {
        if (axis < 0 || axis > 2 || selectedIndex_ < 0) return;
        gizmoDragging_ = true;
        gizmoAxis_ = axis;
        dragObjectIndex_ = selectedIndex_;
        dragStartMouse_ = mouse;
        const ValidationObject& object = validationObjects_[dragObjectIndex_];
        dragStartT_ = object.t;
        dragStartR_ = object.r;
        dragStartS_ = object.s;
        SetCapture(overlay_);
    }

    void EditorViewportController::UpdateGizmoDrag_(POINT mouse)
    {
        if (!gizmoDragging_ || !engine_ || gizmoAxis_ < 0 || dragObjectIndex_ < 0 || dragObjectIndex_ >= kValidationObjectCount)
            return;

        ValidationObject& object = validationObjects_[dragObjectIndex_];
        const noc::Vec3 unit[3] = { {1,0,0},{0,1,0},{0,0,1} };
        const noc::Vec3 axisWorld = noc::Rotate(dragStartR_, unit[gizmoAxis_]);
        POINT a{}, b{};
        if (!Project_(dragStartT_, a) || !Project_(dragStartT_ + axisWorld * 1.5f, b)) return;
        const float vx = float(b.x - a.x), vy = float(b.y - a.y);
        const float len = std::sqrt(vx * vx + vy * vy);
        if (len < 1.0f) return;
        const float dx = float(mouse.x - dragStartMouse_.x);
        const float dy = float(mouse.y - dragStartMouse_.y);
        const float signedPixels = (dx * vx + dy * vy) / len;

        const int tool = ActiveTool_();
        if (tool == kMoveTool)
            object.t = dragStartT_ + axisWorld * (signedPixels * 0.02f);
        else if (tool == kScaleTool)
        {
            object.s = dragStartS_;
            float* component = gizmoAxis_ == 0 ? &object.s.x : (gizmoAxis_ == 1 ? &object.s.y : &object.s.z);
            const float start = gizmoAxis_ == 0 ? dragStartS_.x : (gizmoAxis_ == 1 ? dragStartS_.y : dragStartS_.z);
            *component = (std::max)(0.05f, start + signedPixels * 0.01f);
        }
        else if (tool == kRotateTool)
            object.r = NormalizeQuat(MulQuat(AxisAngle(axisWorld, signedPixels * 0.01f), dragStartR_));

        engine_->GetWorld().SetLocalTRS(object.handle, object.t, object.r, object.s);
        engine_->GetWorld().Update();
        InvalidateRect(overlay_, nullptr, FALSE);
    }

    void EditorViewportController::EndGizmoDrag_()
    {
        if (!gizmoDragging_) return;
        gizmoDragging_ = false;
        gizmoAxis_ = -1;
        dragObjectIndex_ = -1;
        if (GetCapture() == overlay_) ReleaseCapture();
        if (overlay_) InvalidateRect(overlay_, nullptr, FALSE);
    }

    void EditorViewportController::HandleOverlayMouse_(UINT msg, WPARAM wParam, LPARAM lParam)
    {
        const POINT mouse{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        switch (msg)
        {
        case WM_RBUTTONDOWN:
            SetFocus(overlay_); cameraCapturing_ = true; lastMouse_ = mouse; SetCapture(overlay_); break;
        case WM_RBUTTONUP:
            cameraCapturing_ = false; if (GetCapture() == overlay_) ReleaseCapture(); break;
        case WM_LBUTTONDOWN:
        {
            SetFocus(overlay_);
            const int tool = ActiveTool_();
            if (selectedIndex_ >= 0 && (tool == kMoveTool || tool == kRotateTool || tool == kScaleTool))
            {
                const int axis = HitGizmoAxis_(mouse);
                if (axis >= 0) { BeginGizmoDrag_(axis, mouse); break; }
            }
            SetSelectedIndex_(PickValidationObject_(cameraPos_, MakePickRay_(mouse.x, mouse.y)));
            break;
        }
        case WM_LBUTTONUP: EndGizmoDrag_(); break;
        case WM_MOUSEMOVE:
            if (cameraCapturing_)
            {
                const int dx = mouse.x - lastMouse_.x;
                const int dy = mouse.y - lastMouse_.y;
                lastMouse_ = mouse;
                cameraYaw_ += float(dx) * 0.004f;
                cameraPitch_ = (std::clamp)(cameraPitch_ + float(dy) * 0.004f, -1.45f, 1.45f);
                ApplyCameraTransform_();
                InvalidateRect(overlay_, nullptr, FALSE);
            }
            else if (gizmoDragging_) UpdateGizmoDrag_(mouse);
            break;
        case WM_MOUSEWHEEL:
            cameraSpeed_ = (std::clamp)(cameraSpeed_ + float(GET_WHEEL_DELTA_WPARAM(wParam)) / 120.0f, 1.0f, 30.0f); break;
        case WM_CAPTURECHANGED:
            cameraCapturing_ = false; gizmoDragging_ = false; gizmoAxis_ = -1; dragObjectIndex_ = -1; break;
        }
    }

    void EditorViewportController::PaintOverlay_(HDC dc, const RECT& rc)
    {
        HBRUSH keyBrush = CreateSolidBrush(kOverlayKey);
        FillRect(dc, &rc, keyBrush);
        DeleteObject(keyBrush);

        // Grid + world axes moved to the DX12 pass so they participate in depth.
        // The layered overlay now contains only intentionally always-visible editor
        // feedback: selection bounds and transform gizmo handles.
        if (selectedIndex_ < 0)
            return;

        const auto& theme = EditorTheme::Colors();
        const noc::AABB bounds = ValidationWorldBounds_(selectedIndex_);
        const noc::Vec3 c[8] = {
            {bounds.min.x,bounds.min.y,bounds.min.z},{bounds.max.x,bounds.min.y,bounds.min.z},
            {bounds.min.x,bounds.max.y,bounds.min.z},{bounds.max.x,bounds.max.y,bounds.min.z},
            {bounds.min.x,bounds.min.y,bounds.max.z},{bounds.max.x,bounds.min.y,bounds.max.z},
            {bounds.min.x,bounds.max.y,bounds.max.z},{bounds.max.x,bounds.max.y,bounds.max.z}
        };
        const int edges[12][2] = {
            {0,1},{0,2},{1,3},{2,3},{4,5},{4,6},{5,7},{6,7},{0,4},{1,5},{2,6},{3,7}
        };
        for (const auto& edge : edges)
        {
            POINT a{}, e{};
            if (Project_(c[edge[0]], a) && Project_(c[edge[1]], e))
                DrawLine(dc, a, e, theme.warning, 2);
        }

        const int tool = ActiveTool_();
        if (tool == kMoveTool || tool == kRotateTool || tool == kScaleTool)
        {
            const ValidationObject& object = validationObjects_[selectedIndex_];
            const noc::Vec3 axes[3] = { {1,0,0},{0,1,0},{0,0,1} };
            const COLORREF colors[3] = { RGB(224,75,75), RGB(74,207,112), RGB(73,139,239) };
            POINT center{};
            if (Project_(object.t, center))
            {
                for (int i = 0; i < 3; ++i)
                {
                    POINT end{};
                    if (!Project_(object.t + noc::Rotate(object.r, axes[i]) * 1.5f, end)) continue;
                    const COLORREF color = (gizmoDragging_ && gizmoAxis_ == i) ? RGB(255,236,130) : colors[i];
                    DrawLine(dc, center, end, color, 3);
                    HBRUSH brush = CreateSolidBrush(color);
                    HGDIOBJ oldBrush = SelectObject(dc, brush);
                    HGDIOBJ oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
                    Ellipse(dc, end.x - 4, end.y - 4, end.x + 5, end.y + 5);
                    SelectObject(dc, oldPen); SelectObject(dc, oldBrush); DeleteObject(brush);
                }
            }
        }
    }

    int EditorViewportController::HierarchyRowFromY_(int y) const
    {
        if (y < 0) return -1;
        const int row = y / kHierarchyRowHeight;
        const int count = hierarchyObjectsExpanded_ ? 8 : 4;
        return row >= 0 && row < count ? row : -1;
    }

    int EditorViewportController::HierarchyObjectIndexFromRow_(int row) const
    {
        if (!hierarchyObjectsExpanded_) return -1;
        return row >= 2 && row < 2 + kValidationObjectCount ? row - 2 : -1;
    }

    void EditorViewportController::PaintSceneHierarchy_(HWND hwnd, HDC dc, const RECT& rc)
    {
        const auto& colors = EditorTheme::Colors();
        FillColor(dc, rc, colors.panelBg);
        HFONT font = reinterpret_cast<HFONT>(SendMessageW(hwnd, WM_GETFONT, 0, 0));

        struct Row
        {
            const wchar_t* text;
            int depth;
            EditorIconId icon;
            bool expandable;
            bool expanded;
        };

        std::vector<Row> rows;
        rows.reserve(8);
        rows.push_back({ L"Scene (Runtime World)", 0, EditorIconId::World, true, true });
        rows.push_back({ L"Runtime Objects (4)", 1, EditorIconId::Folder, true, hierarchyObjectsExpanded_ });
        if (hierarchyObjectsExpanded_)
        {
            rows.push_back({ L"Cube_A", 2, EditorIconId::Cube, false, true });
            rows.push_back({ L"Cube_B", 2, EditorIconId::Cube, false, true });
            rows.push_back({ L"Cube_C", 2, EditorIconId::Cube, false, true });
            rows.push_back({ L"Ground_Plane", 2, EditorIconId::Grid, false, true });
        }
        rows.push_back({ L"Main Camera", 1, EditorIconId::Camera, false, true });
        rows.push_back({ L"Environment (Procedural Sky)", 1, EditorIconId::World, false, true });

        int y = 0;
        for (int rowIndex = 0; rowIndex < static_cast<int>(rows.size()); ++rowIndex, y += kHierarchyRowHeight)
        {
            if (y >= rc.bottom) break;
            const Row& item = rows[rowIndex];
            RECT rowRc{ 0, y, rc.right - 10, y + kHierarchyRowHeight };
            if (rowIndex == hierarchySelectedRow_)
                FillColor(dc, rowRc, BlendColor(colors.panelBg, colors.accent, 35));
            else if (rowIndex == hierarchyHoverRow_)
                FillColor(dc, rowRc, colors.panelBgAlt);

            const int arrowX = 9 + item.depth * 16;
            if (item.expandable)
            {
                POINT p[3]{};
                if (item.expanded)
                {
                    p[0] = { arrowX + 2, y + 9 }; p[1] = { arrowX + 10, y + 9 }; p[2] = { arrowX + 6, y + 13 };
                }
                else
                {
                    p[0] = { arrowX + 4, y + 7 }; p[1] = { arrowX + 4, y + 15 }; p[2] = { arrowX + 9, y + 11 };
                }
                HBRUSH brush = CreateSolidBrush(colors.textMuted);
                HGDIOBJ oldBrush = SelectObject(dc, brush);
                HGDIOBJ oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
                Polygon(dc, p, 3);
                SelectObject(dc, oldPen); SelectObject(dc, oldBrush); DeleteObject(brush);
            }

            RECT iconRc{ arrowX + 15, y + 5, arrowX + 29, y + 19 };
            DrawEditorSvgIcon(dc, item.icon, iconRc,
                rowIndex == hierarchySelectedRow_ ? colors.textPrimary : colors.textMuted);
            RECT textRc{ arrowX + 34, y, rowRc.right - 4, y + kHierarchyRowHeight };
            DrawHierarchyText(dc, item.text, textRc, colors.textPrimary, font);
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
        case WM_RBUTTONDOWN: case WM_RBUTTONUP:
        case WM_LBUTTONDOWN: case WM_LBUTTONUP:
        case WM_MOUSEMOVE: case WM_MOUSEWHEEL: case WM_CAPTURECHANGED:
            self->HandleOverlayMouse_(msg, wParam, lParam);
            return 0;
        case WM_KILLFOCUS:
            self->cameraCapturing_ = false;
            self->EndGizmoDrag_();
            return 0;
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

    LRESULT CALLBACK EditorViewportController::MainSubclassProc_(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
        UINT_PTR subclassId, DWORD_PTR refData)
    {
        (void)subclassId;
        auto* self = reinterpret_cast<EditorViewportController*>(refData);
        if (self && msg == WM_TIMER && wParam == kTimerId)
        {
            self->UpdateCamera_();
            if (self->overlay_) InvalidateRect(self->overlay_, nullptr, FALSE);
        }
        return DefSubclassProc(hwnd, msg, wParam, lParam);
    }

    LRESULT CALLBACK EditorViewportController::SceneTreeSubclassProc_(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
        UINT_PTR subclassId, DWORD_PTR refData)
    {
        (void)subclassId;
        auto* self = reinterpret_cast<EditorViewportController*>(refData);
        if (!self)
            return DefSubclassProc(hwnd, msg, wParam, lParam);

        switch (msg)
        {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
        {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(hwnd, &ps);
            RECT rc{}; GetClientRect(hwnd, &rc);
            self->PaintSceneHierarchy_(hwnd, dc, rc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_MOUSEMOVE:
        {
            const int row = self->HierarchyRowFromY_(GET_Y_LPARAM(lParam));
            if (row != self->hierarchyHoverRow_)
            {
                self->hierarchyHoverRow_ = row;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            TRACKMOUSEEVENT t{ sizeof(t), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&t);
            return 0;
        }
        case WM_MOUSELEAVE:
            self->hierarchyHoverRow_ = -1;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_LBUTTONDOWN:
        {
            SetFocus(hwnd);
            const int row = self->HierarchyRowFromY_(GET_Y_LPARAM(lParam));
            if (row < 0) return 0;

            // Runtime Objects is a real grouping row. Clicking its arrow only
            // expands/collapses it; selecting the row itself no longer maps to a
            // synthetic cube click, removing the Phase 14 hierarchy flicker.
            if (row == 1)
            {
                const int x = GET_X_LPARAM(lParam);
                const int arrowX = 9 + 16;
                if (x >= arrowX && x <= arrowX + 14)
                    self->hierarchyObjectsExpanded_ = !self->hierarchyObjectsExpanded_;

                self->SetSelectedIndex_(-1, false);
                self->hierarchySelectedRow_ = 1;
            }
            else
            {
                const int objectIndex = self->HierarchyObjectIndexFromRow_(row);
                if (objectIndex >= 0)
                {
                    self->SetSelectedIndex_(objectIndex, false);
                    self->hierarchySelectedRow_ = row;
                }
                else
                {
                    self->SetSelectedIndex_(-1, false);
                    self->hierarchySelectedRow_ = row;
                }
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            if (self->overlay_) InvalidateRect(self->overlay_, nullptr, FALSE);
            return 0;
        }
        case WM_LBUTTONUP:
        case WM_MOUSEWHEEL:
            return 0;
        }

        return DefSubclassProc(hwnd, msg, wParam, lParam);
    }
}
