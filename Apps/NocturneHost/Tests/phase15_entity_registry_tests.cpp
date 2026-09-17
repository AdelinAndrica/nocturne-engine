#include "Runtime/EntityRegistry.h"

#include "Core/Log.h"
#include "Core/Memory/Allocator.h"
#include "Core/Memory/DebugAlloc.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace
{
    bool Check(bool condition, const char* message)
    {
        if (!condition)
        {
            NOC_LOG_ERROR("Phase15", "%s", message);
            return false;
        }
        return true;
    }
}

bool RunPhase15EntityRegistryTests()
{
    NOC_LOG_INFO("Phase15", "%s", "Entity registry tests begin");

    noc::MallocAllocator backing;
    noc::DebugAlloc allocator(backing);
    noc::EntityRegistry registry;

    bool ok = true;
    ok &= Check(!noc::EntityHandle{}.IsValid(), "Default EntityHandle must be invalid");
    ok &= Check(registry.Init(allocator, 4), "EntityRegistry::Init failed");
    ok &= Check(registry.Capacity() >= 4, "Initial registry capacity is too small");
    ok &= Check(registry.AliveCount() == 0, "Registry must begin empty");

    const noc::EntityHandle e0 = registry.Create();
    const noc::EntityHandle e1 = registry.Create();
    const noc::EntityHandle e2 = registry.Create();
    const noc::EntityHandle e3 = registry.Create();

    ok &= Check(e0.IsValid() && e0.index == 0, "First entity must use slot 0");
    ok &= Check(e1.IsValid() && e1.index == 1, "Second entity must use slot 1");
    ok &= Check(e2.IsValid() && e2.index == 2, "Third entity must use slot 2");
    ok &= Check(e3.IsValid() && e3.index == 3, "Fourth entity must use slot 3");
    ok &= Check(registry.AliveCount() == 4, "AliveCount mismatch after creation");

    const noc::EntityHandle e4 = registry.Create();
    ok &= Check(e4.IsValid() && e4.index == 4, "Capacity growth must continue with slot 4");
    ok &= Check(registry.Capacity() >= 5, "Registry did not grow for the fifth entity");

    ok &= Check(registry.Destroy(e1), "Destroy of live entity failed");
    ok &= Check(!registry.IsAlive(e1), "Destroyed handle must become stale");
    ok &= Check(registry.AliveCount() == 4, "AliveCount mismatch after destroy");
    ok &= Check(!registry.Destroy(e1), "Double destroy must be rejected");
    ok &= Check(!registry.Destroy(noc::EntityHandle::Invalid()), "Invalid destroy must be rejected");

    const noc::EntityHandle reused = registry.Create();
    ok &= Check(reused.IsValid(), "Reused entity handle is invalid");
    ok &= Check(reused.index == e1.index, "Free-list should reuse the destroyed slot");
    ok &= Check(reused.generation != e1.generation, "Reused slot must have a new generation");
    ok &= Check(registry.IsAlive(reused), "Reused handle must be alive");
    ok &= Check(!registry.IsAlive(e1), "Old handle must remain stale after slot reuse");

    ok &= Check(registry.EntityAtIndex(reused.index) == reused,
        "EntityAtIndex must return the live generation");
    ok &= Check(!registry.EntityAtIndex(999999u).IsValid(),
        "EntityAtIndex must reject out-of-range indices");

    // Capacity / stale-handle stress: create 10k entities, destroy half, then
    // refill exactly those free slots. Old handles must never become valid.
    constexpr uint32_t kStressCount = 10000;
    std::vector<noc::EntityHandle> stress;
    stress.reserve(kStressCount);

    for (uint32_t i = 0; i < kStressCount; ++i)
    {
        const noc::EntityHandle entity = registry.Create();
        if (!Check(entity.IsValid(), "Stress create returned invalid handle"))
        {
            ok = false;
            break;
        }
        stress.push_back(entity);
    }

    for (std::size_t i = 0; i < stress.size(); i += 2)
        ok &= Check(registry.Destroy(stress[i]), "Stress destroy failed");

    std::vector<noc::EntityHandle> replacements;
    replacements.reserve((stress.size() + 1u) / 2u);
    for (std::size_t i = 0; i < stress.size(); i += 2)
    {
        const noc::EntityHandle entity = registry.Create();
        ok &= Check(entity.IsValid(), "Stress replacement create failed");
        replacements.push_back(entity);
    }

    for (std::size_t i = 0; i < stress.size(); ++i)
    {
        if ((i & 1u) == 0u)
            ok &= Check(!registry.IsAlive(stress[i]), "Destroyed stress handle resurrected");
        else
            ok &= Check(registry.IsAlive(stress[i]), "Live stress handle became invalid");
    }

    for (const noc::EntityHandle entity : replacements)
        ok &= Check(registry.IsAlive(entity), "Replacement handle is not alive");

    registry.Shutdown();
    ok &= Check(allocator.OutstandingBytes() == 0,
        "EntityRegistry leaked allocator-owned memory");

    NOC_LOG_INFO("Phase15", "Entity registry tests %s", ok ? "PASS" : "FAIL");
    return ok;
}
