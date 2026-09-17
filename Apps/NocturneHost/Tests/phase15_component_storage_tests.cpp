#include "Runtime/ComponentStorage.h"
#include "Runtime/EntityRegistry.h"

#include "Core/Log.h"
#include "Core/Memory/Allocator.h"
#include "Core/Memory/DebugAlloc.h"

#include <cstdint>

namespace
{
    bool CheckComponentStorage(bool condition, const char* message)
    {
        if (!condition)
        {
            NOC_LOG_ERROR("Phase15", "%s", message);
            return false;
        }
        return true;
    }

    struct alignas(64) TrackedComponent
    {
        explicit TrackedComponent(int v = 0)
            : value(v)
        {
            ++liveCount;
            ++constructCount;
        }

        TrackedComponent(const TrackedComponent&) = delete;
        TrackedComponent& operator=(const TrackedComponent&) = delete;

        TrackedComponent(TrackedComponent&& other) noexcept
            : value(other.value)
        {
            other.value = -1;
            ++liveCount;
            ++moveConstructCount;
        }

        TrackedComponent& operator=(TrackedComponent&&) = delete;

        ~TrackedComponent()
        {
            --liveCount;
            ++destructCount;
        }

        int value = 0;

        static int liveCount;
        static int constructCount;
        static int moveConstructCount;
        static int destructCount;
    };

    int TrackedComponent::liveCount = 0;
    int TrackedComponent::constructCount = 0;
    int TrackedComponent::moveConstructCount = 0;
    int TrackedComponent::destructCount = 0;
}

bool RunPhase15ComponentStorageTests()
{
    NOC_LOG_INFO("Phase15", "%s", "Component storage tests begin");

    TrackedComponent::liveCount = 0;
    TrackedComponent::constructCount = 0;
    TrackedComponent::moveConstructCount = 0;
    TrackedComponent::destructCount = 0;

    noc::MallocAllocator backing;
    noc::DebugAlloc allocator(backing);
    noc::EntityRegistry registry;
    noc::ComponentStorage<TrackedComponent> storage;

    bool ok = true;
    ok &= CheckComponentStorage(registry.Init(allocator, 2), "Registry init failed");
    ok &= CheckComponentStorage(storage.Init(allocator, 2, 2), "ComponentStorage init failed");
    ok &= CheckComponentStorage(storage.Count() == 0, "Storage must begin empty");

    const noc::EntityHandle e0 = registry.Create();
    const noc::EntityHandle e1 = registry.Create();
    const noc::EntityHandle e2 = registry.Create();

    ok &= CheckComponentStorage(storage.Emplace(e0, 10) != nullptr, "Add e0 failed");
    ok &= CheckComponentStorage(storage.Emplace(e1, 20) != nullptr, "Add e1 failed");
    ok &= CheckComponentStorage(storage.Count() == 2, "Count mismatch after two adds");
    ok &= CheckComponentStorage(storage.Has(e0), "Has(e0) failed");
    ok &= CheckComponentStorage(storage.Get(e1) && storage.Get(e1)->value == 20,
        "Get(e1) returned wrong value");

    const auto* aligned = storage.Get(e0);
    ok &= CheckComponentStorage(
        aligned && (reinterpret_cast<std::uintptr_t>(aligned) % alignof(TrackedComponent)) == 0,
        "Component storage did not preserve over-alignment");

    ok &= CheckComponentStorage(storage.Emplace(e0, 999) == nullptr,
        "Duplicate component add must be rejected");
    ok &= CheckComponentStorage(storage.Count() == 2,
        "Duplicate add changed component count");

    // Third add forces dense and sparse growth from the explicit capacity of 2.
    ok &= CheckComponentStorage(storage.Emplace(e2, 30) != nullptr, "Add e2 failed");
    ok &= CheckComponentStorage(storage.DenseCapacity() >= 3, "Dense storage did not grow");
    ok &= CheckComponentStorage(storage.SparseCapacity() > e2.index, "Sparse storage did not grow");
    ok &= CheckComponentStorage(storage.Get(e0) && storage.Get(e0)->value == 10,
        "Growth corrupted e0");
    ok &= CheckComponentStorage(storage.Get(e1) && storage.Get(e1)->value == 20,
        "Growth corrupted e1");
    ok &= CheckComponentStorage(storage.Get(e2) && storage.Get(e2)->value == 30,
        "Growth corrupted e2");

    // Removing the middle dense element must swap-move the last component and
    // repair the sparse lookup for its owner.
    const uint32_t e2DenseBefore = storage.DenseIndex(e2);
    ok &= CheckComponentStorage(storage.Remove(e1), "Remove(e1) failed");
    ok &= CheckComponentStorage(!storage.Has(e1), "Removed entity still has component");
    ok &= CheckComponentStorage(storage.Count() == 2, "Count mismatch after remove");
    ok &= CheckComponentStorage(storage.Get(e2) && storage.Get(e2)->value == 30,
        "Swap-remove corrupted moved component");
    ok &= CheckComponentStorage(storage.DenseIndex(e2) != noc::kInvalidComponentIndex,
        "Swap-remove did not repair sparse lookup");
    ok &= CheckComponentStorage(storage.DenseIndex(e2) <= e2DenseBefore,
        "Swap-remove did not compact dense storage");
    ok &= CheckComponentStorage(!storage.Remove(e1), "Removing absent component must fail safely");

    ok &= CheckComponentStorage(storage.Emplace(e1, 25) != nullptr,
        "Re-adding removed component failed");
    ok &= CheckComponentStorage(storage.Get(e1) && storage.Get(e1)->value == 25,
        "Re-added component has wrong value");

    // Storage lookup is generation-aware. A different generation with the same
    // entity index must not resolve to the old owner's component.
    const noc::EntityHandle fakeStaleGeneration{ e0.index, e0.generation + 1u };
    ok &= CheckComponentStorage(!storage.Has(fakeStaleGeneration),
        "Different generation resolved to an existing component");
    ok &= CheckComponentStorage(storage.Get(fakeStaleGeneration) == nullptr,
        "Get accepted a different generation");
    ok &= CheckComponentStorage(storage.Emplace(fakeStaleGeneration, 77) == nullptr,
        "Storage must reject insertion over an occupied entity slot");

    const noc::EntityHandle owner0 = storage.OwnerAtDenseIndex(storage.DenseIndex(e0));
    ok &= CheckComponentStorage(owner0 == e0, "Dense owner tracking is incorrect");
    ok &= CheckComponentStorage(storage.ComponentAtDenseIndex(storage.Count()) == nullptr,
        "Out-of-range dense access must return null");
    ok &= CheckComponentStorage(!storage.OwnerAtDenseIndex(storage.Count()).IsValid(),
        "Out-of-range owner access must return invalid handle");

    ok &= CheckComponentStorage(TrackedComponent::liveCount == 3,
        "Unexpected live component count before shutdown");

    storage.Shutdown();
    ok &= CheckComponentStorage(TrackedComponent::liveCount == 0,
        "Storage shutdown did not destroy all components");

    registry.Shutdown();
    ok &= CheckComponentStorage(allocator.OutstandingBytes() == 0,
        "Component storage test leaked allocator-owned memory");

    ok &= CheckComponentStorage(
        TrackedComponent::destructCount
            == TrackedComponent::constructCount + TrackedComponent::moveConstructCount,
        "Component construction/destruction counts do not balance");

    NOC_LOG_INFO("Phase15", "Component storage tests %s", ok ? "PASS" : "FAIL");
    return ok;
}
