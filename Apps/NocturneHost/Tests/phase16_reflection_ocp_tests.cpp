#include "Runtime/Reflection/BuiltinTypes.h"
#include "Runtime/Reflection/ComponentReflection.h"
#include "Runtime/Reflection/PropertyAccess.h"
#include "Runtime/Reflection/ReflectionRegistry.h"
#include "Runtime/World.h"

#include "Core/Log.h"
#include "Core/Memory/Allocator.h"
#include "Core/Memory/DebugAlloc.h"

namespace
{
    struct ReflectionTestComponent
    {
        float speed = 1.0f;
        bool enabled = true;

        [[nodiscard]] bool operator==(
            const ReflectionTestComponent&) const = default;
    };

    constexpr noc::TypeId kReflectionTestTypeId{ 0x1005ull };

    struct TestComponentStore
    {
        noc::EntityHandle entity{};
        ReflectionTestComponent component{};
        bool has = false;
    };

    TestComponentStore gStore;

    bool CheckOcp(bool condition, const char* message)
    {
        if (!condition)
        {
            NOC_LOG_ERROR("Phase16OCP", "%s", message);
            return false;
        }
        return true;
    }

    bool HasTestComponent(
        const noc::World& world,
        noc::EntityHandle entity)
    {
        return world.IsAlive(entity)
            && gStore.has
            && gStore.entity == entity;
    }

    bool AddTestComponent(
        noc::World& world,
        noc::EntityHandle entity)
    {
        if (!world.IsAlive(entity)
            || (gStore.has && gStore.entity == entity))
        {
            return false;
        }

        gStore.entity = entity;
        gStore.component = {};
        gStore.has = true;
        return true;
    }

    bool RemoveTestComponent(
        noc::World& world,
        noc::EntityHandle entity)
    {
        if (!world.IsAlive(entity)
            || !gStore.has
            || gStore.entity != entity)
        {
            return false;
        }

        gStore = {};
        return true;
    }

    const void* GetTestComponent(
        const noc::World& world,
        noc::EntityHandle entity)
    {
        return HasTestComponent(world, entity)
            ? &gStore.component
            : nullptr;
    }

    void* GetMutableTestComponent(
        noc::World& world,
        noc::EntityHandle entity)
    {
        return HasTestComponent(world, entity)
            ? &gStore.component
            : nullptr;
    }

    bool RegisterReflectionTestComponent(
        noc::ReflectionRegistry& registry)
    {
        constexpr noc::PropertyFlags kAuthorable =
            noc::PropertyFlags::EditorVisible
            | noc::PropertyFlags::Serializable
            | noc::PropertyFlags::ScriptVisible;

        const noc::PropertyMetadata properties[] = {
            noc::MakeMemberPropertyMetadata<
                ReflectionTestComponent,
                float,
                &ReflectionTestComponent::speed>(
                    noc::MakePropertyId(
                        "Nocturne.Tests.ReflectionTestComponent.speed"),
                    "speed",
                    kReflectionTestTypeId,
                    noc::BuiltinTypeIds::Float32,
                    kAuthorable),
            noc::MakeMemberPropertyMetadata<
                ReflectionTestComponent,
                bool,
                &ReflectionTestComponent::enabled>(
                    noc::MakePropertyId(
                        "Nocturne.Tests.ReflectionTestComponent.enabled"),
                    "enabled",
                    kReflectionTestTypeId,
                    noc::BuiltinTypeIds::Bool,
                    kAuthorable)
        };

        const noc::ComponentMetadata componentOps{
            noc::ComponentReflectionFlags::EditorAddable
                | noc::ComponentReflectionFlags::EditorRemovable,
            &HasTestComponent,
            &AddTestComponent,
            &RemoveTestComponent,
            &GetTestComponent,
            &GetMutableTestComponent
        };

        noc::TypeMetadata metadata =
            noc::MakeTypeMetadata<ReflectionTestComponent>(
                kReflectionTestTypeId,
                "Nocturne.Tests.ReflectionTestComponent",
                noc::TypeKind::Component,
                1,
                noc::TypeFlags::EditorVisible
                    | noc::TypeFlags::Serializable);

        metadata.properties = properties;
        metadata.propertyCount = 2;
        metadata.componentMetadata = &componentOps;

        return registry.RegisterType(metadata);
    }
}

