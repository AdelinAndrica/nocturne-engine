#include "Runtime/NameSystem.h"

#include "Core/Log.h"
#include "Core/Memory/Allocator.h"
#include "Core/Memory/DebugAlloc.h"
#include "Runtime/EntityRegistry.h"

#include <cstring>

namespace
{
    bool CheckName(bool condition, const char* message)
    {
        if (!condition)
        {
            NOC_LOG_ERROR("Phase15", "%s", message);
            return false;
        }
        return true;
    }
}

bool RunPhase15NameTests()
{
    NOC_LOG_INFO("Phase15", "%s", "Name component tests begin");

    noc::MallocAllocator backing;
    noc::DebugAlloc allocator(backing);
    noc::EntityRegistry entities;
    noc::NameSystem names;

    bool ok = true;

    ok &= CheckName(entities.Init(allocator, 4), "EntityRegistry init failed");
    ok &= CheckName(names.Init(entities, allocator, 2), "NameSystem init failed");

    const noc::EntityHandle e0 = entities.Create();
    const noc::EntityHandle e1 = entities.Create();
    const noc::EntityHandle e2 = entities.Create();

    noc::NameComponent* n0 = names.Add(e0);
    ok &= CheckName(n0 != nullptr, "Add default NameComponent failed");
    ok &= CheckName(n0 && n0->value[0] == '\0', "Default name must be empty");
    ok &= CheckName(names.Count() == 1, "Name count mismatch after add");

    ok &= CheckName(
        names.Add(e0, "Duplicate") == nullptr,
        "Duplicate NameComponent add must fail");

    char mutableSource[] = "Cube_A";
    ok &= CheckName(names.SetName(e0, mutableSource), "SetName(Cube_A) failed");
    mutableSource[0] = 'X';

    const noc::NameComponent* stored = names.Get(e0);
    ok &= CheckName(
        stored && std::strcmp(stored->value, "Cube_A") == 0,
        "NameSystem did not own/copy name bytes");

    ok &= CheckName(names.SetName(e0, ""), "Empty name must be accepted");
    stored = names.Get(e0);
    ok &= CheckName(
        stored && stored->value[0] == '\0',
        "Empty name did not persist");

    ok &= CheckName(
        !names.SetName(e0, nullptr),
        "Null name must be rejected");

    const char validUtf8[] = {
        'N', 'a', 'm', 'e', ' ',
        static_cast<char>(0xE2),
        static_cast<char>(0x98),
        static_cast<char>(0x83),
        '\0'
    };

    ok &= CheckName(
        names.SetName(e0, validUtf8),
        "Valid UTF-8 name was rejected");
    ok &= CheckName(
        std::strcmp(names.Get(e0)->value, validUtf8) == 0,
        "Valid UTF-8 name did not persist");

    const char invalidUtf8[] = {
        'B', 'a', 'd', ' ',
        static_cast<char>(0xE2),
        static_cast<char>(0x28),
        static_cast<char>(0xA1),
        '\0'
    };

    ok &= CheckName(
        !names.SetName(e0, invalidUtf8),
        "Invalid UTF-8 name was accepted");
    ok &= CheckName(
        std::strcmp(names.Get(e0)->value, validUtf8) == 0,
        "Rejected invalid UTF-8 name mutated previous value");

    // Exact maximum payload (63 bytes) is valid.
    char maxName[noc::kNameComponentCapacity]{};
    for (uint32_t i = 0; i < noc::kNameComponentMaxBytes; ++i)
        maxName[i] = 'A';
    maxName[noc::kNameComponentMaxBytes] = '\0';

    ok &= CheckName(
        names.SetName(e0, maxName),
        "Maximum-length name was rejected");
    stored = names.Get(e0);
    ok &= CheckName(
        stored
            && std::strlen(stored->value) == noc::kNameComponentMaxBytes,
        "Maximum-length name stored incorrectly");

    // 64 payload bytes exceed the fixed storage and must be rejected without
    // truncating or mutating the previous valid value.
    char tooLong[noc::kNameComponentCapacity + 1u]{};
    for (uint32_t i = 0; i < noc::kNameComponentCapacity; ++i)
        tooLong[i] = 'B';
    tooLong[noc::kNameComponentCapacity] = '\0';

    ok &= CheckName(
        !names.SetName(e0, tooLong),
        "Over-limit name was accepted");
    stored = names.Get(e0);
    ok &= CheckName(
        stored
            && std::strlen(stored->value) == noc::kNameComponentMaxBytes
            && stored->value[0] == 'A',
        "Rejected over-limit rename mutated the old name");

    // Duplicate names are explicitly legal: identity is EntityHandle, not name.
    ok &= CheckName(names.Add(e1, "SharedName") != nullptr, "Add e1 name failed");
    ok &= CheckName(names.Add(e2, "SharedName") != nullptr, "Duplicate display name was rejected");
    ok &= CheckName(
        std::strcmp(names.Get(e1)->value, names.Get(e2)->value) == 0,
        "Duplicate display names did not persist");

    // Dense storage must survive growth and swap-remove.
    ok &= CheckName(names.Count() == 3, "Name count mismatch before remove");
    ok &= CheckName(names.Remove(e1), "Remove(e1) failed");
    ok &= CheckName(!names.Has(e1), "Removed name still exists");
    ok &= CheckName(
        names.Get(e2) && std::strcmp(names.Get(e2)->value, "SharedName") == 0,
        "Swap-remove corrupted moved NameComponent");
    ok &= CheckName(
        names.DenseCount() == names.Count(),
        "Dense name count mismatch");
    ok &= CheckName(
        names.OwnerAtDenseIndex(0).IsValid(),
        "Dense name owner invalid");
    ok &= CheckName(
        names.ComponentAtDenseIndex(names.DenseCount()) == nullptr,
        "Out-of-range dense name access must return null");

    // Stale handles must be rejected.
    const noc::EntityHandle stale = entities.Create();
    ok &= CheckName(names.Add(stale, "Temporary") != nullptr, "Add stale-test name failed");
    ok &= CheckName(names.Remove(stale), "Remove stale-test name failed");
    ok &= CheckName(entities.Destroy(stale), "Destroy stale-test entity failed");
    ok &= CheckName(
        names.Add(stale, "Stale") == nullptr,
        "Stale entity accepted for Name add");
    ok &= CheckName(
        !names.SetName(stale, "Stale"),
        "Stale entity accepted for rename");

    // Metadata contract for the concrete foundation component.
    constexpr noc::ComponentTypeMetadata metadata =
        noc::NameComponentMetadata();

    ok &= CheckName(
        metadata.typeId == noc::kNameComponentTypeId,
        "NameComponent metadata type ID mismatch");
    ok &= CheckName(
        metadata.version == noc::kNameComponentVersion,
        "NameComponent metadata version mismatch");
    ok &= CheckName(
        metadata.size == sizeof(noc::NameComponent),
        "NameComponent metadata size mismatch");
    ok &= CheckName(
        metadata.alignment == alignof(noc::NameComponent),
        "NameComponent metadata alignment mismatch");

    ok &= CheckName(names.Remove(e0), "Remove(e0) failed");
    ok &= CheckName(names.Remove(e2), "Remove(e2) failed");
    ok &= CheckName(names.Count() == 0, "Name storage not empty");

    names.Shutdown();
    entities.Shutdown();

    ok &= CheckName(
        allocator.OutstandingBytes() == 0,
        "Name tests leaked allocator-owned memory");

    NOC_LOG_INFO("Phase15", "Name component tests %s", ok ? "PASS" : "FAIL");
    return ok;
}
