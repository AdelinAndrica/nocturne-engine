#include "Runtime/Reflection/ReflectionRegistry.h"

#include "Core/Log.h"
#include "Core/Memory/Allocator.h"
#include "Core/Memory/DebugAlloc.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace
{
    bool CheckReflectionRegistry(bool condition, const char* message)
    {
        if (!condition)
        {
            NOC_LOG_ERROR("Phase16", "%s", message);
            return false;
        }

        return true;
    }

    struct RegistryTypeA
    {
        int value = 0;

        [[nodiscard]] bool operator==(
            const RegistryTypeA&) const = default;
    };

    struct alignas(64) RegistryTypeB
    {
        uint64_t values[8]{};

        [[nodiscard]] bool operator==(
            const RegistryTypeB&) const = default;
    };

    struct RegistryTypeC
    {
        float value = 0.0f;

        [[nodiscard]] bool operator==(
            const RegistryTypeC&) const = default;
    };
}

bool RunPhase16ReflectionRegistryTests()
{
    NOC_LOG_INFO(
        "Phase16",
        "%s",
        "Reflection registry tests begin");

    noc::MallocAllocator backing;
    noc::DebugAlloc allocator(backing);
    noc::ReflectionRegistry registry;

    bool ok = true;

    ok &= CheckReflectionRegistry(
        registry.State()
            == noc::ReflectionRegistryState::Uninitialized,
        "Registry must begin Uninitialized");

    ok &= CheckReflectionRegistry(
        !registry.Freeze()
            && registry.LastError()
                == noc::ReflectionRegistryError::NotInitialized,
        "Freeze before Init must fail");

    ok &= CheckReflectionRegistry(
        registry.Init(allocator, 1),
        "ReflectionRegistry::Init failed");

    ok &= CheckReflectionRegistry(
        registry.State()
            == noc::ReflectionRegistryState::Building,
        "Registry did not enter Building state");

    constexpr noc::TypeId kTypeA{ 100 };
    constexpr noc::TypeId kTypeB{ 300 };
    constexpr noc::TypeId kTypeC{ 200 };

    char mutableName[] = "Nocturne.Tests.RegistryA";

    const noc::TypeMetadata typeA =
        noc::MakeTypeMetadata<RegistryTypeA>(
            kTypeA,
            mutableName,
            noc::TypeKind::Struct,
            1,
            noc::TypeFlags::Serializable);

    const noc::TypeMetadata typeB =
        noc::MakeTypeMetadata<RegistryTypeB>(
            kTypeB,
            "Nocturne.Tests.RegistryB",
            noc::TypeKind::Struct,
            2,
            noc::TypeFlags::EditorVisible);

    const noc::TypeMetadata typeC =
        noc::MakeTypeMetadata<RegistryTypeC>(
            kTypeC,
            "Nocturne.Tests.RegistryC",
            noc::TypeKind::Struct,
            1);

    // Deliberately register out of order.
    ok &= CheckReflectionRegistry(
        registry.RegisterType(typeB),
        "RegistryB registration failed");
    ok &= CheckReflectionRegistry(
        registry.RegisterType(typeA),
        "RegistryA registration failed");
    ok &= CheckReflectionRegistry(
        registry.RegisterType(typeC),
        "RegistryC registration failed");

    ok &= CheckReflectionRegistry(
        registry.TypeCount() == 3,
        "Registry type count mismatch");

    mutableName[0] = 'X';

    const noc::TypeMetadata* foundA =
        registry.FindType(kTypeA);

    ok &= CheckReflectionRegistry(
        foundA != nullptr
            && std::strcmp(
                foundA->canonicalName,
                "Nocturne.Tests.RegistryA") == 0,
        "Registry must own canonical-name storage");

    ok &= CheckReflectionRegistry(
        registry.TypeAt(0)
            && registry.TypeAt(0)->typeId == kTypeA,
        "Type enumeration index 0 is not deterministic");
    ok &= CheckReflectionRegistry(
        registry.TypeAt(1)
            && registry.TypeAt(1)->typeId == kTypeC,
        "Type enumeration index 1 is not deterministic");
    ok &= CheckReflectionRegistry(
        registry.TypeAt(2)
            && registry.TypeAt(2)->typeId == kTypeB,
        "Type enumeration index 2 is not deterministic");

    ok &= CheckReflectionRegistry(
        !registry.RegisterType(typeA)
            && registry.LastError()
                == noc::ReflectionRegistryError::DuplicateTypeId,
        "Duplicate TypeId must be rejected");

    noc::TypeMetadata duplicateName = typeC;
    duplicateName.typeId = noc::TypeId{ 400 };
    duplicateName.canonicalName =
        "Nocturne.Tests.RegistryB";

    ok &= CheckReflectionRegistry(
        !registry.RegisterType(duplicateName)
            && registry.LastError()
                == noc::ReflectionRegistryError::
                    DuplicateCanonicalName,
        "Duplicate canonical name must be rejected");

    noc::TypeMetadata invalid = typeC;
    invalid.typeId = noc::TypeId{ 401 };
    invalid.canonicalName =
        "Nocturne.Tests.InvalidAlignment";
    invalid.alignment = 3;

    ok &= CheckReflectionRegistry(
        !registry.RegisterType(invalid)
            && registry.LastError()
                == noc::ReflectionRegistryError::InvalidMetadata,
        "Invalid metadata must be rejected");

    ok &= CheckReflectionRegistry(
        registry.Freeze(),
        "Registry Freeze failed");
    ok &= CheckReflectionRegistry(
        registry.IsFrozen(),
        "Registry did not enter Frozen state");

    const noc::TypeMetadata* frozenA =
        registry.FindType(kTypeA);

    ok &= CheckReflectionRegistry(
        frozenA == foundA,
        "Metadata address changed at Freeze");

    ok &= CheckReflectionRegistry(
        registry.FindTypeByName(
            "Nocturne.Tests.RegistryB")
            == registry.FindType(kTypeB),
        "FindTypeByName failed");

    const std::size_t allocationsBeforeLookup =
        allocator.AllocationCount();
    const std::size_t bytesBeforeLookup =
        allocator.TotalAllocatedBytes();

    for (uint32_t i = 0; i < 10000; ++i)
    {
        ok &= CheckReflectionRegistry(
            registry.FindType(kTypeA) == frozenA,
            "Frozen TypeId lookup changed address");

        ok &= CheckReflectionRegistry(
            registry.TypeAt(i % 3u) != nullptr,
            "Frozen deterministic enumeration failed");
    }

    ok &= CheckReflectionRegistry(
        allocator.AllocationCount()
            == allocationsBeforeLookup,
        "Frozen lookup/enumeration allocated memory");
    ok &= CheckReflectionRegistry(
        allocator.TotalAllocatedBytes()
            == bytesBeforeLookup,
        "Frozen lookup/enumeration allocated bytes");

    ok &= CheckReflectionRegistry(
        !registry.RegisterType(typeA)
            && registry.LastError()
                == noc::ReflectionRegistryError::WrongState,
        "Registration after Freeze must be rejected");

    ok &= CheckReflectionRegistry(
        !registry.Freeze()
            && registry.LastError()
                == noc::ReflectionRegistryError::WrongState,
        "Second Freeze must be rejected");

    registry.Shutdown();

    ok &= CheckReflectionRegistry(
        registry.State()
            == noc::ReflectionRegistryState::Uninitialized,
        "Shutdown did not restore Uninitialized state");
    ok &= CheckReflectionRegistry(
        allocator.OutstandingBytes() == 0,
        "ReflectionRegistry leaked allocator memory");

    NOC_LOG_INFO(
        "Phase16",
        "Reflection registry tests %s",
        ok ? "PASS" : "FAIL");

    return ok;
}
