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
        [[nodiscard]] bool operator==(const RegistryTypeA&) const = default;
    };

    struct alignas(64) RegistryTypeB
    {
        uint64_t values[8]{};
        [[nodiscard]] bool operator==(const RegistryTypeB&) const = default;
    };

    struct RegistryTypeC
    {
        float value = 0.0f;
        [[nodiscard]] bool operator==(const RegistryTypeC&) const = default;
    };
}

bool RunPhase16ReflectionRegistryTests()
{
    NOC_LOG_INFO("Phase16", "%s", "Reflection registry tests begin");

    noc::MallocAllocator backing;
    noc::DebugAlloc allocator(backing);
    noc::ReflectionRegistry registry;

    bool ok = true;

    ok &= CheckReflectionRegistry(
        registry.State() == noc::ReflectionRegistryState::Uninitialized,
        "Registry must begin Uninitialized");
    ok &= CheckReflectionRegistry(
        !registry.Freeze()
            && registry.LastError()
                == noc::ReflectionRegistryError::NotInitialized,
        "Freeze before Init must fail");
    ok &= CheckReflectionRegistry(
        registry.Init(allocator, 1),
        "ReflectionRegistry::Init failed");

    constexpr noc::TypeId kIntType{ 50 };
    constexpr noc::TypeId kTypeA{ 100 };
    constexpr noc::TypeId kTypeB{ 300 };
    constexpr noc::TypeId kTypeC{ 200 };

    const noc::TypeMetadata intType =
        noc::MakeTypeMetadata<int>(
            kIntType,
            "Nocturne.Tests.Int",
            noc::TypeKind::SignedInteger,
            1);

    char mutableTypeName[] = "Nocturne.Tests.RegistryA";
    char mutablePropertyName[] = "value";

    const noc::PropertyMetadata typeAProperties[] = {
        noc::MakeMemberPropertyMetadata<
            RegistryTypeA,
            int,
            &RegistryTypeA::value>(
                noc::MakePropertyId("Nocturne.Tests.RegistryA.value"),
                mutablePropertyName,
                kTypeA,
                kIntType,
                noc::PropertyFlags::EditorVisible
                    | noc::PropertyFlags::Serializable)
    };

    noc::TypeMetadata typeA =
        noc::MakeTypeMetadata<RegistryTypeA>(
            kTypeA,
            mutableTypeName,
            noc::TypeKind::Struct,
            1,
            noc::TypeFlags::Serializable);
    typeA.properties = typeAProperties;
    typeA.propertyCount = 1;

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

    ok &= CheckReflectionRegistry(
        registry.RegisterType(typeB)
            && registry.RegisterType(typeA)
            && registry.RegisterType(intType)
            && registry.RegisterType(typeC),
        "Out-of-order registration failed");
    ok &= CheckReflectionRegistry(
        registry.TypeCount() == 4,
        "Registry type count mismatch");

    mutableTypeName[0] = 'X';
    mutablePropertyName[0] = 'X';

    const noc::TypeMetadata* foundA = registry.FindType(kTypeA);
    const noc::PropertyMetadata* foundValue =
        registry.FindProperty(
            kTypeA,
            noc::MakePropertyId("Nocturne.Tests.RegistryA.value"));

    ok &= CheckReflectionRegistry(
        foundA
            && std::strcmp(
                foundA->canonicalName,
                "Nocturne.Tests.RegistryA") == 0,
        "Registry did not own type canonical name");
    ok &= CheckReflectionRegistry(
        foundValue
            && std::strcmp(foundValue->canonicalName, "value") == 0,
        "Registry did not deep-copy property descriptor/name");

    RegistryTypeA object{};
    object.value = 7;
    int reflectedValue = 0;
    noc::PropertyAccessContext context{};
    context.object = &object;
    context.mutableObject = &object;

    ok &= CheckReflectionRegistry(
        foundValue->read(context, &reflectedValue)
            && reflectedValue == 7,
        "Plain property read adapter failed");

    const int replacement = 42;
    ok &= CheckReflectionRegistry(
        foundValue->write(context, &replacement)
            && object.value == 42,
        "Plain property write adapter failed");
    ok &= CheckReflectionRegistry(
        foundValue->constAddress(context) == &object.value
            && foundValue->mutableAddress(context) == &object.value,
        "Plain property address adapters failed");

    ok &= CheckReflectionRegistry(
        registry.TypeAt(0)->typeId == kIntType
            && registry.TypeAt(1)->typeId == kTypeA
            && registry.TypeAt(2)->typeId == kTypeC
            && registry.TypeAt(3)->typeId == kTypeB,
        "Type enumeration is not sorted by TypeId");

    noc::TypeMetadata duplicateName = typeC;
    duplicateName.typeId = noc::TypeId{ 400 };
    duplicateName.canonicalName = "Nocturne.Tests.RegistryB";
    ok &= CheckReflectionRegistry(
        !registry.RegisterType(duplicateName)
            && registry.LastError()
                == noc::ReflectionRegistryError::DuplicateCanonicalName,
        "Duplicate canonical type name must be rejected");

    const noc::PropertyMetadata duplicateIdProperties[] = {
        noc::MakeMemberPropertyMetadata<
            RegistryTypeA, int, &RegistryTypeA::value>(
                noc::PropertyId{ 1 }, "a", noc::TypeId{ 410 }, kIntType),
        noc::MakeMemberPropertyMetadata<
            RegistryTypeA, int, &RegistryTypeA::value>(
                noc::PropertyId{ 1 }, "b", noc::TypeId{ 410 }, kIntType)
    };
    noc::TypeMetadata duplicatePropertyId =
        noc::MakeTypeMetadata<RegistryTypeA>(
            noc::TypeId{ 410 },
            "Nocturne.Tests.DuplicatePropertyId",
            noc::TypeKind::Struct,
            1);
    duplicatePropertyId.properties = duplicateIdProperties;
    duplicatePropertyId.propertyCount = 2;

    ok &= CheckReflectionRegistry(
        !registry.RegisterType(duplicatePropertyId)
            && registry.LastError()
                == noc::ReflectionRegistryError::DuplicatePropertyId,
        "Duplicate PropertyId must be rejected");

    const noc::PropertyMetadata duplicateNameProperties[] = {
        noc::MakeMemberPropertyMetadata<
            RegistryTypeA, int, &RegistryTypeA::value>(
                noc::PropertyId{ 2 }, "value", noc::TypeId{ 411 }, kIntType),
        noc::MakeMemberPropertyMetadata<
            RegistryTypeA, int, &RegistryTypeA::value>(
                noc::PropertyId{ 3 }, "value", noc::TypeId{ 411 }, kIntType)
    };
    noc::TypeMetadata duplicatePropertyName =
        noc::MakeTypeMetadata<RegistryTypeA>(
            noc::TypeId{ 411 },
            "Nocturne.Tests.DuplicatePropertyName",
            noc::TypeKind::Struct,
            1);
    duplicatePropertyName.properties = duplicateNameProperties;
    duplicatePropertyName.propertyCount = 2;

    ok &= CheckReflectionRegistry(
        !registry.RegisterType(duplicatePropertyName)
            && registry.LastError()
                == noc::ReflectionRegistryError::
                    DuplicatePropertyCanonicalName,
        "Duplicate property canonical name must be rejected");

    ok &= CheckReflectionRegistry(
        registry.Freeze(),
        "Registry Freeze failed");
    ok &= CheckReflectionRegistry(
        registry.IsFrozen(),
        "Registry did not enter Frozen state");

    const std::size_t allocationsBeforeLookup =
        allocator.AllocationCount();
    const std::size_t bytesBeforeLookup =
        allocator.TotalAllocatedBytes();

    for (uint32_t i = 0; i < 10000; ++i)
    {
        ok &= CheckReflectionRegistry(
            registry.FindType(kTypeA) == foundA
                && registry.FindProperty(
                    kTypeA,
                    noc::MakePropertyId(
                        "Nocturne.Tests.RegistryA.value")) == foundValue,
            "Frozen type/property lookup changed metadata address");
    }

    ok &= CheckReflectionRegistry(
        allocator.AllocationCount() == allocationsBeforeLookup
            && allocator.TotalAllocatedBytes() == bytesBeforeLookup,
        "Frozen lookup allocated memory");

    registry.Shutdown();
    ok &= CheckReflectionRegistry(
        allocator.OutstandingBytes() == 0,
        "ReflectionRegistry leaked allocator memory");

    // Cross-type property references are validated at Freeze so registration
    // order remains irrelevant.
    noc::ReflectionRegistry invalidRegistry;
    ok &= CheckReflectionRegistry(
        invalidRegistry.Init(allocator, 1),
        "Invalid-schema registry init failed");

    const noc::PropertyMetadata unresolvedProperty =
        noc::MakeMemberPropertyMetadata<
            RegistryTypeA, int, &RegistryTypeA::value>(
                noc::PropertyId{ 77 },
                "value",
                noc::TypeId{ 500 },
                noc::TypeId{ 999999 });

    noc::TypeMetadata unresolvedType =
        noc::MakeTypeMetadata<RegistryTypeA>(
            noc::TypeId{ 500 },
            "Nocturne.Tests.UnresolvedValueType",
            noc::TypeKind::Struct,
            1);
    unresolvedType.properties = &unresolvedProperty;
    unresolvedType.propertyCount = 1;

    ok &= CheckReflectionRegistry(
        invalidRegistry.RegisterType(unresolvedType),
        "Unresolved value TypeId should be accepted while Building");
    ok &= CheckReflectionRegistry(
        !invalidRegistry.Freeze()
            && invalidRegistry.LastError()
                == noc::ReflectionRegistryError::UnknownPropertyValueType,
        "Freeze must reject unresolved property value TypeId");

    invalidRegistry.Shutdown();
    ok &= CheckReflectionRegistry(
        allocator.OutstandingBytes() == 0,
        "Failed Freeze path leaked allocator memory");

    NOC_LOG_INFO(
        "Phase16",
        "Reflection registry tests %s",
        ok ? "PASS" : "FAIL");
    return ok;
}
