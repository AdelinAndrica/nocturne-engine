#include "Runtime/RenderableSystem.h"

#include "Core/Log.h"
#include "Core/Memory/Allocator.h"
#include "Core/Memory/DebugAlloc.h"
#include "Runtime/EntityRegistry.h"

#include <cmath>

namespace
{
    bool CheckRenderable(bool condition, const char* message)
    {
        if (!condition)
        {
            NOC_LOG_ERROR("Phase15", "%s", message);
            return false;
        }
        return true;
    }

    bool NearRenderable(float a, float b, float epsilon = 1e-4f)
    {
        return std::fabs(a - b) <= epsilon;
    }

    bool BoundsAre(
        const noc::AABB& bounds,
        const noc::Vec3& min,
        const noc::Vec3& max)
    {
        return NearRenderable(bounds.min.x, min.x)
            && NearRenderable(bounds.min.y, min.y)
            && NearRenderable(bounds.min.z, min.z)
            && NearRenderable(bounds.max.x, max.x)
            && NearRenderable(bounds.max.y, max.y)
            && NearRenderable(bounds.max.z, max.z);
    }
}

bool RunPhase15RenderableTests()
{
    NOC_LOG_INFO("Phase15", "%s", "Renderable component tests begin");

    noc::MallocAllocator backing;
    noc::DebugAlloc allocator(backing);
    noc::EntityRegistry entities;
    noc::RenderableSystem renderables;

    bool ok = true;

    ok &= CheckRenderable(entities.Init(allocator, 4), "EntityRegistry init failed");
    ok &= CheckRenderable(
        renderables.Init(entities, allocator, 2),
        "RenderableSystem init failed");

    const noc::EntityHandle e0 = entities.Create();
    const noc::EntityHandle e1 = entities.Create();
    const noc::EntityHandle e2 = entities.Create();

    noc::RenderableComponent* r0 = renderables.Add(e0);
    ok &= CheckRenderable(r0 != nullptr, "Add(e0) failed");
    ok &= CheckRenderable(r0 && !r0->mesh.IsValid(), "Default mesh must be invalid");
    ok &= CheckRenderable(r0 && r0->enabled, "Renderable must default enabled");
    ok &= CheckRenderable(
        r0 && r0->worldBoundsDirty,
        "Renderable world bounds must begin dirty");
    ok &= CheckRenderable(
        r0 && BoundsAre(
            r0->localBounds,
            noc::Vec3::Zero(),
            noc::Vec3::Zero()),
        "Default local bounds must be zero-sized");

    ok &= CheckRenderable(
        renderables.Add(e0) == nullptr,
        "Duplicate Renderable add must fail");

    const noc::ResourceHandle mesh0{ 7u, 3u };
    ok &= CheckRenderable(
        renderables.SetMesh(e0, mesh0),
        "SetMesh(e0) failed");
    ok &= CheckRenderable(
        renderables.Get(e0) && renderables.Get(e0)->mesh == mesh0,
        "Mesh handle did not persist");

    const noc::AABB local{
        noc::Vec3{ -1.0f, -2.0f, -3.0f },
        noc::Vec3{  1.0f,  2.0f,  3.0f }
    };

    ok &= CheckRenderable(
        renderables.SetLocalBounds(e0, local),
        "SetLocalBounds(e0) failed");
    ok &= CheckRenderable(
        renderables.Get(e0) && renderables.Get(e0)->worldBoundsDirty,
        "Changing local bounds must dirty world bounds");

    const noc::Mat4 translated =
        noc::TRS(
            noc::Vec3{ 10.0f, 20.0f, 30.0f },
            noc::Quat::Identity(),
            noc::Vec3::One());

    ok &= CheckRenderable(
        renderables.UpdateWorldBounds(e0, translated),
        "UpdateWorldBounds(e0) failed");

    const noc::RenderableComponent* updated = renderables.Get(e0);
    ok &= CheckRenderable(
        updated && !updated->worldBoundsDirty,
        "World bounds dirty flag was not cleared");
    ok &= CheckRenderable(
        updated && BoundsAre(
            updated->worldBounds,
            noc::Vec3{ 9.0f, 18.0f, 27.0f },
            noc::Vec3{ 11.0f, 22.0f, 33.0f }),
        "Translated world bounds are incorrect");

    // Updating a clean cache is a valid no-op.
    ok &= CheckRenderable(
        renderables.UpdateWorldBounds(
            e0,
            noc::Translation(noc::Vec3{ 100.0f, 0.0f, 0.0f })),
        "Clean world-bounds update failed");
    updated = renderables.Get(e0);
    ok &= CheckRenderable(
        updated && NearRenderable(updated->worldBounds.min.x, 9.0f),
        "Clean world-bounds update unexpectedly recomputed cache");

    ok &= CheckRenderable(
        renderables.MarkWorldBoundsDirty(e0),
        "MarkWorldBoundsDirty(e0) failed");
    ok &= CheckRenderable(
        renderables.UpdateWorldBounds(
            e0,
            noc::Translation(noc::Vec3{ 100.0f, 0.0f, 0.0f })),
        "Dirty world-bounds rebuild failed");
    updated = renderables.Get(e0);
    ok &= CheckRenderable(
        updated && NearRenderable(updated->worldBounds.min.x, 99.0f),
        "Dirty world-bounds cache did not rebuild");

    // Conservative AABB under non-uniform scale.
    ok &= CheckRenderable(
        renderables.MarkWorldBoundsDirty(e0),
        "Second bounds invalidation failed");
    ok &= CheckRenderable(
        renderables.UpdateWorldBounds(
            e0,
            noc::Scale(noc::Vec3{ 2.0f, 3.0f, 4.0f })),
        "Scaled world-bounds rebuild failed");
    updated = renderables.Get(e0);
    ok &= CheckRenderable(
        updated && BoundsAre(
            updated->worldBounds,
            noc::Vec3{ -2.0f, -6.0f, -12.0f },
            noc::Vec3{  2.0f,  6.0f,  12.0f }),
        "Non-uniform scale produced incorrect world bounds");

    ok &= CheckRenderable(
        renderables.SetEnabled(e0, false),
        "Disable renderable failed");
    ok &= CheckRenderable(
        renderables.Get(e0) && !renderables.Get(e0)->enabled,
        "Disabled state did not persist");
    ok &= CheckRenderable(
        renderables.SetEnabled(e0, true),
        "Re-enable renderable failed");

    // Add enough components to force dense/sparse growth; then remove the
    // middle item to exercise ComponentStorage swap-remove through this system.
    ok &= CheckRenderable(renderables.Add(e1) != nullptr, "Add(e1) failed");
    ok &= CheckRenderable(renderables.Add(e2) != nullptr, "Add(e2) failed");
    ok &= CheckRenderable(renderables.Count() == 3, "Renderable count mismatch");

    const noc::ResourceHandle mesh2{ 42u, 9u };
    ok &= CheckRenderable(renderables.SetMesh(e2, mesh2), "SetMesh(e2) failed");

    ok &= CheckRenderable(renderables.Remove(e1), "Remove(e1) failed");
    ok &= CheckRenderable(!renderables.Has(e1), "Removed renderable still exists");
    ok &= CheckRenderable(
        renderables.Get(e2) && renderables.Get(e2)->mesh == mesh2,
        "Swap-remove corrupted moved RenderableComponent");

    ok &= CheckRenderable(
        renderables.DenseCount() == renderables.Count(),
        "DenseCount mismatch");
    ok &= CheckRenderable(
        renderables.OwnerAtDenseIndex(0).IsValid(),
        "Dense owner enumeration returned invalid entity");
    ok &= CheckRenderable(
        renderables.ComponentAtDenseIndex(renderables.DenseCount()) == nullptr,
        "Out-of-range dense renderable access must return null");

    // Stale/dead entities must be rejected by public system operations.
    const noc::EntityHandle stale = entities.Create();
    ok &= CheckRenderable(renderables.Add(stale) != nullptr, "Add stale-test renderable failed");
    ok &= CheckRenderable(renderables.Remove(stale), "Remove stale-test renderable failed");
    ok &= CheckRenderable(entities.Destroy(stale), "Destroy stale-test entity failed");
    ok &= CheckRenderable(
        renderables.Add(stale) == nullptr,
        "Stale entity accepted for Renderable add");
    ok &= CheckRenderable(
        !renderables.SetEnabled(stale, false),
        "Stale entity accepted for Renderable mutation");

    // Entity destruction itself is intentionally registry-only at this stage:
    // World destruction cascade integration comes later. Clean up components
    // before destroying owners to preserve the system contract.
    ok &= CheckRenderable(renderables.Remove(e0), "Remove(e0) failed");
    ok &= CheckRenderable(renderables.Remove(e2), "Remove(e2) failed");
    ok &= CheckRenderable(renderables.Count() == 0, "Renderable storage not empty");

    renderables.Shutdown();
    entities.Shutdown();

    ok &= CheckRenderable(
        allocator.OutstandingBytes() == 0,
        "Renderable tests leaked allocator-owned memory");

    NOC_LOG_INFO("Phase15", "Renderable component tests %s", ok ? "PASS" : "FAIL");
    return ok;
}
