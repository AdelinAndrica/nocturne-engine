#include "Runtime/Reflection/BuiltinTypes.h"
#include "Runtime/Reflection/ReflectionRegistry.h"
#include "Runtime/Reflection/ReflectionString.h"
#include "Runtime/Reflection/ReflectedValue.h"
#include "Runtime/Reflection/PropertyAccess.h"
#include "Runtime/Reflection/FunctionInvocation.h"
#include "Runtime/Reflection/ComponentReflection.h"
#include "Runtime/Reflection/FoundationComponents.h"
#include "Runtime/World.h"

#include "Core/Math/MathTypes.h"
#include "Runtime/Bounds.h"

#include "Core/Log.h"
#include "Core/Memory/Allocator.h"
#include "Core/Memory/DebugAlloc.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

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

    struct OwnedTestValue
    {
        static inline int liveCount = 0;

        int value = 0;

        OwnedTestValue() { ++liveCount; }
        OwnedTestValue(const OwnedTestValue& other)
            : value(other.value) { ++liveCount; }
        OwnedTestValue(OwnedTestValue&& other) noexcept
            : value(other.value)
        {
            other.value = -1;
            ++liveCount;
        }
        OwnedTestValue& operator=(const OwnedTestValue&) = default;
        OwnedTestValue& operator=(OwnedTestValue&&) noexcept = default;
        ~OwnedTestValue() { --liveCount; }

        [[nodiscard]] bool operator==(
            const OwnedTestValue& other) const
        {
            return value == other.value;
        }
    };

    struct alignas(64) OwnedOverAligned
    {
        uint64_t words[8]{};
    };

    struct NoDefaultValue
    {
        explicit NoDefaultValue(int inValue) : value(inValue) {}
        int value = 0;
    };

    struct SemanticOwner
    {
        int value = 0;
    };

    bool ReadSemanticInt(
        const noc::PropertyAccessContext& context,
        void* destination)
    {
        if (!context.object || !destination)
            return false;
        *static_cast<int*>(destination) =
            static_cast<const SemanticOwner*>(context.object)->value;
        return true;
    }

    bool WriteSemanticInt(
        noc::PropertyAccessContext& context,
        const void* source)
    {
        if (!context.mutableObject || !source)
            return false;
        static_cast<SemanticOwner*>(context.mutableObject)->value =
            *static_cast<const int*>(source);
        return true;
    }

    bool ValidateNonNegative(
        const noc::PropertyAccessContext&,
        const void* candidate)
    {
        return candidate
            && *static_cast<const int*>(candidate) >= 0;
    }

    struct FixedInt3
    {
        int values[3]{};
    };

    uint32_t FixedCount(const void*) { return 3; }
    uint32_t FixedCapacity(const void*) { return 3; }
    const void* FixedConstElement(const void* container, uint32_t index)
    {
        if (!container || index >= 3)
            return nullptr;
        return &static_cast<const FixedInt3*>(container)->values[index];
    }
    void* FixedMutableElement(void* container, uint32_t index)
    {
        if (!container || index >= 3)
            return nullptr;
        return &static_cast<FixedInt3*>(container)->values[index];
    }

    struct SmallSequence
    {
        int values[8]{};
        uint32_t count = 0;
    };

    uint32_t SequenceCount(const void* container)
    {
        return container
            ? static_cast<const SmallSequence*>(container)->count
            : 0;
    }
    uint32_t SequenceCapacity(const void*) { return 8; }
    const void* SequenceConstElement(const void* container, uint32_t index)
    {
        if (!container)
            return nullptr;
        const auto* sequence = static_cast<const SmallSequence*>(container);
        return index < sequence->count ? &sequence->values[index] : nullptr;
    }
    void* SequenceMutableElement(void* container, uint32_t index)
    {
        if (!container)
            return nullptr;
        auto* sequence = static_cast<SmallSequence*>(container);
        return index < sequence->count ? &sequence->values[index] : nullptr;
    }
    bool SequenceResize(void* container, uint32_t newCount)
    {
        if (!container || newCount > 8)
            return false;
        auto* sequence = static_cast<SmallSequence*>(container);
        for (uint32_t i = sequence->count; i < newCount; ++i)
            sequence->values[i] = 0;
        sequence->count = newCount;
        return true;
    }
    bool SequenceInsertDefault(void* container, uint32_t index)
    {
        if (!container)
            return false;
        auto* sequence = static_cast<SmallSequence*>(container);
        if (sequence->count >= 8 || index > sequence->count)
            return false;
        for (uint32_t i = sequence->count; i > index; --i)
            sequence->values[i] = sequence->values[i - 1u];
        sequence->values[index] = 0;
        ++sequence->count;
        return true;
    }
    bool SequenceRemove(void* container, uint32_t index)
    {
        if (!container)
            return false;
        auto* sequence = static_cast<SmallSequence*>(container);
        if (index >= sequence->count)
            return false;
        for (uint32_t i = index; i + 1u < sequence->count; ++i)
            sequence->values[i] = sequence->values[i + 1u];
        --sequence->count;
        return true;
    }

    struct FunctionOwner
    {
        int base = 0;
    };

    bool InvokeAddWithBase(
        noc::FunctionInvocationContext& context,
        const noc::ReflectedConstValueView* arguments,
        uint32_t argumentCount,
        noc::ReflectedValueView returnValue)
    {
        const void* object =
            context.object ? context.object : context.mutableObject;
        if (!object
            || !arguments
            || argumentCount != 2
            || !returnValue.IsValid())
        {
            return false;
        }

        const auto* owner = static_cast<const FunctionOwner*>(object);
        const int a = *static_cast<const int*>(arguments[0].data);
        const int b = *static_cast<const int*>(arguments[1].data);
        *static_cast<int*>(returnValue.data) = owner->base + a + b;
        return true;
    }

    bool InvokeSetBase(
        noc::FunctionInvocationContext& context,
        const noc::ReflectedConstValueView* arguments,
        uint32_t argumentCount,
        noc::ReflectedValueView returnValue)
    {
        if (!context.mutableObject
            || !arguments
            || argumentCount != 1
            || returnValue.IsValid())
        {
            return false;
        }

        static_cast<FunctionOwner*>(context.mutableObject)->base =
            *static_cast<const int*>(arguments[0].data);
        return true;
    }

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
        builtinRegistry.TypeCount() == 21,
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

    {
        noc::ReflectionRegistry componentRegistry;
        ok &= CheckReflectionRegistry(
            componentRegistry.Init(allocator, 4),
            "Component reflection registry init failed");
        ok &= CheckReflectionRegistry(
            noc::RegisterBuiltinReflectionTypes(componentRegistry)
                && noc::RegisterFoundationComponentReflectionTypes(
                    componentRegistry),
            "Foundation component reflection registration failed");
        ok &= CheckReflectionRegistry(
            componentRegistry.Freeze(),
            "Foundation component reflection Freeze failed");

        ok &= CheckReflectionRegistry(
            componentRegistry.ComponentTypeCount() == 4,
            "Foundation reflected component count mismatch");

        const noc::TypeMetadata* transformType =
            componentRegistry.ComponentTypeAt(0);
        const noc::TypeMetadata* nameType =
            componentRegistry.ComponentTypeAt(3);

        ok &= CheckReflectionRegistry(
            transformType
                && transformType->typeId.value
                    == noc::kTransformComponentTypeId.value
                && transformType->componentMetadata
                && noc::HasFlag(
                    transformType->componentMetadata->flags,
                    noc::ComponentReflectionFlags::EditorAddable),
            "Transform reflected component metadata mismatch");
        ok &= CheckReflectionRegistry(
            nameType
                && nameType->typeId.value
                    == noc::kNameComponentTypeId.value,
            "Component reflection enumeration is not deterministic");

        noc::World componentWorld;
        ok &= CheckReflectionRegistry(
            componentWorld.Init(allocator, componentRegistry),
            "Component reflection test World init failed");

        const noc::EntityHandle componentEntity =
            componentWorld.CreateEntity();
        ok &= CheckReflectionRegistry(
            componentEntity.IsValid()
                && noc::ReflectedComponentCountForEntity(
                    componentRegistry,
                    componentWorld,
                    componentEntity) == 0,
            "Fresh entity reflected component membership mismatch");

        ok &= CheckReflectionRegistry(
            transformType->componentMetadata->add(
                componentWorld,
                componentEntity),
            "Generic reflected Transform add failed");
        ok &= CheckReflectionRegistry(
            nameType->componentMetadata->add(
                componentWorld,
                componentEntity),
            "Generic reflected Name add failed");

        ok &= CheckReflectionRegistry(
            noc::ReflectedComponentCountForEntity(
                componentRegistry,
                componentWorld,
                componentEntity) == 2,
            "Generic reflected component enumeration failed");

        const noc::TypeMetadata* entityComponent0 =
            noc::ReflectedComponentAtForEntity(
                componentRegistry,
                componentWorld,
                componentEntity,
                0);
        const noc::TypeMetadata* entityComponent1 =
            noc::ReflectedComponentAtForEntity(
                componentRegistry,
                componentWorld,
                componentEntity,
                1);

        ok &= CheckReflectionRegistry(
            entityComponent0
                && entityComponent0->typeId.value
                    == noc::kTransformComponentTypeId.value
                && entityComponent1
                && entityComponent1->typeId.value
                    == noc::kNameComponentTypeId.value,
            "Per-entity reflected component order mismatch");

        ok &= CheckReflectionRegistry(
            transformType->componentMetadata->getConst(
                componentWorld,
                componentEntity) != nullptr,
            "Generic reflected component const access failed");

        const noc::PropertyMetadata* translationProperty =
            componentRegistry.FindPropertyByName(
                noc::TypeId{
                    noc::kTransformComponentTypeId.value },
                "localTranslation");

        noc::ComponentPropertyRuntimeContext propertyRuntime{
            &componentWorld,
            componentEntity
        };
        noc::PropertyAccessContext propertyContext =
            noc::MakeComponentPropertyAccessContext(propertyRuntime);

        const noc::Vec3 reflectedTranslation{
            4.0f, 5.0f, 6.0f
        };

        ok &= CheckReflectionRegistry(
            translationProperty
                && noc::WritePropertyValue(
                    *translationProperty,
                    propertyContext,
                    noc::ReflectedConstValueView{
                        noc::BuiltinTypeIds::Vec3,
                        &reflectedTranslation })
                    == noc::PropertyAccessStatus::Success
                && componentWorld.GetTransform(componentEntity)
                && componentWorld.GetTransform(componentEntity)
                    ->localTranslation.x == 4.0f,
            "Semantic Transform property write failed");

        const noc::TypeMetadata* renderableType =
            componentRegistry.FindType(
                noc::TypeId{
                    noc::kRenderableComponentTypeId.value });
        ok &= CheckReflectionRegistry(
            renderableType
                && renderableType->componentMetadata->add(
                    componentWorld,
                    componentEntity),
            "Generic reflected Renderable add failed");

        const noc::PropertyMetadata* meshProperty =
            componentRegistry.FindPropertyByName(
                renderableType->typeId,
                "mesh");
        const noc::AttributeMetadata* meshConstraint =
            componentRegistry.FindPropertyAttribute(
                renderableType->typeId,
                meshProperty->propertyId,
                noc::AttributeKind::ResourceTypeConstraint);

        ok &= CheckReflectionRegistry(
            meshConstraint
                && meshConstraint->valueKind
                    == noc::AttributeValueKind::TypeId
                && meshConstraint->typeIdValue
                    == noc::BuiltinTypeIds::MeshResource
                && componentRegistry.FindType(
                    meshConstraint->typeIdValue) != nullptr,
            "Renderable mesh resource constraint mismatch");

        const noc::ResourceHandle reflectedMesh{ 77u, 9u };
        ok &= CheckReflectionRegistry(
            meshProperty
                && noc::WritePropertyValue(
                    *meshProperty,
                    propertyContext,
                    noc::ReflectedConstValueView{
                        noc::BuiltinTypeIds::ResourceHandle,
                        &reflectedMesh })
                    == noc::PropertyAccessStatus::Success
                && componentWorld.GetRenderable(componentEntity)
                && componentWorld.GetRenderable(componentEntity)->mesh
                    == reflectedMesh,
            "Semantic Renderable mesh write failed");

        const noc::TypeMetadata* cameraType =
            componentRegistry.FindType(
                noc::TypeId{
                    noc::kCameraComponentTypeId.value });
        ok &= CheckReflectionRegistry(
            cameraType
                && cameraType->componentMetadata->add(
                    componentWorld,
                    componentEntity),
            "Generic reflected Camera add failed");

        const noc::PropertyMetadata* nearProperty =
            componentRegistry.FindPropertyByName(
                cameraType->typeId,
                "nearZ");
        float invalidNear = 1000.0f;
        ok &= CheckReflectionRegistry(
            nearProperty
                && noc::WritePropertyValue(
                    *nearProperty,
                    propertyContext,
                    noc::ReflectedConstValueView{
                        noc::BuiltinTypeIds::Float32,
                        &invalidNear })
                    == noc::PropertyAccessStatus::ValidationFailed,
            "Camera near-plane cross-field validation failed");

        float validNear = 0.25f;
        ok &= CheckReflectionRegistry(
            noc::WritePropertyValue(
                *nearProperty,
                propertyContext,
                noc::ReflectedConstValueView{
                    noc::BuiltinTypeIds::Float32,
                    &validNear })
                == noc::PropertyAccessStatus::Success
                && componentWorld.GetCamera(componentEntity)
                && componentWorld.GetCamera(componentEntity)->nearZ
                    == validNear,
            "Semantic Camera near-plane write failed");

        const float nanValue =
            std::numeric_limits<float>::quiet_NaN();
        const noc::Vec3 invalidTranslation{
            nanValue, 0.0f, 0.0f
        };
        ok &= CheckReflectionRegistry(
            noc::WritePropertyValue(
                *translationProperty,
                propertyContext,
                noc::ReflectedConstValueView{
                    noc::BuiltinTypeIds::Vec3,
                    &invalidTranslation })
                == noc::PropertyAccessStatus::WriteFailed,
            "World semantic TRS seam accepted NaN");


        const noc::PropertyMetadata* nameProperty =
            componentRegistry.FindPropertyByName(
                nameType->typeId,
                "value");

        noc::ReflectionString reflectedName(allocator);
        ok &= CheckReflectionRegistry(
            reflectedName.Assign("Reflected Entity"),
            "Reflected string setup failed");
        ok &= CheckReflectionRegistry(
            nameProperty
                && noc::WritePropertyValue(
                    *nameProperty,
                    propertyContext,
                    noc::ReflectedConstValueView{
                        noc::BuiltinTypeIds::String,
                        &reflectedName })
                    == noc::PropertyAccessStatus::Success
                && componentWorld.GetName(componentEntity)
                && std::strcmp(
                    componentWorld.GetName(componentEntity)->value,
                    "Reflected Entity") == 0,
            "Semantic Name property write failed");

        noc::OwnedReflectedValue reflectedNameRead;
        ok &= CheckReflectionRegistry(
            noc::ReadPropertyValue(
                componentRegistry,
                *nameProperty,
                propertyContext,
                allocator,
                reflectedNameRead)
                == noc::PropertyAccessStatus::Success
                && std::strcmp(
                    static_cast<const noc::ReflectionString*>(
                        reflectedNameRead.Data())->CStr(),
                    "Reflected Entity") == 0,
            "Semantic Name property read failed");

        reflectedNameRead.Clear();
        reflectedName.Clear();

        ok &= CheckReflectionRegistry(
            nameType->componentMetadata->remove(
                componentWorld,
                componentEntity)
                && !nameType->componentMetadata->has(
                    componentWorld,
                    componentEntity),
            "Generic reflected Name remove failed");

        componentWorld.Shutdown();
        componentRegistry.Shutdown();

        ok &= CheckReflectionRegistry(
            allocator.OutstandingBytes() == 0,
            "Component reflection test leaked allocator memory");
    }

    {
        noc::ReflectionRegistry functionRegistry;
        ok &= CheckReflectionRegistry(
            functionRegistry.Init(allocator, 2),
            "Function registry init failed");

        constexpr noc::TypeId kFunctionInt{ 0x3300000000000001ull };
        constexpr noc::TypeId kFunctionOwner{ 0x3300000000000002ull };

        const noc::TypeMetadata functionInt =
            noc::MakeTypeMetadata<int>(
                kFunctionInt,
                "Nocturne.Tests.FunctionInt",
                noc::TypeKind::SignedInteger,
                1);

        char mutableParameterName[] = "a";
        const noc::FunctionParameterMetadata addParameters[] = {
            { mutableParameterName, kFunctionInt },
            { "b", kFunctionInt }
        };
        const noc::FunctionParameterMetadata setParameters[] = {
            { "value", kFunctionInt }
        };

        const noc::FunctionMetadata functions[] = {
            {
                noc::MakeFunctionId(
                    "Nocturne.Tests.FunctionOwner.AddWithBase"),
                "AddWithBase",
                kFunctionOwner,
                kFunctionInt,
                noc::FunctionFlags::Member
                    | noc::FunctionFlags::Const,
                addParameters,
                2,
                &InvokeAddWithBase
            },
            {
                noc::MakeFunctionId(
                    "Nocturne.Tests.FunctionOwner.SetBase"),
                "SetBase",
                kFunctionOwner,
                noc::TypeId::Invalid(),
                noc::FunctionFlags::Member,
                setParameters,
                1,
                &InvokeSetBase
            }
        };

        noc::TypeMetadata ownerType =
            noc::MakeTypeMetadata<FunctionOwner>(
                kFunctionOwner,
                "Nocturne.Tests.FunctionOwner",
                noc::TypeKind::Struct,
                1);
        ownerType.functions = functions;
        ownerType.functionCount = 2;

        ok &= CheckReflectionRegistry(
            functionRegistry.RegisterType(ownerType)
                && functionRegistry.RegisterType(functionInt)
                && functionRegistry.Freeze(),
            "Function reflection schema failed");

        mutableParameterName[0] = 'X';

        const noc::FunctionMetadata* addFunction =
            functionRegistry.FindFunctionByName(
                kFunctionOwner,
                "AddWithBase");
        const noc::FunctionMetadata* setFunction =
            functionRegistry.FindFunction(
                kFunctionOwner,
                noc::MakeFunctionId(
                    "Nocturne.Tests.FunctionOwner.SetBase"));

        ok &= CheckReflectionRegistry(
            addFunction
                && std::strcmp(
                    addFunction->parameters[0].canonicalName,
                    "a") == 0
                && setFunction,
            "Function metadata ownership/lookup failed");

        FunctionOwner owner{};
        owner.base = 10;
        noc::FunctionInvocationContext functionContext{};
        functionContext.object = &owner;

        int a = 2;
        int b = 3;
        const noc::ReflectedConstValueView addArguments[] = {
            { kFunctionInt, &a },
            { kFunctionInt, &b }
        };

        noc::OwnedReflectedValue result;
        ok &= CheckReflectionRegistry(
            noc::InvokeReflectedFunction(
                functionRegistry,
                *addFunction,
                functionContext,
                addArguments,
                2,
                &allocator,
                &result)
                == noc::FunctionInvokeStatus::Success
                && *static_cast<const int*>(result.Data()) == 15,
            "Generic reflected function invocation failed");
        result.Clear();

        ok &= CheckReflectionRegistry(
            noc::InvokeReflectedFunction(
                functionRegistry,
                *addFunction,
                functionContext,
                addArguments,
                1,
                &allocator,
                &result)
                == noc::FunctionInvokeStatus::ArgumentCountMismatch,
            "Function invocation did not reject argument count mismatch");

        const noc::ReflectedConstValueView wrongArgument{
            kFunctionOwner,
            &a
        };
        ok &= CheckReflectionRegistry(
            noc::InvokeReflectedFunction(
                functionRegistry,
                *addFunction,
                functionContext,
                &wrongArgument,
                1,
                &allocator,
                &result)
                == noc::FunctionInvokeStatus::ArgumentCountMismatch,
            "Function count validation ordering changed unexpectedly");

        noc::FunctionInvocationContext mutatingContext{};
        int newBase = 21;
        const noc::ReflectedConstValueView setArgument{
            kFunctionInt,
            &newBase
        };

        ok &= CheckReflectionRegistry(
            noc::InvokeReflectedFunction(
                functionRegistry,
                *setFunction,
                mutatingContext,
                &setArgument,
                1,
                nullptr,
                nullptr)
                == noc::FunctionInvokeStatus::MissingObject,
            "Non-const member invocation did not require mutable object");

        mutatingContext.mutableObject = &owner;
        ok &= CheckReflectionRegistry(
            noc::InvokeReflectedFunction(
                functionRegistry,
                *setFunction,
                mutatingContext,
                &setArgument,
                1,
                nullptr,
                nullptr)
                == noc::FunctionInvokeStatus::Success
                && owner.base == 21,
            "Void reflected member invocation failed");

        functionRegistry.Shutdown();
        ok &= CheckReflectionRegistry(
            allocator.OutstandingBytes() == 0,
            "Function reflection leaked allocator memory");
    }

    {
        noc::ReflectionRegistry containerRegistry;
        ok &= CheckReflectionRegistry(
            containerRegistry.Init(allocator, 3),
            "Container registry init failed");

        constexpr noc::TypeId kContainerInt{ 0x3200000000000001ull };
        constexpr noc::TypeId kFixedType{ 0x3200000000000002ull };
        constexpr noc::TypeId kSequenceType{ 0x3200000000000003ull };

        const noc::TypeMetadata containerInt =
            noc::MakeTypeMetadata<int>(
                kContainerInt,
                "Nocturne.Tests.ContainerInt",
                noc::TypeKind::SignedInteger,
                1);

        const noc::ContainerMetadata fixedContainer{
            kContainerInt,
            3,
            false,
            &FixedCount,
            &FixedCapacity,
            &FixedConstElement,
            &FixedMutableElement,
            nullptr,
            nullptr,
            nullptr
        };

        noc::TypeMetadata fixedType =
            noc::MakeTypeMetadata<FixedInt3>(
                kFixedType,
                "Nocturne.Tests.FixedInt3",
                noc::TypeKind::FixedArray,
                1);
        fixedType.containerMetadata = &fixedContainer;

        const noc::ContainerMetadata sequenceContainer{
            kContainerInt,
            0,
            false,
            &SequenceCount,
            &SequenceCapacity,
            &SequenceConstElement,
            &SequenceMutableElement,
            &SequenceResize,
            &SequenceInsertDefault,
            &SequenceRemove
        };

        noc::TypeMetadata sequenceType =
            noc::MakeTypeMetadata<SmallSequence>(
                kSequenceType,
                "Nocturne.Tests.SmallSequence",
                noc::TypeKind::DynamicSequence,
                1);
        sequenceType.containerMetadata = &sequenceContainer;

        ok &= CheckReflectionRegistry(
            containerRegistry.RegisterType(sequenceType)
                && containerRegistry.RegisterType(containerInt)
                && containerRegistry.RegisterType(fixedType)
                && containerRegistry.Freeze(),
            "Container reflection schema failed");

        const noc::ContainerMetadata* reflectedFixed =
            containerRegistry.FindContainer(kFixedType);
        const noc::ContainerMetadata* reflectedSequence =
            containerRegistry.FindContainer(kSequenceType);

        FixedInt3 fixed{};
        fixed.values[1] = 9;
        ok &= CheckReflectionRegistry(
            reflectedFixed
                && reflectedFixed->fixedCount == 3
                && *static_cast<const int*>(
                    reflectedFixed->constElement(&fixed, 1)) == 9,
            "Fixed-array container reflection failed");

        SmallSequence sequence{};
        ok &= CheckReflectionRegistry(
            reflectedSequence
                && reflectedSequence->resize(&sequence, 2)
                && sequence.count == 2,
            "Dynamic sequence resize failed");

        *static_cast<int*>(
            reflectedSequence->mutableElement(&sequence, 0)) = 11;
        *static_cast<int*>(
            reflectedSequence->mutableElement(&sequence, 1)) = 22;

        ok &= CheckReflectionRegistry(
            reflectedSequence->insertDefault(&sequence, 1)
                && sequence.count == 3
                && sequence.values[0] == 11
                && sequence.values[1] == 0
                && sequence.values[2] == 22,
            "Dynamic sequence insert failed");

        ok &= CheckReflectionRegistry(
            reflectedSequence->remove(&sequence, 0)
                && sequence.count == 2
                && sequence.values[0] == 0
                && sequence.values[1] == 22,
            "Dynamic sequence remove failed");

        containerRegistry.Shutdown();
        ok &= CheckReflectionRegistry(
            allocator.OutstandingBytes() == 0,
            "Container reflection leaked allocator memory");
    }

    {
        noc::ReflectionRegistry accessRegistry;
        ok &= CheckReflectionRegistry(
            accessRegistry.Init(allocator, 2),
            "Property access registry init failed");

        constexpr noc::TypeId kAccessInt{ 0x3100000000000001ull };
        constexpr noc::TypeId kSemanticOwner{ 0x3100000000000002ull };

        const noc::TypeMetadata accessInt =
            noc::MakeTypeMetadata<int>(
                kAccessInt,
                "Nocturne.Tests.AccessInt",
                noc::TypeKind::SignedInteger,
                1);

        const noc::PropertyMetadata semanticProperties[] = {
            noc::PropertyMetadata{
                noc::MakePropertyId(
                    "Nocturne.Tests.SemanticOwner.value"),
                "value",
                kSemanticOwner,
                kAccessInt,
                noc::PropertyFlags::EditorVisible,
                &ReadSemanticInt,
                &WriteSemanticInt,
                nullptr,
                nullptr,
                &ValidateNonNegative,
                nullptr,
                nullptr,
                0
            },
            noc::PropertyMetadata{
                noc::MakePropertyId(
                    "Nocturne.Tests.SemanticOwner.readOnlyValue"),
                "readOnlyValue",
                kSemanticOwner,
                kAccessInt,
                noc::PropertyFlags::ReadOnly,
                &ReadSemanticInt,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                0
            }
        };

        noc::TypeMetadata semanticOwnerType =
            noc::MakeTypeMetadata<SemanticOwner>(
                kSemanticOwner,
                "Nocturne.Tests.SemanticOwner",
                noc::TypeKind::Struct,
                1);
        semanticOwnerType.properties = semanticProperties;
        semanticOwnerType.propertyCount = 2;

        ok &= CheckReflectionRegistry(
            accessRegistry.RegisterType(semanticOwnerType)
                && accessRegistry.RegisterType(accessInt)
                && accessRegistry.Freeze(),
            "Semantic property access schema failed");

        SemanticOwner semanticOwner{};
        semanticOwner.value = 5;
        noc::PropertyAccessContext semanticContext{};
        semanticContext.object = &semanticOwner;
        semanticContext.mutableObject = &semanticOwner;

        const noc::PropertyMetadata* semanticValue =
            accessRegistry.FindPropertyByName(
                kSemanticOwner,
                "value");
        const noc::PropertyMetadata* readOnlyValue =
            accessRegistry.FindPropertyByName(
                kSemanticOwner,
                "readOnlyValue");

        noc::OwnedReflectedValue reflectedRead;
        ok &= CheckReflectionRegistry(
            semanticValue
                && noc::ReadPropertyValue(
                    accessRegistry,
                    *semanticValue,
                    semanticContext,
                    allocator,
                    reflectedRead)
                    == noc::PropertyAccessStatus::Success
                && *static_cast<const int*>(
                    reflectedRead.Data()) == 5,
            "Generic semantic property read failed");

        int negative = -1;
        ok &= CheckReflectionRegistry(
            noc::WritePropertyValue(
                *semanticValue,
                semanticContext,
                noc::ReflectedConstValueView{
                    kAccessInt,
                    &negative })
                == noc::PropertyAccessStatus::ValidationFailed
                && semanticOwner.value == 5,
            "Semantic validation did not reject invalid value");

        int positive = 12;
        ok &= CheckReflectionRegistry(
            noc::WritePropertyValue(
                *semanticValue,
                semanticContext,
                noc::ReflectedConstValueView{
                    kAccessInt,
                    &positive })
                == noc::PropertyAccessStatus::Success
                && semanticOwner.value == 12,
            "Generic semantic property write failed");

        ok &= CheckReflectionRegistry(
            noc::WritePropertyValue(
                *semanticValue,
                semanticContext,
                noc::ReflectedConstValueView{
                    kSemanticOwner,
                    &positive })
                == noc::PropertyAccessStatus::TypeMismatch,
            "Property write did not reject mismatched TypeId");

        ok &= CheckReflectionRegistry(
            readOnlyValue
                && noc::WritePropertyValue(
                    *readOnlyValue,
                    semanticContext,
                    noc::ReflectedConstValueView{
                        kAccessInt,
                        &positive })
                    == noc::PropertyAccessStatus::ReadOnly,
            "Read-only property write was not rejected");

        reflectedRead.Clear();
        accessRegistry.Shutdown();
        ok &= CheckReflectionRegistry(
            allocator.OutstandingBytes() == 0,
            "Semantic property access leaked allocator memory");
    }

    {
        noc::ReflectionRegistry builtinRegistry;

        ok &= CheckReflectionRegistry(
            builtinRegistry.Init(allocator, 32)
                && noc::RegisterBuiltinReflectionTypes(
                    builtinRegistry)
                && builtinRegistry.Freeze(),
            "Real engine function reflection registry setup failed");

        const noc::FunctionMetadata* lengthFunction =
            builtinRegistry.FindFunction(
                noc::BuiltinTypeIds::Vec3,
                noc::BuiltinFunctionIds::Vec3Length);
        const noc::FunctionMetadata* dotFunction =
            builtinRegistry.FindFunctionByName(
                noc::BuiltinTypeIds::Vec3,
                "Dot");

        ok &= CheckReflectionRegistry(
            lengthFunction
                && dotFunction
                && noc::HasFlag(
                    lengthFunction->flags,
                    noc::FunctionFlags::Static)
                && noc::HasFlag(
                    dotFunction->flags,
                    noc::FunctionFlags::Static)
                && lengthFunction->returnTypeId
                    == noc::BuiltinTypeIds::Float32
                && lengthFunction->parameterCount == 1
                && dotFunction->parameterCount == 2,
            "Real engine Vec3 function metadata/lookup failed");

        const noc::Vec3 lengthInput{
            3.0f, 4.0f, 0.0f
        };
        const noc::ReflectedConstValueView lengthArgs[] = {
            {
                noc::BuiltinTypeIds::Vec3,
                &lengthInput
            }
        };
        noc::FunctionInvocationContext functionContext{};
        noc::OwnedReflectedValue lengthResult;

        ok &= CheckReflectionRegistry(
            lengthFunction
                && noc::InvokeReflectedFunction(
                    builtinRegistry,
                    *lengthFunction,
                    functionContext,
                    lengthArgs,
                    1,
                    &allocator,
                    &lengthResult)
                    == noc::FunctionInvokeStatus::Success
                && lengthResult.Type()
                    == noc::BuiltinTypeIds::Float32
                && lengthResult.Data()
                && std::fabs(
                    *static_cast<const float*>(
                        lengthResult.Data())
                        - 5.0f) < 1.0e-6f,
            "Real engine Vec3.Length reflected invocation failed");

        const noc::Vec3 dotA{
            1.0f, 2.0f, 3.0f
        };
        const noc::Vec3 dotB{
            4.0f, 5.0f, 6.0f
        };
        const noc::ReflectedConstValueView dotArgs[] = {
            {
                noc::BuiltinTypeIds::Vec3,
                &dotA
            },
            {
                noc::BuiltinTypeIds::Vec3,
                &dotB
            }
        };
        noc::OwnedReflectedValue dotResult;

        ok &= CheckReflectionRegistry(
            dotFunction
                && noc::InvokeReflectedFunction(
                    builtinRegistry,
                    *dotFunction,
                    functionContext,
                    dotArgs,
                    2,
                    &allocator,
                    &dotResult)
                    == noc::FunctionInvokeStatus::Success
                && dotResult.Type()
                    == noc::BuiltinTypeIds::Float32
                && dotResult.Data()
                && std::fabs(
                    *static_cast<const float*>(
                        dotResult.Data())
                        - 32.0f) < 1.0e-6f,
            "Real engine Vec3.Dot reflected invocation failed");

        ok &= CheckReflectionRegistry(
            lengthFunction
                && noc::InvokeReflectedFunction(
                    builtinRegistry,
                    *lengthFunction,
                    functionContext,
                    nullptr,
                    0,
                    &allocator,
                    nullptr)
                    == noc::FunctionInvokeStatus::ArgumentCountMismatch,
            "Real engine function invocation did not reject argument-count mismatch");

        const float wrongArgument = 1.0f;
        const noc::ReflectedConstValueView wrongArgs[] = {
            {
                noc::BuiltinTypeIds::Float32,
                &wrongArgument
            }
        };

        ok &= CheckReflectionRegistry(
            lengthFunction
                && noc::InvokeReflectedFunction(
                    builtinRegistry,
                    *lengthFunction,
                    functionContext,
                    wrongArgs,
                    1,
                    &allocator,
                    nullptr)
                    == noc::FunctionInvokeStatus::ArgumentTypeMismatch,
            "Real engine function invocation did not reject argument TypeId mismatch");

        lengthResult.Clear();
        dotResult.Clear();
        builtinRegistry.Shutdown();

        ok &= CheckReflectionRegistry(
            allocator.OutstandingBytes() == 0,
            "Real engine function reflection proof leaked allocator memory");
    }

    {
        OwnedTestValue::liveCount = 0;

        const noc::TypeMetadata ownedMetadata =
            noc::MakeTypeMetadata<OwnedTestValue>(
                noc::TypeId{ 0x3000000000000001ull },
                "Nocturne.Tests.OwnedTestValue",
                noc::TypeKind::Struct,
                1);

        OwnedTestValue source{};
        source.value = 77;

        noc::OwnedReflectedValue owned;
        ok &= CheckReflectionRegistry(
            owned.InitCopy(allocator, ownedMetadata, &source),
            "Owned reflected copy initialization failed");
        ok &= CheckReflectionRegistry(
            owned.IsValid()
                && owned.Type() == ownedMetadata.typeId
                && static_cast<const OwnedTestValue*>(
                    owned.Data())->value == 77
                && OwnedTestValue::liveCount == 2,
            "Owned reflected value state/lifetime mismatch");

        OwnedTestValue replacement{};
        replacement.value = 91;
        ok &= CheckReflectionRegistry(
            owned.CopyAssign(&replacement)
                && static_cast<const OwnedTestValue*>(
                    owned.Data())->value == 91,
            "Owned reflected copy assignment failed");

        noc::OwnedReflectedValue moved = std::move(owned);
        ok &= CheckReflectionRegistry(
            !owned.IsValid()
                && moved.IsValid()
                && moved.Type() == ownedMetadata.typeId,
            "Owned reflected move transfer failed");

        ok &= CheckReflectionRegistry(
            moved.ResetToDefault()
                && static_cast<const OwnedTestValue*>(
                    moved.Data())->value == 0,
            "Owned reflected reset failed");

        OwnedTestValue defaultValue{};
        ok &= CheckReflectionRegistry(
            moved.Equals(
                noc::ReflectedConstValueView{
                    ownedMetadata.typeId,
                    &defaultValue }),
            "Owned reflected equality failed");

        moved.Clear();
        ok &= CheckReflectionRegistry(
            OwnedTestValue::liveCount == 3,
            "Owned reflected Clear did not destroy its object");

        // source, replacement and defaultValue remain alive until scope exit.
    }

    ok &= CheckReflectionRegistry(
        OwnedTestValue::liveCount == 0,
        "Owned reflected value leaked non-trivial objects");

    {
        const noc::TypeMetadata alignedMetadata =
            noc::MakeTypeMetadata<OwnedOverAligned>(
                noc::TypeId{ 0x3000000000000002ull },
                "Nocturne.Tests.OwnedOverAligned",
                noc::TypeKind::Struct,
                1);

        noc::OwnedReflectedValue aligned;
        ok &= CheckReflectionRegistry(
            aligned.InitDefault(allocator, alignedMetadata),
            "Over-aligned reflected value init failed");
        ok &= CheckReflectionRegistry(
            reinterpret_cast<uintptr_t>(aligned.Data())
                % alignof(OwnedOverAligned) == 0,
            "Owned reflected value lost alignment");
        aligned.Clear();
    }

    {
        const noc::TypeMetadata noDefaultMetadata =
            noc::MakeTypeMetadata<NoDefaultValue>(
                noc::TypeId{ 0x3000000000000003ull },
                "Nocturne.Tests.NoDefault",
                noc::TypeKind::Struct,
                1);

        const std::size_t allocationCount =
            allocator.AllocationCount();

        noc::OwnedReflectedValue unavailable;
        ok &= CheckReflectionRegistry(
            !unavailable.InitDefault(
                allocator,
                noDefaultMetadata),
            "Missing default constructor must fail cleanly");
        ok &= CheckReflectionRegistry(
            allocator.AllocationCount() == allocationCount,
            "Failed owned-value init allocated memory");
    }

    ok &= CheckReflectionRegistry(
        allocator.OutstandingBytes() == 0,
        "Reflected value tests leaked allocator memory");

    NOC_LOG_INFO(
        "Phase16",
        "Reflection registry tests %s",
        ok ? "PASS" : "FAIL");
    return ok;
}