bool RunPhase16ReflectionOcpTests()
{
    NOC_LOG_INFO(
        "Phase16OCP",
        "%s",
        "Open/Closed reflection extension test begin");

    noc::MallocAllocator backing;
    noc::DebugAlloc allocator(backing);
    noc::ReflectionRegistry registry;
    noc::World world;

    gStore = {};
    bool ok = true;

    ok &= CheckOcp(
        registry.Init(allocator, 32),
        "ReflectionRegistry init failed");
    ok &= CheckOcp(
        noc::RegisterBuiltinReflectionTypes(registry),
        "Builtin reflection registration failed");

    // Acceptance criterion: this is the only new-component registration call.
    // No World/component-registry/property-command/inspector central switch is
    // modified for ReflectionTestComponent.
    ok &= CheckOcp(
        RegisterReflectionTestComponent(registry),
        "Synthetic component registration failed");
    ok &= CheckOcp(
        registry.Freeze(),
        "Synthetic component registry Freeze failed");

    ok &= CheckOcp(
        registry.ComponentTypeCount() == 1,
        "Synthetic component enumeration count mismatch");

    const noc::TypeMetadata* type =
        registry.FindType(kReflectionTestTypeId);

    ok &= CheckOcp(
        type
            && type->kind == noc::TypeKind::Component
            && type->propertyCount == 2
            && type->componentMetadata,
        "Synthetic component schema mismatch");

    ok &= CheckOcp(
        world.Init(allocator, registry),
        "World init with synthetic reflected component failed");

    const noc::EntityHandle entity = world.CreateEntity();
    ok &= CheckOcp(
        entity.IsValid(),
        "Synthetic component test entity creation failed");

    ok &= CheckOcp(
        type->componentMetadata->add(world, entity),
        "Generic synthetic component add failed");
    ok &= CheckOcp(
        noc::ReflectedComponentCountForEntity(
            registry,
            world,
            entity) == 1,
        "Synthetic component was not generically enumerable");

    auto* component =
        static_cast<ReflectionTestComponent*>(
            type->componentMetadata->getMutable(world, entity));

    ok &= CheckOcp(
        component != nullptr,
        "Synthetic mutable component adapter failed");

    noc::PropertyAccessContext propertyContext{};
    propertyContext.object = component;
    propertyContext.mutableObject = component;

    const noc::PropertyMetadata* speed =
        registry.FindPropertyByName(
            kReflectionTestTypeId,
            "speed");
    const noc::PropertyMetadata* enabled =
        registry.FindPropertyByName(
            kReflectionTestTypeId,
            "enabled");

    const float newSpeed = 7.5f;
    const bool newEnabled = false;

    ok &= CheckOcp(
        speed
            && noc::WritePropertyValue(
                *speed,
                propertyContext,
                noc::ReflectedConstValueView{
                    noc::BuiltinTypeIds::Float32,
                    &newSpeed })
                == noc::PropertyAccessStatus::Success,
        "Generic synthetic speed write failed");

    ok &= CheckOcp(
        enabled
            && noc::WritePropertyValue(
                *enabled,
                propertyContext,
                noc::ReflectedConstValueView{
                    noc::BuiltinTypeIds::Bool,
                    &newEnabled })
                == noc::PropertyAccessStatus::Success,
        "Generic synthetic enabled write failed");

    ok &= CheckOcp(
        component
            && component->speed == 7.5f
            && !component->enabled,
        "Generic synthetic property writes did not persist");

    ok &= CheckOcp(
        type->componentMetadata->remove(world, entity)
            && noc::ReflectedComponentCountForEntity(
                registry,
                world,
                entity) == 0,
        "Generic synthetic component remove failed");

    ok &= CheckOcp(
        world.DestroyEntity(entity),
        "Synthetic component test entity cleanup failed");

    world.Shutdown();
    registry.Shutdown();
    gStore = {};

    ok &= CheckOcp(
        allocator.OutstandingBytes() == 0,
        "OCP test leaked allocator-owned memory");

    NOC_LOG_INFO(
        "Phase16OCP",
        "Open/Closed reflection extension test %s",
        ok ? "PASS" : "FAIL");

    return ok;
}
