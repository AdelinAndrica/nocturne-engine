#include "Runtime/World.h"

#include "Core/Log.h"
#include "Core/Memory/Allocator.h"
#include "Core/Memory/DebugAlloc.h"
#include "Core/Memory/LinearArena.h"
#include "Render/RenderQueue.h"

#include <cmath>
#include <cstddef>
#include <cstring>

namespace
{
    bool CheckWorld(bool condition, const char* message)
    {
        if (!condition)
        {
            NOC_LOG_ERROR("Phase15", "%s", message);
            return false;
        }
        return true;
    }

    bool NearWorld(float a, float b, float epsilon = 1.0e-4f)
    {
        return std::fabs(a - b) <= epsilon;
    }
}

bool RunPhase15WorldTests()
{
    NOC_LOG_INFO("Phase15", "%s", "World ECS integration tests begin");

    noc::MallocAllocator backing;
    noc::DebugAlloc allocator(backing);
    noc::World world;

    bool ok = true;

    ok &= CheckWorld(world.Init(allocator), "World::Init failed");
    ok &= CheckWorld(world.AliveCount() == 0, "World must begin empty");

    ok &= CheckWorld(
        world.FindComponentType(noc::kTransformComponentTypeId) != nullptr,
        "Transform metadata missing from World");
    ok &= CheckWorld(
        world.FindComponentType(noc::kRenderableComponentTypeId) != nullptr,
        "Renderable metadata missing from World");
    ok &= CheckWorld(
        world.FindComponentType(noc::kCameraComponentTypeId) != nullptr,
        "Camera metadata missing from World");
    ok &= CheckWorld(
        world.FindComponentType(noc::kNameComponentTypeId) != nullptr,
        "Name metadata missing from World");

    const noc::EntityHandle generic = world.CreateEntity();
    ok &= CheckWorld(generic.IsValid(), "CreateEntity failed");
    ok &= CheckWorld(world.IsAlive(generic), "Created entity is not alive");
    ok &= CheckWorld(
        world.GetTransform(generic) == nullptr,
        "Generic entity unexpectedly received TransformComponent");

    ok &= CheckWorld(world.AddTransform(generic), "AddTransform(generic) failed");
    ok &= CheckWorld(world.AddName(generic, "Generic"), "AddName(generic) failed");
    ok &= CheckWorld(
        world.GetName(generic)
            && std::strcmp(world.GetName(generic)->value, "Generic") == 0,
        "NameComponent did not persist through World");

    const noc::SceneObjectHandle parent = world.CreateObject();
    const noc::SceneObjectHandle child = world.CreateObject();

    ok &= CheckWorld(parent.IsValid() && child.IsValid(), "CreateObject failed");
    ok &= CheckWorld(
        world.GetTransform(parent) != nullptr
            && world.GetTransform(child) != nullptr,
        "CreateObject did not create TransformComponent");

    ok &= CheckWorld(
        world.SetLocalTRS(
            parent,
            noc::Vec3{ 10.0f, 0.0f, 0.0f },
            noc::Quat::Identity(),
            noc::Vec3::One()),
        "SetLocalTRS(parent) failed");

    ok &= CheckWorld(
        world.SetLocalTRS(
            child,
            noc::Vec3{ 2.0f, 0.0f, 0.0f },
            noc::Quat::Identity(),
            noc::Vec3::One()),
        "SetLocalTRS(child) failed");

    ok &= CheckWorld(world.SetParent(child, parent), "SetParent failed");

    world.Update();

    const noc::Mat4 childWorld = world.GetWorldMatrix(child);
    ok &= CheckWorld(
        NearWorld(childWorld.m[12], 12.0f),
        "World transform hierarchy integration is incorrect");

    const noc::ResourceHandle mesh{ 77u, 5u };
    const noc::AABB localBounds{
        noc::Vec3{ -1.0f, -1.0f, -1.0f },
        noc::Vec3{ 1.0f, 1.0f, 1.0f }
    };

    ok &= CheckWorld(
        world.SetRenderable(child, mesh, localBounds),
        "SetRenderable(child) failed");

    const noc::RenderableComponent* renderable = world.GetRenderable(child);
    ok &= CheckWorld(
        renderable && renderable->mesh == mesh,
        "RenderableComponent mesh mismatch");

    const noc::SceneObjectHandle camera = world.CreateObject();
    ok &= CheckWorld(camera.IsValid(), "Create camera object failed");

    ok &= CheckWorld(
        world.SetLocalTRS(
            camera,
            noc::Vec3{ 0.0f, 0.0f, -5.0f },
            noc::Quat::Identity(),
            noc::Vec3::One()),
        "Set camera transform failed");

    ok &= CheckWorld(
        world.SetCameraParams(
            1.04719755f,
            16.0f / 9.0f,
            0.05f,
            500.0f),
        "SetCameraParams failed");

    ok &= CheckWorld(
        world.SetCameraFromObject(camera),
        "SetCameraFromObject failed");

    ok &= CheckWorld(
        world.ActiveCamera() == camera
            && world.GetCamera(camera) != nullptr,
        "World active camera did not use CameraComponent");

    constexpr std::size_t kFrameArenaBytes = 256u * 1024u;
    void* frameMemory = allocator.Allocate(kFrameArenaBytes, 64);
    ok &= CheckWorld(frameMemory != nullptr, "Frame arena backing allocation failed");

    noc::LinearArena frameArena;
    frameArena.Init(frameMemory, kFrameArenaBytes);

    world.SetCullingEnabled(false);

    const noc::RenderQueue queue =
        world.BuildRenderQueue(frameArena, 1280, 720);

    ok &= CheckWorld(queue.totalRenderables == 1, "RenderQueue total mismatch");
    ok &= CheckWorld(queue.instanceCount == 1, "RenderQueue visible count mismatch");
    ok &= CheckWorld(queue.instances != nullptr, "RenderQueue instances missing");
    ok &= CheckWorld(
        queue.instances
            && queue.instances[0].mesh == mesh,
        "RenderQueue mesh extraction mismatch");
    ok &= CheckWorld(
        queue.instances
            && NearWorld(queue.instances[0].world.m[12], 12.0f),
        "RenderQueue world transform extraction mismatch");
    ok &= CheckWorld(
        queue.view.viewportWidth == 1280
            && queue.view.viewportHeight == 720,
        "RenderQueue viewport mismatch");

    ok &= CheckWorld(world.DestroyEntity(parent), "Destroy parent failed");
    ok &= CheckWorld(!world.IsAlive(parent), "Destroyed parent still alive");
    ok &= CheckWorld(world.IsAlive(child), "Destroy parent destroyed child");

    world.Update();

    const noc::Mat4 promotedWorld = world.GetWorldMatrix(child);
    ok &= CheckWorld(
        NearWorld(promotedWorld.m[12], 2.0f),
        "Promoted child did not preserve local TRS");

    const noc::EntityHandle staleChild = child;
    ok &= CheckWorld(world.DestroyEntity(child), "Destroy child failed");
    ok &= CheckWorld(world.GetTransform(staleChild) == nullptr, "Destroyed Transform still visible");
    ok &= CheckWorld(world.GetRenderable(staleChild) == nullptr, "Destroyed Renderable still visible");

    const noc::EntityHandle replacement = world.CreateEntity();
    ok &= CheckWorld(replacement.IsValid(), "Replacement entity creation failed");
    ok &= CheckWorld(
        !world.IsAlive(staleChild),
        "Stale entity handle resurrected after slot reuse");

    if (replacement.index == staleChild.index)
    {
        ok &= CheckWorld(
            replacement.generation != staleChild.generation,
            "Reused entity slot did not advance generation");
    }

    ok &= CheckWorld(
        !world.AddName(staleChild, "Stale"),
        "Stale entity accepted NameComponent");
    ok &= CheckWorld(
        !world.SetLocalTRS(
            staleChild,
            noc::Vec3::Zero(),
            noc::Quat::Identity(),
            noc::Vec3::One()),
        "Stale entity accepted transform mutation");

    ok &= CheckWorld(
        world.EntityAtIndex(generic.index) == generic,
        "EntityAtIndex did not resolve live entity");
    ok &= CheckWorld(
        !world.EntityAtIndex(world.EntityCapacity() + 10u).IsValid(),
        "EntityAtIndex accepted out-of-range slot");

    ok &= CheckWorld(world.DestroyEntity(camera), "Destroy camera failed");
    ok &= CheckWorld(
        !world.ActiveCamera().IsValid(),
        "Destroy active camera did not clear active selection");

    ok &= CheckWorld(world.DestroyEntity(generic), "Destroy generic failed");
    ok &= CheckWorld(world.DestroyEntity(replacement), "Destroy replacement failed");
    ok &= CheckWorld(world.AliveCount() == 0, "World not empty after cleanup");

    world.Shutdown();

    allocator.Deallocate(frameMemory);

    ok &= CheckWorld(
        allocator.OutstandingBytes() == 0,
        "World ECS integration leaked allocator-owned memory");

    NOC_LOG_INFO("Phase15", "World ECS integration tests %s", ok ? "PASS" : "FAIL");
    return ok;
}
