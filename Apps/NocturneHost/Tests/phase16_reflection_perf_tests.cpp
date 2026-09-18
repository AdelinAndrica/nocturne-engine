#include "Runtime/Reflection/BuiltinTypes.h"
#include "Runtime/Reflection/ComponentReflection.h"
#include "Runtime/Reflection/FoundationComponents.h"
#include "Runtime/Reflection/FunctionInvocation.h"
#include "Runtime/Reflection/ReflectionRegistry.h"
#include "Runtime/World.h"

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
            registry.TypeCount() == typeCount + 21u,
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

        constexpr uint32_t kNameLookups = 10000;
        uint32_t nameHits = 0;

        const auto nameLookupBegin = Clock::now();

        for (uint32_t i = 0; i < kNameLookups; ++i)
        {
            char typeName[96]{};
            std::snprintf(
                typeName,
                sizeof(typeName),
                "Nocturne.Perf.Type.%u",
                i % typeCount);

            if (registry.FindTypeByName(typeName))
                ++nameHits;
        }

        const auto nameLookupEnd = Clock::now();

        NOC_LOG_INFO(
            "Phase16Perf",
            "reflection_name_lookup: synthetic_types=%u lookups=%u time_us=%lld",
            typeCount,
            kNameLookups,
            Micros(nameLookupBegin, nameLookupEnd));

        ok &= CheckPerf(
            nameHits == kNameLookups,
            "Reflection canonical-name lookup workload failed");

        constexpr uint32_t kPropertyEnumerations = 100000;
        uint32_t propertyEnumerationHits = 0;

        const auto propertyEnumerationBegin =
            Clock::now();

        for (uint32_t i = 0;
             i < kPropertyEnumerations;
             ++i)
        {
            const noc::TypeMetadata* type =
                registry.FindType(
                    PerfTypeId(i % typeCount));

            if (!type)
                continue;

            for (uint32_t p = 0;
                 p < type->propertyCount;
                 ++p)
            {
                if (type->properties[p].propertyId.IsValid())
                    ++propertyEnumerationHits;
            }
        }

        const auto propertyEnumerationEnd =
            Clock::now();

        NOC_LOG_INFO(
            "Phase16Perf",
            "reflection_property_enumeration: synthetic_types=%u enumerations=%u time_us=%lld",
            typeCount,
            kPropertyEnumerations,
            Micros(
                propertyEnumerationBegin,
                propertyEnumerationEnd));

        ok &= CheckPerf(
            propertyEnumerationHits
                == kPropertyEnumerations,
            "Reflection property enumeration workload failed");

        const noc::FunctionMetadata* vec3Length =
            registry.FindFunction(
                noc::BuiltinTypeIds::Vec3,
                noc::BuiltinFunctionIds::Vec3Length);

        constexpr uint32_t kFunctionLookups = 100000;
        uint32_t functionLookupHits = 0;

        const auto functionLookupBegin =
            Clock::now();

        for (uint32_t i = 0;
             i < kFunctionLookups;
             ++i)
        {
            if (registry.FindFunction(
                    noc::BuiltinTypeIds::Vec3,
                    noc::BuiltinFunctionIds::Vec3Length))
            {
                ++functionLookupHits;
            }
        }

        const auto functionLookupEnd =
            Clock::now();

        NOC_LOG_INFO(
            "Phase16Perf",
            "reflection_function_lookup: lookups=%u time_us=%lld",
            kFunctionLookups,
            Micros(
                functionLookupBegin,
                functionLookupEnd));

        ok &= CheckPerf(
            vec3Length
                && functionLookupHits
                    == kFunctionLookups,
            "Reflection function lookup workload failed");

        const noc::Vec3 functionInput{
            3.0f, 4.0f, 0.0f
        };
        const noc::ReflectedConstValueView functionArgs[] = {
            {
                noc::BuiltinTypeIds::Vec3,
                &functionInput
            }
        };
        noc::FunctionInvocationContext functionContext{};
        float rawFunctionResult = 0.0f;

        constexpr uint32_t kRawFunctionInvokes = 100000;
        uint32_t rawFunctionHits = 0;

        const auto rawFunctionBegin =
            Clock::now();

        for (uint32_t i = 0;
             i < kRawFunctionInvokes;
             ++i)
        {
            if (vec3Length
                && vec3Length->invoke(
                    functionContext,
                    functionArgs,
                    1,
                    noc::ReflectedValueView{
                        noc::BuiltinTypeIds::Float32,
                        &rawFunctionResult })
                && rawFunctionResult == 5.0f)
            {
                ++rawFunctionHits;
            }
        }

        const auto rawFunctionEnd =
            Clock::now();

        NOC_LOG_INFO(
            "Phase16Perf",
            "reflection_function_raw_invoke: invokes=%u time_us=%lld",
            kRawFunctionInvokes,
            Micros(
                rawFunctionBegin,
                rawFunctionEnd));

        ok &= CheckPerf(
            rawFunctionHits == kRawFunctionInvokes,
            "Reflection raw function invoke workload failed");

        ok &= CheckPerf(
            allocator.AllocationCount() == allocationsBeforeLookup,
            "Frozen reflection hot lookup/enumeration/raw-invoke performed allocator calls");
        ok &= CheckPerf(
            allocator.TotalAllocatedBytes() == bytesBeforeLookup,
            "Frozen reflection hot lookup/enumeration/raw-invoke allocated persistent bytes");

        constexpr uint32_t kGenericFunctionInvokes = 10000;
        uint32_t genericFunctionHits = 0;
        const std::size_t genericAllocationsBefore =
            allocator.AllocationCount();

        const auto genericFunctionBegin =
            Clock::now();

        for (uint32_t i = 0;
             i < kGenericFunctionInvokes;
             ++i)
        {
            noc::OwnedReflectedValue result;

            if (vec3Length
                && noc::InvokeReflectedFunction(
                    registry,
                    *vec3Length,
                    functionContext,
                    functionArgs,
                    1,
                    &allocator,
                    &result)
                    == noc::FunctionInvokeStatus::Success
                && result.Data()
                && *static_cast<const float*>(
                    result.Data()) == 5.0f)
            {
                ++genericFunctionHits;
            }
        }

        const auto genericFunctionEnd =
            Clock::now();

        NOC_LOG_INFO(
            "Phase16Perf",
            "reflection_function_generic_invoke: invokes=%u time_us=%lld allocator_calls=%zu",
            kGenericFunctionInvokes,
            Micros(
                genericFunctionBegin,
                genericFunctionEnd),
            allocator.AllocationCount()
                - genericAllocationsBefore);

        ok &= CheckPerf(
            genericFunctionHits
                == kGenericFunctionInvokes,
            "Generic reflected function invocation workload failed");

        registry.Shutdown();

        ok &= CheckPerf(
            allocator.OutstandingBytes() == 0,
            "Reflection performance workload leaked allocator memory");

        return ok;
    }

    bool RunComponentEnumerationBaseline()
    {
        noc::MallocAllocator backing;
        noc::DebugAlloc allocator(backing);
        noc::ReflectionRegistry registry;
        noc::World world;

        bool ok = true;

        ok &= CheckPerf(
            registry.Init(allocator, 32)
                && noc::RegisterBuiltinReflectionTypes(
                    registry)
                && noc::RegisterFoundationComponentReflectionTypes(
                    registry)
                && registry.Freeze(),
            "Component enumeration perf reflection setup failed");

        ok &= CheckPerf(
            world.Init(allocator, registry),
            "Component enumeration perf World init failed");

        const noc::EntityHandle entity =
            world.CreateEntity();
        const noc::AABB bounds{
            noc::Vec3{
                -1.0f, -1.0f, -1.0f },
            noc::Vec3{
                1.0f, 1.0f, 1.0f }
        };

        ok &= CheckPerf(
            entity.IsValid()
                && world.AddName(
                    entity,
                    "Reflection Perf Entity")
                && world.AddTransform(entity)
                && world.AddRenderable(
                    entity,
                    noc::ResourceHandle{},
                    bounds)
                && world.AddCamera(entity),
            "Component enumeration perf entity setup failed");

        constexpr uint32_t kEnumerations = 100000;
        const std::size_t allocationsBefore =
            allocator.AllocationCount();
        const std::size_t bytesBefore =
            allocator.TotalAllocatedBytes();

        uint32_t componentHits = 0;
        const auto begin = Clock::now();

        for (uint32_t i = 0;
             i < kEnumerations;
             ++i)
        {
            const uint32_t count =
                noc::ReflectedComponentCountForEntity(
                    registry,
                    world,
                    entity);

            if (count != 4)
                continue;

            bool allValid = true;
            for (uint32_t c = 0;
                 c < count;
                 ++c)
            {
                const noc::TypeMetadata* component =
                    noc::ReflectedComponentAtForEntity(
                        registry,
                        world,
                        entity,
                        c);

                if (!component
                    || component->kind
                        != noc::TypeKind::Component)
                {
                    allValid = false;
                    break;
                }
            }

            if (allValid)
                ++componentHits;
        }

        const auto end = Clock::now();

        NOC_LOG_INFO(
            "Phase16Perf",
            "reflection_component_enumeration: enumerations=%u components_per_entity=4 time_us=%lld",
            kEnumerations,
            Micros(begin, end));

        ok &= CheckPerf(
            componentHits == kEnumerations,
            "Reflected component enumeration workload failed");
        ok &= CheckPerf(
            allocator.AllocationCount()
                == allocationsBefore,
            "Frozen reflected component enumeration performed allocator calls");
        ok &= CheckPerf(
            allocator.TotalAllocatedBytes()
                == bytesBefore,
            "Frozen reflected component enumeration allocated persistent bytes");

        world.Shutdown();
        registry.Shutdown();

        ok &= CheckPerf(
            allocator.OutstandingBytes() == 0,
            "Component enumeration performance workload leaked allocator memory");

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
    ok &= RunComponentEnumerationBaseline();

    NOC_LOG_INFO(
        "Phase16Perf",
        "Reflection stress/performance baseline %s",
        ok ? "PASS" : "FAIL");

    return ok;
}
