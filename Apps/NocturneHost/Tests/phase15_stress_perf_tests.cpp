#include "Runtime/EntityRegistry.h"
#include "Runtime/NameSystem.h"
#include "Runtime/TransformSystem.h"
#include "Runtime/World.h"

#include "Core/Log.h"
#include "Core/Memory/Allocator.h"
#include "Core/Memory/DebugAlloc.h"
#include "Core/Memory/LinearArena.h"
#include "Render/RenderQueue.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace
{
    using Clock = std::chrono::steady_clock;

    bool CheckPerf(bool condition, const char* message)
    {
        if (!condition)
        {
            NOC_LOG_ERROR("Phase15Perf", "%s", message);
            return false;
        }
        return true;
    }

    long long Micros(Clock::time_point begin, Clock::time_point end)
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            end - begin).count();
    }

    void LogBaseline(
        const char* label,
        uint32_t items,
        long long microseconds,
        std::size_t allocationDelta,
        std::size_t byteDelta)
    {
        NOC_LOG_INFO(
            "Phase15Perf",
            "%s: items=%u time_us=%lld allocator_calls=%zu allocated_bytes=%zu",
            label,
            items,
            microseconds,
            allocationDelta,
            byteDelta);
    }

    bool RunEntityRegistryBaseline()
    {
        constexpr uint32_t kEntityCount = 10000;

        noc::MallocAllocator backing;
        noc::DebugAlloc allocator(backing);
        noc::EntityRegistry entities;

        bool ok = true;
        ok &= CheckPerf(
            entities.Init(allocator, 64),
            "EntityRegistry baseline init failed");

        std::vector<noc::EntityHandle> handles;
        handles.reserve(kEntityCount);

        const std::size_t allocBeforeCreate = allocator.AllocationCount();
        const std::size_t bytesBeforeCreate = allocator.TotalAllocatedBytes();
        const auto createBegin = Clock::now();

        for (uint32_t i = 0; i < kEntityCount; ++i)
        {
            const noc::EntityHandle entity = entities.Create();
            if (!entity.IsValid())
            {
                ok &= CheckPerf(false, "EntityRegistry failed during 10k create");
                break;
            }
            handles.push_back(entity);
        }

        const auto createEnd = Clock::now();
        LogBaseline(
            "entity_create_10k",
            static_cast<uint32_t>(handles.size()),
            Micros(createBegin, createEnd),
            allocator.AllocationCount() - allocBeforeCreate,
            allocator.TotalAllocatedBytes() - bytesBeforeCreate);

        ok &= CheckPerf(
            handles.size() == kEntityCount,
            "EntityRegistry did not create 10k entities");

        const auto aliveBegin = Clock::now();
        uint32_t aliveHits = 0;
        for (const noc::EntityHandle entity : handles)
        {
            if (entities.IsAlive(entity))
                ++aliveHits;
        }
        const auto aliveEnd = Clock::now();

        LogBaseline(
            "entity_is_alive_10k",
            kEntityCount,
            Micros(aliveBegin, aliveEnd),
            0,
            0);

        ok &= CheckPerf(
            aliveHits == kEntityCount,
            "IsAlive failed during 10k baseline");

        const auto destroyBegin = Clock::now();
        uint32_t destroyed = 0;
        for (uint32_t i = 0; i < kEntityCount; i += 2)
        {
            if (entities.Destroy(handles[i]))
                ++destroyed;
        }
        const auto destroyEnd = Clock::now();

        LogBaseline(
            "entity_destroy_5k",
            destroyed,
            Micros(destroyBegin, destroyEnd),
            0,
            0);

        ok &= CheckPerf(
            destroyed == kEntityCount / 2u,
            "EntityRegistry failed during 5k destroy");

        const auto reuseBegin = Clock::now();
        uint32_t recreated = 0;
        for (uint32_t i = 0; i < kEntityCount / 2u; ++i)
        {
            if (entities.Create().IsValid())
                ++recreated;
        }
        const auto reuseEnd = Clock::now();

        LogBaseline(
            "entity_reuse_5k",
            recreated,
            Micros(reuseBegin, reuseEnd),
            0,
            0);

        ok &= CheckPerf(
            recreated == kEntityCount / 2u,
            "EntityRegistry failed during free-list reuse baseline");
        ok &= CheckPerf(
            entities.AliveCount() == kEntityCount,
            "EntityRegistry alive count mismatch after reuse");

        entities.Shutdown();

        ok &= CheckPerf(
            allocator.OutstandingBytes() == 0,
            "EntityRegistry baseline leaked allocator memory");

        return ok;
    }

    bool RunComponentStorageBaseline()
    {
        constexpr uint32_t kEntityCount = 10000;

        noc::MallocAllocator backing;
        noc::DebugAlloc allocator(backing);
        noc::EntityRegistry entities;
        noc::NameSystem names;

        bool ok = true;
        ok &= CheckPerf(
            entities.Init(allocator, 64),
            "Component baseline EntityRegistry init failed");
        ok &= CheckPerf(
            names.Init(entities, allocator, 64),
            "Component baseline NameSystem init failed");

        std::vector<noc::EntityHandle> handles;
        handles.reserve(kEntityCount);

        for (uint32_t i = 0; i < kEntityCount; ++i)
        {
            const noc::EntityHandle entity = entities.Create();
            if (!entity.IsValid())
            {
                ok &= CheckPerf(false, "Component baseline entity creation failed");
                break;
            }
            handles.push_back(entity);
        }

        ok &= CheckPerf(
            handles.size() == kEntityCount,
            "Component baseline did not create 10k entities");

        const std::size_t allocBeforeAdd = allocator.AllocationCount();
        const std::size_t bytesBeforeAdd = allocator.TotalAllocatedBytes();
        const auto addBegin = Clock::now();

        uint32_t added = 0;
        for (const noc::EntityHandle entity : handles)
        {
            if (names.Add(entity, "PerfEntity"))
                ++added;
        }

        const auto addEnd = Clock::now();
        LogBaseline(
            "component_add_name_10k",
            added,
            Micros(addBegin, addEnd),
            allocator.AllocationCount() - allocBeforeAdd,
            allocator.TotalAllocatedBytes() - bytesBeforeAdd);

        ok &= CheckPerf(
            added == kEntityCount,
            "Component baseline failed to add 10k NameComponents");

        const auto lookupBegin = Clock::now();
        uint32_t lookupHits = 0;
        for (const noc::EntityHandle entity : handles)
        {
            if (names.Has(entity) && names.Get(entity))
                ++lookupHits;
        }
        const auto lookupEnd = Clock::now();

        LogBaseline(
            "component_has_get_name_10k",
            kEntityCount,
            Micros(lookupBegin, lookupEnd),
            0,
            0);

        ok &= CheckPerf(
            lookupHits == kEntityCount,
            "Component Has/Get baseline missed live components");

        const auto iterationBegin = Clock::now();
        uint32_t denseVisited = 0;
        for (uint32_t i = 0; i < names.DenseCount(); ++i)
        {
            const noc::EntityHandle owner = names.OwnerAtDenseIndex(i);
            const noc::NameComponent* component = names.ComponentAtDenseIndex(i);
            if (owner.IsValid() && component)
                ++denseVisited;
        }
        const auto iterationEnd = Clock::now();

        LogBaseline(
            "component_dense_iteration_name_10k",
            denseVisited,
            Micros(iterationBegin, iterationEnd),
            0,
            0);

        ok &= CheckPerf(
            denseVisited == kEntityCount,
            "Dense component iteration baseline missed components");

        const std::size_t allocBeforeRemove = allocator.AllocationCount();
        const std::size_t bytesBeforeRemove = allocator.TotalAllocatedBytes();
        const auto removeBegin = Clock::now();

        uint32_t removed = 0;
        for (uint32_t i = 0; i < kEntityCount; i += 2u)
        {
            if (names.Remove(handles[i]))
                ++removed;
        }

        const auto removeEnd = Clock::now();
        LogBaseline(
            "component_remove_name_5k",
            removed,
            Micros(removeBegin, removeEnd),
            allocator.AllocationCount() - allocBeforeRemove,
            allocator.TotalAllocatedBytes() - bytesBeforeRemove);

        ok &= CheckPerf(
            removed == kEntityCount / 2u,
            "Component baseline failed to remove 5k NameComponents");
        ok &= CheckPerf(
            names.Count() == kEntityCount / 2u,
            "Component count mismatch after remove baseline");
        ok &= CheckPerf(
            allocator.AllocationCount() == allocBeforeRemove,
            "Component remove performed allocator calls");
        ok &= CheckPerf(
            allocator.TotalAllocatedBytes() == bytesBeforeRemove,
            "Component remove allocated bytes");

        names.Shutdown();
        entities.Shutdown();

        ok &= CheckPerf(
            allocator.OutstandingBytes() == 0,
            "Component storage baseline leaked allocator memory");

        return ok;
    }

    bool RunTransformRootAndWideBaseline()
    {
        constexpr uint32_t kEntityCount = 10000;

        noc::MallocAllocator backing;
        noc::DebugAlloc allocator(backing);
        noc::EntityRegistry entities;
        noc::TransformSystem transforms;

        bool ok = true;
        ok &= CheckPerf(entities.Init(allocator, 64), "Transform entity registry init failed");
        ok &= CheckPerf(transforms.Init(entities, allocator, 64), "TransformSystem baseline init failed");

        std::vector<noc::EntityHandle> handles;
        handles.reserve(kEntityCount);

        for (uint32_t i = 0; i < kEntityCount; ++i)
        {
            const noc::EntityHandle entity = entities.Create();
            if (!entity.IsValid() || !transforms.Add(entity))
            {
                ok &= CheckPerf(false, "Transform baseline setup failed");
                break;
            }
            handles.push_back(entity);
        }

        ok &= CheckPerf(
            handles.size() == kEntityCount,
            "Transform baseline did not create 10k components");

        const std::size_t allocBeforeMutation = allocator.AllocationCount();
        const std::size_t bytesBeforeMutation = allocator.TotalAllocatedBytes();
        const auto mutationBegin = Clock::now();

        for (uint32_t i = 0; i < static_cast<uint32_t>(handles.size()); ++i)
        {
            ok &= CheckPerf(
                transforms.SetLocalTRS(
                    handles[i],
                    noc::Vec3{ static_cast<float>(i % 100u), 0.0f, 0.0f },
                    noc::Quat::Identity(),
                    noc::Vec3::One()),
                "Transform component mutation failed");
        }

        const auto mutationEnd = Clock::now();
        LogBaseline(
            "transform_mutation_10k",
            static_cast<uint32_t>(handles.size()),
            Micros(mutationBegin, mutationEnd),
            allocator.AllocationCount() - allocBeforeMutation,
            allocator.TotalAllocatedBytes() - bytesBeforeMutation);

        const auto allDirtyBegin = Clock::now();
        transforms.Update();
        const auto allDirtyEnd = Clock::now();

        LogBaseline(
            "transform_update_roots_all_dirty_10k",
            static_cast<uint32_t>(handles.size()),
            Micros(allDirtyBegin, allDirtyEnd),
            0,
            0);

        // Mostly-clean workload: dirty 1% of independent roots.
        for (uint32_t i = 0; i < static_cast<uint32_t>(handles.size()); i += 100u)
        {
            const noc::TransformComponent* transform = transforms.Get(handles[i]);
            ok &= CheckPerf(transform != nullptr, "Transform lookup failed in mostly-clean setup");
            if (!transform)
                continue;

            ok &= CheckPerf(
                transforms.SetLocalTRS(
                    handles[i],
                    noc::Vec3{
                        transform->localTranslation.x + 1.0f,
                        transform->localTranslation.y,
                        transform->localTranslation.z
                    },
                    transform->localRotation,
                    transform->localScale),
                "Mostly-clean transform mutation failed");
        }

        const auto mostlyCleanBegin = Clock::now();
        transforms.Update();
        const auto mostlyCleanEnd = Clock::now();

        LogBaseline(
            "transform_update_roots_1pct_dirty_10k",
            static_cast<uint32_t>(handles.size()),
            Micros(mostlyCleanBegin, mostlyCleanEnd),
            0,
            0);

        // Build a wide hierarchy: one root with 9,999 direct children.
        const noc::EntityHandle root = handles[0];
        for (uint32_t i = 1; i < static_cast<uint32_t>(handles.size()); ++i)
        {
            ok &= CheckPerf(
                transforms.SetParent(handles[i], root),
                "Wide hierarchy parent setup failed");
        }
        transforms.Update();

        const noc::TransformComponent* rootTransform = transforms.Get(root);
        ok &= CheckPerf(rootTransform != nullptr, "Wide hierarchy root missing");

        if (rootTransform)
        {
            ok &= CheckPerf(
                transforms.SetLocalTRS(
                    root,
                    noc::Vec3{
                        rootTransform->localTranslation.x + 1.0f,
                        rootTransform->localTranslation.y,
                        rootTransform->localTranslation.z
                    },
                    rootTransform->localRotation,
                    rootTransform->localScale),
                "Wide hierarchy root mutation failed");
        }

        const auto wideBegin = Clock::now();
        transforms.Update();
        const auto wideEnd = Clock::now();

        LogBaseline(
            "transform_update_wide_10k",
            static_cast<uint32_t>(handles.size()),
            Micros(wideBegin, wideEnd),
            0,
            0);

        ok &= CheckPerf(
            !transforms.IsDirty(handles.back()),
            "Wide hierarchy update left descendant dirty");

        transforms.Shutdown();
        entities.Shutdown();

        ok &= CheckPerf(
            allocator.OutstandingBytes() == 0,
            "Transform root/wide baseline leaked allocator memory");

        return ok;
    }

    bool RunTransformDeepBaseline()
    {
        // Deep traversal is measured separately because constructing a chain
        // performs cycle checks against ancestors. 2,048 levels is already
        // substantially deeper than normal authored scene hierarchies, while the
        // overall Phase 15 stress workload still reaches 10k entities elsewhere.
        constexpr uint32_t kDepth = 2048;

        noc::MallocAllocator backing;
        noc::DebugAlloc allocator(backing);
        noc::EntityRegistry entities;
        noc::TransformSystem transforms;

        bool ok = true;
        ok &= CheckPerf(entities.Init(allocator, 64), "Deep hierarchy registry init failed");
        ok &= CheckPerf(transforms.Init(entities, allocator, 64), "Deep hierarchy transform init failed");

        std::vector<noc::EntityHandle> chain;
        chain.reserve(kDepth);

        for (uint32_t i = 0; i < kDepth; ++i)
        {
            const noc::EntityHandle entity = entities.Create();
            if (!entity.IsValid() || !transforms.Add(entity))
            {
                ok &= CheckPerf(false, "Deep hierarchy setup failed");
                break;
            }

            chain.push_back(entity);

            ok &= CheckPerf(
                transforms.SetLocalTRS(
                    entity,
                    noc::Vec3{ 1.0f, 0.0f, 0.0f },
                    noc::Quat::Identity(),
                    noc::Vec3::One()),
                "Deep hierarchy local transform setup failed");

            if (i > 0)
            {
                ok &= CheckPerf(
                    transforms.SetParent(entity, chain[i - 1u]),
                    "Deep hierarchy parent setup failed");
            }
        }

        transforms.Update();

        const noc::TransformComponent* rootTransform =
            chain.empty() ? nullptr : transforms.Get(chain.front());

        if (rootTransform)
        {
            ok &= CheckPerf(
                transforms.SetLocalTRS(
                    chain.front(),
                    noc::Vec3{ 2.0f, 0.0f, 0.0f },
                    rootTransform->localRotation,
                    rootTransform->localScale),
                "Deep hierarchy root mutation failed");
        }

        const auto deepBegin = Clock::now();
        transforms.Update();
        const auto deepEnd = Clock::now();

        LogBaseline(
            "transform_update_deep_2048",
            static_cast<uint32_t>(chain.size()),
            Micros(deepBegin, deepEnd),
            0,
            0);

        noc::Mat4 tailWorld{};
        ok &= CheckPerf(
            !chain.empty()
                && transforms.GetWorldMatrix(chain.back(), tailWorld),
            "Deep hierarchy tail world lookup failed");

        transforms.Shutdown();
        entities.Shutdown();

        ok &= CheckPerf(
            allocator.OutstandingBytes() == 0,
            "Deep transform baseline leaked allocator memory");

        return ok;
    }

    bool RunRenderExtractionBaseline()
    {
        constexpr uint32_t kEntityCount = 10000;

        noc::MallocAllocator backing;
        noc::DebugAlloc allocator(backing);
        noc::World world;

        bool ok = true;
        ok &= CheckPerf(world.Init(allocator), "Render extraction World init failed");

        const noc::ResourceHandle mesh{ 1u, 1u };
        const noc::AABB bounds{
            noc::Vec3{ -0.5f, -0.5f, -0.5f },
            noc::Vec3{  0.5f,  0.5f,  0.5f }
        };

        uint32_t created = 0;
        for (uint32_t i = 0; i < kEntityCount; ++i)
        {
            const noc::EntityHandle entity = world.CreateObject();
            if (!entity.IsValid())
            {
                ok &= CheckPerf(false, "Render extraction entity creation failed");
                break;
            }

            if (!world.SetLocalTRS(
                    entity,
                    noc::Vec3{
                        static_cast<float>(i % 100u),
                        static_cast<float>((i / 100u) % 100u),
                        10.0f + static_cast<float>(i / 10000u)
                    },
                    noc::Quat::Identity(),
                    noc::Vec3::One())
                || !world.SetRenderable(entity, mesh, bounds))
            {
                ok &= CheckPerf(false, "Render extraction component setup failed");
                break;
            }

            ++created;
        }

        ok &= CheckPerf(
            created == kEntityCount,
            "Render extraction baseline did not build 10k renderables");

        world.SetCullingEnabled(false);

        const std::size_t frameArenaBytes =
            sizeof(noc::RenderInstance) * static_cast<std::size_t>(kEntityCount)
            + 4096u;

        void* frameMemory = allocator.Allocate(frameArenaBytes, 64);
        ok &= CheckPerf(frameMemory != nullptr, "Render extraction frame arena allocation failed");

        noc::LinearArena frameArena;
        frameArena.Init(frameMemory, frameArenaBytes);

        const std::size_t allocationsBefore = allocator.AllocationCount();
        const std::size_t bytesBefore = allocator.TotalAllocatedBytes();

        const auto renderBegin = Clock::now();
        const noc::RenderQueue queue =
            world.BuildRenderQueue(frameArena, 1920, 1080);
        const auto renderEnd = Clock::now();

        const std::size_t allocationsAfter = allocator.AllocationCount();
        const std::size_t bytesAfter = allocator.TotalAllocatedBytes();

        LogBaseline(
            "render_extraction_10k",
            kEntityCount,
            Micros(renderBegin, renderEnd),
            allocationsAfter - allocationsBefore,
            bytesAfter - bytesBefore);

        NOC_LOG_INFO(
            "Phase15Perf",
            "render_extraction_frame_arena: used_bytes=%zu capacity_bytes=%zu",
            frameArena.Used(),
            frameArena.Capacity());

        ok &= CheckPerf(
            queue.totalRenderables == kEntityCount,
            "Render extraction total renderable count mismatch");
        ok &= CheckPerf(
            queue.instanceCount == kEntityCount,
            "Render extraction instance count mismatch");
        ok &= CheckPerf(
            allocationsAfter == allocationsBefore,
            "Render extraction performed persistent allocator allocations");
        ok &= CheckPerf(
            bytesAfter == bytesBefore,
            "Render extraction allocated persistent bytes");

        world.Shutdown();

        if (frameMemory)
            allocator.Deallocate(frameMemory);

        ok &= CheckPerf(
            allocator.OutstandingBytes() == 0,
            "Render extraction baseline leaked allocator memory");

        return ok;
    }
}

bool RunPhase15StressPerfTests()
{
    NOC_LOG_INFO(
        "Phase15Perf",
        "%s",
        "Stress/performance baseline begin (timings are observations, not pass/fail budgets)");

    bool ok = true;

    ok &= RunEntityRegistryBaseline();
    ok &= RunComponentStorageBaseline();
    ok &= RunTransformRootAndWideBaseline();
    ok &= RunTransformDeepBaseline();
    ok &= RunRenderExtractionBaseline();

    NOC_LOG_INFO(
        "Phase15Perf",
        "Stress/performance baseline %s",
        ok ? "PASS" : "FAIL");

    return ok;
}
