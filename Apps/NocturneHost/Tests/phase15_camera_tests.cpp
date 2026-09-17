#include "Runtime/CameraSystem.h"

#include "Core/Log.h"
#include "Core/Memory/Allocator.h"
#include "Core/Memory/DebugAlloc.h"
#include "Runtime/EntityRegistry.h"
#include "Runtime/TransformSystem.h"

#include <cmath>

namespace
{
    bool CheckCamera(bool condition, const char* message)
    {
        if (!condition)
        {
            NOC_LOG_ERROR("Phase15", "%s", message);
            return false;
        }
        return true;
    }

    bool NearCamera(float a, float b, float epsilon = 1e-4f)
    {
        return std::fabs(a - b) <= epsilon;
    }
}

bool RunPhase15CameraTests()
{
    NOC_LOG_INFO("Phase15", "%s", "Camera component tests begin");

    noc::MallocAllocator backing;
    noc::DebugAlloc allocator(backing);
    noc::EntityRegistry entities;
    noc::TransformSystem transforms;
    noc::CameraSystem cameras;

    bool ok = true;

    ok &= CheckCamera(entities.Init(allocator, 8), "EntityRegistry init failed");
    ok &= CheckCamera(
        transforms.Init(entities, allocator, 8),
        "TransformSystem init failed");
    ok &= CheckCamera(
        cameras.Init(entities, transforms, allocator, 4),
        "CameraSystem init failed");

    const noc::EntityHandle cameraEntity = entities.Create();

    noc::CameraComponent* camera = cameras.Add(cameraEntity);
    ok &= CheckCamera(camera != nullptr, "Add camera failed");
    ok &= CheckCamera(camera && camera->enabled, "Camera must default enabled");
    ok &= CheckCamera(
        camera && NearCamera(camera->fovYRadians, 1.04719755f),
        "Default FOV mismatch");
    ok &= CheckCamera(
        camera && NearCamera(camera->aspect, 16.0f / 9.0f),
        "Default aspect mismatch");
    ok &= CheckCamera(
        camera && NearCamera(camera->nearZ, 0.1f)
            && NearCamera(camera->farZ, 500.0f),
        "Default near/far mismatch");

    ok &= CheckCamera(
        cameras.Add(cameraEntity) == nullptr,
        "Duplicate CameraComponent add must fail");

    // CameraComponent can exist without TransformComponent, but cannot become
    // active or rebuild until spatial state exists.
    ok &= CheckCamera(
        !cameras.SetActive(cameraEntity),
        "Camera without transform became active");
    ok &= CheckCamera(
        !cameras.Rebuild(cameraEntity),
        "Camera without transform rebuilt successfully");

    ok &= CheckCamera(
        transforms.Add(cameraEntity) != nullptr,
        "Add camera transform failed");
    ok &= CheckCamera(
        transforms.SetLocalTRS(
            cameraEntity,
            noc::Vec3{ 10.0f, 2.0f, -5.0f },
            noc::Quat::Identity(),
            noc::Vec3::One()),
        "Set camera transform failed");

    ok &= CheckCamera(cameras.SetActive(cameraEntity), "SetActive(camera) failed");
    ok &= CheckCamera(
        cameras.ActiveCamera() == cameraEntity,
        "Active camera identity mismatch");

    noc::Mat4 view{};
    noc::Mat4 proj{};
    noc::Mat4 viewProj{};

    ok &= CheckCamera(
        cameras.RebuildActive(view, proj, viewProj),
        "RebuildActive failed");

    // Identity orientation means LH forward +Z and up +Y. Transforming the
    // camera's own world-space position by view must place it at view origin.
    const noc::Vec3 eyeInView =
        noc::TransformPoint(view, noc::Vec3{ 10.0f, 2.0f, -5.0f });

    ok &= CheckCamera(
        NearCamera(eyeInView.x, 0.0f)
            && NearCamera(eyeInView.y, 0.0f)
            && NearCamera(eyeInView.z, 0.0f),
        "View matrix did not move camera eye to origin");

    ok &= CheckCamera(
        NearCamera(proj.m[0], proj.m[5] / (16.0f / 9.0f)),
        "Projection aspect relationship is incorrect");

    const float customFov = 0.9f;
    const float customAspect = 2.0f;
    const float customNear = 0.25f;
    const float customFar = 1000.0f;

    ok &= CheckCamera(
        cameras.SetPerspective(
            cameraEntity,
            customFov,
            customAspect,
            customNear,
            customFar),
        "SetPerspective(valid) failed");

    ok &= CheckCamera(
        cameras.Rebuild(cameraEntity),
        "Rebuild after perspective change failed");

    const noc::CameraComponent* updated = cameras.Get(cameraEntity);
    ok &= CheckCamera(
        updated
            && NearCamera(updated->fovYRadians, customFov)
            && NearCamera(updated->aspect, customAspect)
            && NearCamera(updated->nearZ, customNear)
            && NearCamera(updated->farZ, customFar),
        "Perspective parameters did not persist");

    // Invalid lens parameters must be rejected without mutating existing state.
    ok &= CheckCamera(
        !cameras.SetPerspective(
            cameraEntity,
            0.0f,
            customAspect,
            customNear,
            customFar),
        "Zero FOV was accepted");
    ok &= CheckCamera(
        !cameras.SetPerspective(
            cameraEntity,
            customFov,
            0.0f,
            customNear,
            customFar),
        "Zero aspect was accepted");
    ok &= CheckCamera(
        !cameras.SetPerspective(
            cameraEntity,
            customFov,
            customAspect,
            0.0f,
            customFar),
        "Zero near plane was accepted");
    ok &= CheckCamera(
        !cameras.SetPerspective(
            cameraEntity,
            customFov,
            customAspect,
            5.0f,
            5.0f),
        "farZ <= nearZ was accepted");

    updated = cameras.Get(cameraEntity);
    ok &= CheckCamera(
        updated
            && NearCamera(updated->fovYRadians, customFov)
            && NearCamera(updated->aspect, customAspect)
            && NearCamera(updated->nearZ, customNear)
            && NearCamera(updated->farZ, customFar),
        "Invalid lens update mutated camera state");

    ok &= CheckCamera(
        cameras.SetAspect(cameraEntity, 1.25f),
        "SetAspect(valid) failed");
    ok &= CheckCamera(
        !cameras.SetAspect(cameraEntity, -1.0f),
        "Negative aspect was accepted");

    // Active camera can be disabled; doing so clears active selection.
    ok &= CheckCamera(
        cameras.SetEnabled(cameraEntity, false),
        "Disabling active camera failed");
    ok &= CheckCamera(
        !cameras.ActiveCamera().IsValid(),
        "Disabling active camera did not clear active selection");
    ok &= CheckCamera(
        !cameras.SetActive(cameraEntity),
        "Disabled camera became active");

    ok &= CheckCamera(
        cameras.SetEnabled(cameraEntity, true),
        "Re-enabling camera failed");
    ok &= CheckCamera(
        cameras.SetActive(cameraEntity),
        "Reactivated camera could not become active");

    // Parent transform must contribute to camera world-space eye.
    const noc::EntityHandle rig = entities.Create();
    ok &= CheckCamera(transforms.Add(rig) != nullptr, "Add camera rig transform failed");
    ok &= CheckCamera(
        transforms.SetLocalTRS(
            rig,
            noc::Vec3{ 100.0f, 0.0f, 0.0f },
            noc::Quat::Identity(),
            noc::Vec3::One()),
        "Set camera rig transform failed");
    ok &= CheckCamera(
        transforms.SetParent(cameraEntity, rig),
        "Parent camera to rig failed");

    ok &= CheckCamera(
        cameras.Rebuild(cameraEntity),
        "Rebuild parented camera failed");

    updated = cameras.Get(cameraEntity);
    const noc::Vec3 parentedEyeInView =
        noc::TransformPoint(
            updated->view,
            noc::Vec3{ 110.0f, 2.0f, -5.0f });

    ok &= CheckCamera(
        NearCamera(parentedEyeInView.x, 0.0f)
            && NearCamera(parentedEyeInView.y, 0.0f)
            && NearCamera(parentedEyeInView.z, 0.0f),
        "Camera did not derive eye from world transform hierarchy");

    // Zero camera scale makes basis extraction degenerate and must fail safely.
    ok &= CheckCamera(
        transforms.SetLocalTRS(
            cameraEntity,
            noc::Vec3{ 10.0f, 2.0f, -5.0f },
            noc::Quat::Identity(),
            noc::Vec3::Zero()),
        "Set zero camera scale failed");
    ok &= CheckCamera(
        !cameras.Rebuild(cameraEntity),
        "Degenerate camera basis rebuilt successfully");

    ok &= CheckCamera(
        transforms.SetLocalTRS(
            cameraEntity,
            noc::Vec3{ 10.0f, 2.0f, -5.0f },
            noc::Quat::Identity(),
            noc::Vec3::One()),
        "Restore camera scale failed");

    // Removing active CameraComponent clears selection.
    ok &= CheckCamera(cameras.Remove(cameraEntity), "Remove active camera failed");
    ok &= CheckCamera(
        !cameras.ActiveCamera().IsValid(),
        "Removing active camera did not clear selection");
    ok &= CheckCamera(
        !cameras.Remove(cameraEntity),
        "Removing absent CameraComponent must fail safely");

    // Stale/dead entity operations must be rejected.
    const noc::EntityHandle stale = entities.Create();
    ok &= CheckCamera(cameras.Add(stale) != nullptr, "Add stale-test camera failed");
    ok &= CheckCamera(transforms.Add(stale) != nullptr, "Add stale-test transform failed");
    ok &= CheckCamera(cameras.Remove(stale), "Remove stale-test camera failed");
    ok &= CheckCamera(transforms.Remove(stale), "Remove stale-test transform failed");
    ok &= CheckCamera(entities.Destroy(stale), "Destroy stale-test entity failed");
    ok &= CheckCamera(
        cameras.Add(stale) == nullptr,
        "Stale entity accepted for Camera add");

    // Dense enumeration remains available for editor/query integration.
    const noc::EntityHandle c1 = entities.Create();
    const noc::EntityHandle c2 = entities.Create();
    ok &= CheckCamera(cameras.Add(c1) != nullptr, "Add c1 failed");
    ok &= CheckCamera(cameras.Add(c2) != nullptr, "Add c2 failed");
    ok &= CheckCamera(cameras.DenseCount() == 2, "Dense camera count mismatch");
    ok &= CheckCamera(
        cameras.OwnerAtDenseIndex(0).IsValid(),
        "Dense camera owner invalid");
    ok &= CheckCamera(
        cameras.ComponentAtDenseIndex(cameras.DenseCount()) == nullptr,
        "Out-of-range dense camera access must return null");

    ok &= CheckCamera(cameras.Remove(c1), "Remove c1 failed");
    ok &= CheckCamera(cameras.Remove(c2), "Remove c2 failed");

    cameras.Shutdown();
    transforms.Shutdown();
    entities.Shutdown();

    ok &= CheckCamera(
        allocator.OutstandingBytes() == 0,
        "Camera tests leaked allocator-owned memory");

    NOC_LOG_INFO("Phase15", "Camera component tests %s", ok ? "PASS" : "FAIL");
    return ok;
}
