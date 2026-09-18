#include "Runtime/Reflection/BuiltinTypes.h"
#include "Runtime/Reflection/ReflectionRegistry.h"

#include "Core/Log.h"
#include "Core/Memory/Allocator.h"
#include "Core/Memory/DebugAlloc.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace
{
    using Clock = std::chrono::steady_clock;

    struct PerfValue
    {
        float value = 0.0f;

        [[nodiscard]] bool operator==(
            const PerfValue&) const = default;
    };

    bool CheckPerf(bool condition, const char* message)
    {
        if (!condition)
        {
            NOC_LOG_ERROR("Phase16Perf", "%s", message);
            return false;
        }
        return true;
    }

    long long Micros(
        Clock::time_point begin,
        Clock::time_point end)
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            end - begin).count();
    }

    [[nodiscard]] constexpr noc::TypeId PerfTypeId(
        uint32_t index)
    {
        return noc::TypeId{
            0x4000000000000000ull
                + static_cast<uint64_t>(index)
                + 1ull
        };
    }

    [[nodiscard]] constexpr noc::PropertyId PerfPropertyId(
        uint32_t index)
    {
        return noc::PropertyId{
            0x5000000000000000ull
                + static_cast<uint64_t>(index)
                + 1ull
        };
    }

    bool RunTypeScale(uint32_t typeCount)
    {
        noc::MallocAllocator backing;
        noc::DebugAlloc allocator(backing);
        noc::ReflectionRegistry registry;

        bool ok = true;

        ok &= CheckPerf(
            registry.Init(allocator, typeCount + 32u),
            "Reflection perf registry Init failed");
        ok &= CheckPerf(
            noc::RegisterBuiltinReflectionTypes(registry),
            "Reflection perf builtin registration failed");

        const std::size_t allocationsBeforeRegistration =
            allocator.AllocationCount();
        const std::size_t bytesBeforeRegistration =
            allocator.TotalAllocatedBytes();

        const auto registrationBegin = Clock::now();

        for (uint32_t i = 0; i < typeCount; ++i)
        {
            char typeName[96]{};
            std::snprintf(
                typeName,
                sizeof(typeName),
                "Nocturne.Perf.Type.%u",
                i);

            const noc::PropertyMetadata property =
                noc::MakeMemberPropertyMetadata<
                    PerfValue,
                    float,
                    &PerfValue::value>(
                        PerfPropertyId(i),
                        "value",
                        PerfTypeId(i),
                        noc::BuiltinTypeIds::Float32,
                        noc::PropertyFlags::EditorVisible
                            | noc::PropertyFlags::Serializable);

            noc::TypeMetadata metadata =
                noc::MakeTypeMetadata<PerfValue>(
                    PerfTypeId(i),
                    typeName,
                    noc::TypeKind::Struct,
                    1,
                    noc::TypeFlags::EditorVisible
                        | noc::TypeFlags::Serializable);

            metadata.properties = &property;
            metadata.propertyCount = 1;

            if (!registry.RegisterType(metadata))
            {
                ok &= CheckPerf(
                    false,
                    "Reflection perf type registration failed");
                break;
            }
        }

        const bool freezeOk = registry.Freeze();
        const auto registrationEnd = Clock::now();

        ok &= CheckPerf(
            freezeOk,
            "Reflection perf Freeze failed");

        NOC_LOG_INFO(
            "Phase16Perf",
            "reflection_register_freeze: synthetic_types=%u time_us=%lld "
            "allocator_calls=%zu allocated_bytes=%zu",
            typeCount,
            Micros(registrationBegin, registrationEnd),
            allocator.AllocationCount() - allocationsBeforeRegistration,
            allocator.TotalAllocatedBytes() - bytesBeforeRegistration);

        ok &= CheckPerf(
            registry.TypeCount() == typeCount + 20u,
            "Reflection perf frozen type count mismatch");

        if (!freezeOk)
        {
            registry.Shutdown();
            return false;
        }

        constexpr uint32_t kPropertyLookups = 10000;
        const std::size_t allocationsBeforeLookup =
            allocator.AllocationCount();
        const std::size_t bytesBeforeLookup =
            allocator.TotalAllocatedBytes();

        const auto propertyLookupBegin = Clock::now();

        uint32_t propertyHits = 0;
        for (uint32_t i = 0; i < kPropertyLookups; ++i)
        {
            const uint32_t typeIndex = i % typeCount;

            if (registry.FindProperty(
                    PerfTypeId(typeIndex),
                    PerfPropertyId(typeIndex)))
            {
                ++propertyHits;
            }
        }

        const auto propertyLookupEnd = Clock::now();

        NOC_LOG_INFO(
            "Phase16Perf",
            "reflection_property_lookup: synthetic_types=%u lookups=%u "
            "time_us=%lld",
            typeCount,
            kPropertyLookups,
            Micros(propertyLookupBegin, propertyLookupEnd));

        ok &= CheckPerf(
            propertyHits == kPropertyLookups,
            "Reflection property lookup missed metadata");

        constexpr uint32_t kReadCount = 100000;

        const noc::PropertyMetadata* readProperty =
            registry.FindProperty(
                PerfTypeId(0),
                PerfPropertyId(0));

        PerfValue sample{};
        sample.value = 42.0f;

        noc::PropertyAccessContext context{};
        context.object = &sample;

        float readValue = 0.0f;
        uint32_t readHits = 0;

        const auto readBegin = Clock::now();

        for (uint32_t i = 0; i < kReadCount; ++i)
        {
            if (readProperty
                && readProperty->read(context, &readValue)
                && readValue == 42.0f)
            {
                ++readHits;
            }
        }

        const auto readEnd = Clock::now();

        NOC_LOG_INFO(
            "Phase16Perf",
            "reflection_property_read: synthetic_types=%u reads=%u "
            "time_us=%lld",
            typeCount,
            kReadCount,
            Micros(readBegin, readEnd));

        ok &= CheckPerf(
            readHits == kReadCount,
            "Reflection property read workload failed");

        constexpr uint32_t kTypeLookups = 100000;
        uint32_t typeHits = 0;

        const auto typeLookupBegin = Clock::now();

        for (uint32_t i = 0; i < kTypeLookups; ++i)
        {
            if (registry.FindType(
                    PerfTypeId(i % typeCount)))
            {
                ++typeHits;
            }
        }

        const auto typeLookupEnd = Clock::now();

        NOC_LOG_INFO(
            "Phase16Perf",
            "reflection_type_lookup: synthetic_types=%u lookups=%u "
            "time_us=%lld",
            typeCount,
            kTypeLookups,
            Micros(typeLookupBegin, typeLookupEnd));

        ok &= CheckPerf(
            typeHits == kTypeLookups,
            "Reflection TypeId lookup workload failed");

        ok &= CheckPerf(
            allocator.AllocationCount() == allocationsBeforeLookup,
            "Frozen reflection lookup/read performed allocator calls");
        ok &= CheckPerf(
            allocator.TotalAllocatedBytes() == bytesBeforeLookup,
            "Frozen reflection lookup/read allocated persistent bytes");

        registry.Shutdown();

        ok &= CheckPerf(
            allocator.OutstandingBytes() == 0,
            "Reflection performance workload leaked allocator memory");

        return ok;
    }
}

bool RunPhase16ReflectionPerfTests()
{
    NOC_LOG_INFO(
        "Phase16Perf",
        "%s",
        "Reflection stress/performance baseline begin "
        "(timings are observations, not pass/fail budgets)");

    bool ok = true;
    ok &= RunTypeScale(100);
    ok &= RunTypeScale(1000);

    NOC_LOG_INFO(
        "Phase16Perf",
        "Reflection stress/performance baseline %s",
        ok ? "PASS" : "FAIL");

    return ok;
}
