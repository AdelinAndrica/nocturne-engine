#include "Runtime/Reflection/BuiltinTypes.h"
#include "Runtime/Reflection/ReflectionRegistry.h"

#include "Core/Math/MathTypes.h"
#include "Runtime/Bounds.h"

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

    enum class TestAccess : uint32_t
    {
        None = 0,
        Read = 1,
        Write = 2,
        ReadWrite = 3
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

    char mutableAttributeText[] = "Registry Value";
    const noc::AttributeMetadata valueAttributes[] = {
        noc::MakeStringAttribute(
            noc::AttributeKind::DisplayName,
            mutableAttributeText),
        noc::MakeRangeAttribute(
            noc::AttributeKind::NumericRange,
            -100.0,
            100.0)
    };

    noc::PropertyMetadata typeAProperties[] = {
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

    typeAProperties[0].attributes = valueAttributes;
    typeAProperties[0].attributeCount = 2;

    const noc::AttributeMetadata typeAAttributes[] = {
        noc::MakeStringAttribute(
            noc::AttributeKind::Category,
            "Tests")
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
    typeA.attributes = typeAAttributes;
    typeA.attributeCount = 1;

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
    mutableAttributeText[0] = 'X';

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

    const noc::AttributeMetadata* displayName =
        registry.FindPropertyAttribute(
            kTypeA,
            foundValue->propertyId,
            noc::AttributeKind::DisplayName);
    const noc::AttributeMetadata* category =
        registry.FindTypeAttribute(
            kTypeA,
            noc::AttributeKind::Category);

    ok &= CheckReflectionRegistry(
        displayName
            && displayName->valueKind
                == noc::AttributeValueKind::String
            && std::strcmp(
                displayName->stringValue,
                "Registry Value") == 0,
        "Registry did not own property attribute string");
    ok &= CheckReflectionRegistry(
        category
            && std::strcmp(category->stringValue, "Tests") == 0,
        "Type attribute lookup failed");

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

    const noc::AttributeMetadata duplicateAttributes[] = {
        noc::MakeStringAttribute(
            noc::AttributeKind::Tooltip,
            "First"),
        noc::MakeStringAttribute(
            noc::AttributeKind::Tooltip,
            "Second")
    };
    noc::TypeMetadata duplicateAttributeType =
        noc::MakeTypeMetadata<RegistryTypeA>(
            noc::TypeId{ 412 },
            "Nocturne.Tests.DuplicateAttribute",
            noc::TypeKind::Struct,
            1);
    duplicateAttributeType.attributes = duplicateAttributes;
    duplicateAttributeType.attributeCount = 2;

    ok &= CheckReflectionRegistry(
        !registry.RegisterType(duplicateAttributeType)
            && registry.LastError()
                == noc::ReflectionRegistryError::DuplicateAttributeKind,
        "Duplicate attribute kinds must be rejected");

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

    noc::ReflectionRegistry builtinRegistry;
    ok &= CheckReflectionRegistry(
        builtinRegistry.Init(allocator, 4),
        "Builtin registry init failed");
    ok &= CheckReflectionRegistry(
        noc::RegisterBuiltinReflectionTypes(builtinRegistry),
        "Builtin reflection registration failed");
    ok &= CheckReflectionRegistry(
        builtinRegistry.TypeCount() == 17,
        "Unexpected builtin reflected type count");

    char mutableEnumValueName[] = "Read";
    const noc::EnumValueMetadata enumValues[] = {
        noc::MakeEnumValueMetadata<TestAccess>(
            noc::MakeEnumValueId("Nocturne.Tests.TestAccess.None"),
            "None",
            TestAccess::None),
        noc::MakeEnumValueMetadata<TestAccess>(
            noc::MakeEnumValueId("Nocturne.Tests.TestAccess.Read"),
            mutableEnumValueName,
            TestAccess::Read),
        noc::MakeEnumValueMetadata<TestAccess>(
            noc::MakeEnumValueId("Nocturne.Tests.TestAccess.Write"),
            "Write",
            TestAccess::Write),
        noc::MakeEnumValueMetadata<TestAccess>(
            noc::MakeEnumValueId("Nocturne.Tests.TestAccess.ReadWrite"),
            "ReadWrite",
            TestAccess::ReadWrite)
    };

    const noc::EnumMetadata enumMetadata{
        noc::BuiltinTypeIds::UInt32,
        enumValues,
        4,
        true
    };

    noc::TypeMetadata enumType =
        noc::MakeTypeMetadata<TestAccess>(
            noc::TypeId{ 0x2000000000000001ull },
            "Nocturne.Tests.TestAccess",
            noc::TypeKind::Enum,
            1,
            noc::TypeFlags::EditorVisible
                | noc::TypeFlags::Serializable);
    enumType.enumMetadata = &enumMetadata;

    ok &= CheckReflectionRegistry(
        builtinRegistry.RegisterType(enumType),
        "Enum registration failed");

    mutableEnumValueName[0] = 'X';

    const noc::EnumValueMetadata* reflectedRead =
        builtinRegistry.FindEnumValueByName(
            enumType.typeId,
            "Read");

    ok &= CheckReflectionRegistry(
        reflectedRead
            && reflectedRead->valueId
                == noc::MakeEnumValueId(
                    "Nocturne.Tests.TestAccess.Read")
            && reflectedRead->rawValue == 1,
        "Enum registry did not own/value metadata correctly");

    ok &= CheckReflectionRegistry(
        builtinRegistry.FindEnumValueByRawValue(
            enumType.typeId, 3)
            == builtinRegistry.FindEnumValue(
                enumType.typeId,
                noc::MakeEnumValueId(
                    "Nocturne.Tests.TestAccess.ReadWrite")),
        "Enum value lookup mismatch");

    const noc::EnumValueMetadata duplicateEnumIdValues[] = {
        noc::MakeEnumValueMetadata<TestAccess>(
            noc::EnumValueId{ 1 }, "A", TestAccess::Read),
        noc::MakeEnumValueMetadata<TestAccess>(
            noc::EnumValueId{ 1 }, "B", TestAccess::Write)
    };
    const noc::EnumMetadata duplicateEnumIdMetadata{
        noc::BuiltinTypeIds::UInt32,
        duplicateEnumIdValues,
        2,
        false
    };
    noc::TypeMetadata duplicateEnumId =
        noc::MakeTypeMetadata<TestAccess>(
            noc::TypeId{ 0x2000000000000002ull },
            "Nocturne.Tests.DuplicateEnumId",
            noc::TypeKind::Enum,
            1);
    duplicateEnumId.enumMetadata = &duplicateEnumIdMetadata;

    ok &= CheckReflectionRegistry(
        !builtinRegistry.RegisterType(duplicateEnumId)
            && builtinRegistry.LastError()
                == noc::ReflectionRegistryError::DuplicateEnumValueId,
        "Duplicate enum value IDs must be rejected");

    const noc::EnumValueMetadata duplicateNumericValues[] = {
        noc::MakeEnumValueMetadata<TestAccess>(
            noc::EnumValueId{ 2 }, "A", TestAccess::Read),
        noc::MakeEnumValueMetadata<TestAccess>(
            noc::EnumValueId{ 3 }, "B", TestAccess::Read)
    };
    const noc::EnumMetadata duplicateNumericMetadata{
        noc::BuiltinTypeIds::UInt32,
        duplicateNumericValues,
        2,
        false
    };
    noc::TypeMetadata duplicateNumeric =
        noc::MakeTypeMetadata<TestAccess>(
            noc::TypeId{ 0x2000000000000003ull },
            "Nocturne.Tests.DuplicateEnumNumeric",
            noc::TypeKind::Enum,
            1);
    duplicateNumeric.enumMetadata = &duplicateNumericMetadata;

    ok &= CheckReflectionRegistry(
        !builtinRegistry.RegisterType(duplicateNumeric)
            && builtinRegistry.LastError()
                == noc::ReflectionRegistryError::DuplicateEnumNumericValue,
        "Duplicate enum numeric values must be rejected");

    const noc::TypeMetadata* vec3 =
        builtinRegistry.FindType(noc::BuiltinTypeIds::Vec3);
    const noc::PropertyMetadata* vec3X =
        builtinRegistry.FindPropertyByName(
            noc::BuiltinTypeIds::Vec3, "x");

    ok &= CheckReflectionRegistry(
        vec3
            && vec3->kind == noc::TypeKind::Struct
            && vec3->propertyCount == 3
            && vec3X
            && vec3X->valueTypeId == noc::BuiltinTypeIds::Float32,
        "Vec3 builtin schema mismatch");

    noc::Vec3 vector{ 1.0f, 2.0f, 3.0f };
    float xValue = 0.0f;
    noc::PropertyAccessContext vectorContext{};
    vectorContext.object = &vector;
    vectorContext.mutableObject = &vector;

    ok &= CheckReflectionRegistry(
        vec3X->read(vectorContext, &xValue)
            && xValue == 1.0f,
        "Vec3.x reflected read failed");

    const float newX = 9.0f;
    ok &= CheckReflectionRegistry(
        vec3X->write(vectorContext, &newX)
            && vector.x == 9.0f,
        "Vec3.x reflected write failed");

    const noc::TypeMetadata* aabb =
        builtinRegistry.FindType(noc::BuiltinTypeIds::AABB);
    const noc::TypeMetadata* mat4 =
        builtinRegistry.FindType(noc::BuiltinTypeIds::Mat4);

    ok &= CheckReflectionRegistry(
        aabb
            && aabb->kind == noc::TypeKind::Struct
            && aabb->propertyCount == 2
            && aabb->properties[0].valueTypeId
                == noc::BuiltinTypeIds::Vec3,
        "AABB nested schema mismatch");

    ok &= CheckReflectionRegistry(
        mat4
            && mat4->kind == noc::TypeKind::Opaque
            && mat4->propertyCount == 0,
        "Mat4 opaque reflection policy mismatch");

    // Registration uses stack-local property arrays; lookup after the helper
    // returned proves registry ownership rather than descriptor borrowing.
    ok &= CheckReflectionRegistry(
        builtinRegistry.FindPropertyByName(
            noc::BuiltinTypeIds::Vec4, "w") != nullptr,
        "Temporary builtin property descriptors were not owned");

    ok &= CheckReflectionRegistry(
        builtinRegistry.Freeze(),
        "Builtin/enum reflection Freeze failed");

    const noc::TypeMetadata* reflectedEnum =
        builtinRegistry.FindType(enumType.typeId);
    ok &= CheckReflectionRegistry(
        reflectedEnum
            && reflectedEnum->enumMetadata
            && reflectedEnum->enumMetadata->isFlags
            && reflectedEnum->enumMetadata->underlyingTypeId
                == noc::BuiltinTypeIds::UInt32,
        "Frozen enum metadata mismatch");

    const std::size_t builtinAllocationsBefore =
        allocator.AllocationCount();
    const std::size_t builtinBytesBefore =
        allocator.TotalAllocatedBytes();

    for (uint32_t i = 0; i < 10000; ++i)
    {
        ok &= CheckReflectionRegistry(
            builtinRegistry.FindProperty(
                noc::BuiltinTypeIds::Vec3,
                noc::MakePropertyId("Nocturne.Vec3.z")) != nullptr,
            "Frozen builtin property lookup failed");
    }

    ok &= CheckReflectionRegistry(
        allocator.AllocationCount() == builtinAllocationsBefore
            && allocator.TotalAllocatedBytes() == builtinBytesBefore,
        "Frozen builtin lookup allocated memory");

    builtinRegistry.Shutdown();
    ok &= CheckReflectionRegistry(
        allocator.OutstandingBytes() == 0,
        "Builtin/enum reflection leaked allocator memory");

    noc::ReflectionRegistry invalidEnumRegistry;
    ok &= CheckReflectionRegistry(
        invalidEnumRegistry.Init(allocator, 1),
        "Invalid enum registry init failed");

    const noc::EnumValueMetadata loneEnumValue =
        noc::MakeEnumValueMetadata<TestAccess>(
            noc::EnumValueId{ 10 }, "Read", TestAccess::Read);
    const noc::EnumMetadata unresolvedUnderlying{
        noc::TypeId{ 0xDEADBEEFull },
        &loneEnumValue,
        1,
        false
    };
    noc::TypeMetadata unresolvedEnum =
        noc::MakeTypeMetadata<TestAccess>(
            noc::TypeId{ 0x2000000000000010ull },
            "Nocturne.Tests.UnresolvedEnumUnderlying",
            noc::TypeKind::Enum,
            1);
    unresolvedEnum.enumMetadata = &unresolvedUnderlying;

    ok &= CheckReflectionRegistry(
        invalidEnumRegistry.RegisterType(unresolvedEnum),
        "Unresolved enum underlying type should register while Building");
    ok &= CheckReflectionRegistry(
        !invalidEnumRegistry.Freeze()
            && invalidEnumRegistry.LastError()
                == noc::ReflectionRegistryError::UnknownEnumUnderlyingType,
        "Freeze must reject unresolved enum underlying type");

    invalidEnumRegistry.Shutdown();
    ok &= CheckReflectionRegistry(
        allocator.OutstandingBytes() == 0,
        "Invalid enum Freeze path leaked allocator memory");

    NOC_LOG_INFO(
        "Phase16",
        "Reflection registry tests %s",
        ok ? "PASS" : "FAIL");
    return ok;
}
