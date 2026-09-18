#include "Runtime/Reflection/ReflectionIds.h"
#include "Runtime/Reflection/ReflectionMetadata.h"

#include "Core/Log.h"

#include <cstdint>
#include <new>

namespace
{
    bool CheckReflectionFoundation(bool condition, const char* message)
    {
        if (!condition)
        {
            NOC_LOG_ERROR("Phase16", "%s", message);
            return false;
        }
        return true;
    }

    struct NonTrivialValue
    {
        static inline int liveCount = 0;

        int value = 7;

        NonTrivialValue() { ++liveCount; }
        NonTrivialValue(const NonTrivialValue& other)
            : value(other.value)
        {
            ++liveCount;
        }

        NonTrivialValue(NonTrivialValue&& other) noexcept
            : value(other.value)
        {
            other.value = -1;
            ++liveCount;
        }

        NonTrivialValue& operator=(const NonTrivialValue& other)
        {
            value = other.value;
            return *this;
        }

        NonTrivialValue& operator=(NonTrivialValue&& other) noexcept
        {
            value = other.value;
            other.value = -1;
            return *this;
        }

        ~NonTrivialValue()
        {
            --liveCount;
        }

        [[nodiscard]] bool operator==(const NonTrivialValue& other) const
        {
            return value == other.value;
        }
    };

    struct NoDefaultValue
    {
        explicit NoDefaultValue(int inValue)
            : value(inValue)
        {
        }

        int value = 0;
    };
}

bool RunPhase16ReflectionFoundationTests()
{
    NOC_LOG_INFO("Phase16", "%s", "Reflection foundation tests begin");

    bool ok = true;

    constexpr noc::TypeId invalidType{};
    constexpr noc::TypeId explicitType{ 42 };
    constexpr noc::PropertyId explicitProperty{ 9 };
    constexpr noc::FunctionId explicitFunction{ 11 };

    ok &= CheckReflectionFoundation(
        !invalidType.IsValid(),
        "TypeId zero must be invalid");
    ok &= CheckReflectionFoundation(
        explicitType.IsValid(),
        "Non-zero TypeId must be valid");
    ok &= CheckReflectionFoundation(
        explicitProperty.IsValid() && explicitFunction.IsValid(),
        "PropertyId/FunctionId validity failed");

    constexpr noc::TypeId hashedType =
        noc::MakeTypeId("Nocturne.Tests.ReflectedType");
    constexpr noc::TypeId hashedTypeAgain =
        noc::MakeTypeId("Nocturne.Tests.ReflectedType");
    constexpr noc::PropertyId hashedProperty =
        noc::MakePropertyId("Nocturne.Tests.ReflectedType.value");
    constexpr noc::FunctionId hashedFunction =
        noc::MakeFunctionId("Nocturne.Tests.ReflectedType.Reset()");

    ok &= CheckReflectionFoundation(
        hashedType.IsValid() && hashedType == hashedTypeAgain,
        "Stable reflected type hash is not deterministic");
    ok &= CheckReflectionFoundation(
        hashedProperty.IsValid() && hashedFunction.IsValid(),
        "Stable reflected property/function hash failed");
    ok &= CheckReflectionFoundation(
        !noc::MakeTypeId(nullptr).IsValid()
            && !noc::MakeTypeId("").IsValid(),
        "Null/empty reflected names must map to invalid IDs");

    constexpr auto metadata =
        noc::MakeTypeMetadata<NonTrivialValue>(
            noc::TypeId{ 0x1000 },
            "Nocturne.Tests.NonTrivialValue",
            noc::TypeKind::Struct,
            1,
            noc::TypeFlags::EditorVisible
                | noc::TypeFlags::Serializable);

    ok &= CheckReflectionFoundation(
        metadata.typeId == noc::TypeId{ 0x1000 },
        "Type metadata ID mismatch");
    ok &= CheckReflectionFoundation(
        metadata.kind == noc::TypeKind::Struct
            && metadata.version == 1,
        "Type metadata kind/version mismatch");
    ok &= CheckReflectionFoundation(
        metadata.size == sizeof(NonTrivialValue)
            && metadata.alignment == alignof(NonTrivialValue),
        "Type metadata size/alignment mismatch");
    ok &= CheckReflectionFoundation(
        noc::HasFlag(metadata.flags, noc::TypeFlags::EditorVisible)
            && noc::HasFlag(metadata.flags, noc::TypeFlags::Serializable),
        "Type metadata flags mismatch");

    const noc::TypeLifecycleOperations operations =
        noc::MakeTypeLifecycleOperations<NonTrivialValue>();

    ok &= CheckReflectionFoundation(
        operations.defaultConstruct
            && operations.destruct
            && operations.copyConstruct
            && operations.moveConstruct
            && operations.copyAssign
            && operations.moveAssign
            && operations.equals
            && operations.reset,
        "Non-trivial lifecycle operations are incomplete");

    alignas(NonTrivialValue)
        unsigned char storageA[sizeof(NonTrivialValue)]{};
    alignas(NonTrivialValue)
        unsigned char storageB[sizeof(NonTrivialValue)]{};
    alignas(NonTrivialValue)
        unsigned char storageC[sizeof(NonTrivialValue)]{};
    alignas(NonTrivialValue)
        unsigned char storageD[sizeof(NonTrivialValue)]{};

    NonTrivialValue::liveCount = 0;

    operations.defaultConstruct(storageA);
    operations.defaultConstruct(storageB);

    auto* a = reinterpret_cast<NonTrivialValue*>(storageA);
    auto* b = reinterpret_cast<NonTrivialValue*>(storageB);
    a->value = 31;

    operations.copyAssign(storageB, storageA);
    ok &= CheckReflectionFoundation(
        operations.equals(storageA, storageB),
        "Lifecycle copy-assign/equality failed");

    operations.copyConstruct(storageC, storageA);
    auto* c = reinterpret_cast<NonTrivialValue*>(storageC);
    ok &= CheckReflectionFoundation(
        c->value == 31,
        "Lifecycle copy construction failed");

    operations.moveConstruct(storageD, storageC);
    auto* d = reinterpret_cast<NonTrivialValue*>(storageD);
    ok &= CheckReflectionFoundation(
        d->value == 31,
        "Lifecycle move construction failed");

    operations.reset(storageB);
    ok &= CheckReflectionFoundation(
        b->value == 7,
        "Lifecycle reset failed");

    operations.destruct(storageD);
    operations.destruct(storageC);
    operations.destruct(storageB);
    operations.destruct(storageA);

    ok &= CheckReflectionFoundation(
        NonTrivialValue::liveCount == 0,
        "Lifecycle operations leaked a live reflected object");

    const noc::TypeLifecycleOperations noDefault =
        noc::MakeTypeLifecycleOperations<NoDefaultValue>();
    ok &= CheckReflectionFoundation(
        noDefault.defaultConstruct == nullptr
            && noDefault.reset == nullptr
            && noDefault.destruct != nullptr,
        "Unsupported lifecycle operations must remain null");

    NOC_LOG_INFO(
        "Phase16",
        "Reflection foundation tests %s",
        ok ? "PASS" : "FAIL");
    return ok;
}
