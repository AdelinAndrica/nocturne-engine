#include "Core/Log.h"
#include "Core/Memory/Allocator.h"
#include "Runtime/Components/LightComponent.h"
#include "Runtime/Reflection/BuiltinTypes.h"
#include "Runtime/Reflection/ComponentReflection.h"
#include "Runtime/Reflection/FoundationComponents.h"
#include "Runtime/Reflection/LightReflection.h"
#include "Runtime/Reflection/PropertyAccess.h"
#include "Runtime/Reflection/ReflectionRegistry.h"
#include "Runtime/World.h"

#include <cmath>

namespace
{
    bool CheckLight(bool condition, const char* message)
    {
        if (!condition)
        {
            NOC_LOG_ERROR("Phase16Light", "%s", message);
            return false;
        }
        return true;
    }
}

bool RunPhase16LightTests()
{
    NOC_LOG_INFO("Phase16Light", "%s", "Light foundation tests begin");

    bool ok = true;
    noc::MallocAllocator allocator;
    noc::ReflectionRegistry reflection;
    noc::World world;

    ok &= CheckLight(
        reflection.Init(allocator, 40),
        "ReflectionRegistry init failed");
    ok &= CheckLight(
        noc::RegisterBuiltinReflectionTypes(reflection)
            && noc::RegisterFoundationComponentReflectionTypes(reflection)
            && noc::RegisterLightReflectionTypes(reflection)
            && reflection.Freeze(),
        "Light reflection registration/freeze failed");
    ok &= CheckLight(
        world.Init(allocator, reflection),
        "World init failed");

    const noc::EntityHandle entity = world.CreateEntity();
    ok &= CheckLight(entity.IsValid(), "Entity creation failed");
    ok &= CheckLight(world.AddTransform(entity), "Transform add failed");
    ok &= CheckLight(
        world.AddLight(entity, noc::LightType::Spot),
        "Spot light add failed");
    ok &= CheckLight(
        !world.AddLight(entity, noc::LightType::Point),
        "Duplicate LightComponent add must fail");

    const noc::LightComponent* light = world.GetLight(entity);
    ok &= CheckLight(
        light && light->type == noc::LightType::Spot,
        "Light type/default state mismatch");

    ok &= CheckLight(
        world.SetLightColor(entity, noc::Vec3{ 1.0f, 0.5f, 0.25f }),
        "Valid light color rejected");
    ok &= CheckLight(
        !world.SetLightColor(entity, noc::Vec3{ -1.0f, 0.5f, 0.25f }),
        "Negative light color accepted");
    ok &= CheckLight(
        world.SetLightIntensity(entity, 7.5f)
            && !world.SetLightIntensity(entity, -0.1f),
        "Light intensity validation failed");
    ok &= CheckLight(
        world.SetLightRange(entity, 12.0f)
            && !world.SetLightRange(entity, 0.0f),
        "Light range validation failed");
    ok &= CheckLight(
        world.SetLightSpotAngles(entity, 0.2f, 0.6f)
            && !world.SetLightSpotAngles(entity, 0.7f, 0.6f)
            && !world.SetLightSpotAngles(entity, 0.2f, 1.5707964f),
        "Spot angle validation failed");

    const noc::TypeId componentTypeId{
        noc::kLightComponentTypeId.value
    };
    const noc::TypeMetadata* componentType =
        reflection.FindType(componentTypeId);
    const noc::TypeMetadata* lightType =
        reflection.FindType(noc::FoundationTypeIds::LightType);

    ok &= CheckLight(
        componentType
            && componentType->kind == noc::TypeKind::Component
            && componentType->componentMetadata,
        "Light component reflection metadata missing");
    ok &= CheckLight(
        lightType
            && lightType->kind == noc::TypeKind::Enum
            && lightType->enumMetadata
            && lightType->enumMetadata->valueCount == 3,
        "LightType enum reflection metadata missing");

    const noc::PropertyMetadata* intensityProperty =
        reflection.FindProperty(
            componentTypeId,
            noc::MakePropertyId("Nocturne.Light.intensity"));
    ok &= CheckLight(
        intensityProperty != nullptr,
        "Reflected light intensity property missing");

    if (componentType
        && componentType->componentMetadata
        && intensityProperty)
    {
        noc::ComponentPropertyRuntimeContext runtime{
            &world,
            entity
        };
        noc::PropertyAccessContext context =
            noc::MakeComponentPropertyAccessContext(
                runtime,
                componentType->componentMetadata);

        const float reflectedIntensity = 3.25f;
        ok &= CheckLight(
            noc::WritePropertyValue(
                *intensityProperty,
                context,
                noc::ReflectedConstValueView{
                    noc::BuiltinTypeIds::Float32,
                    &reflectedIntensity })
                == noc::PropertyAccessStatus::Success,
            "Semantic reflected light intensity write failed");

        const float invalidIntensity = -3.0f;
        ok &= CheckLight(
            noc::WritePropertyValue(
                *intensityProperty,
                context,
                noc::ReflectedConstValueView{
                    noc::BuiltinTypeIds::Float32,
                    &invalidIntensity })
                != noc::PropertyAccessStatus::Success,
            "Invalid reflected light intensity write was accepted");

        light = world.GetLight(entity);
        ok &= CheckLight(
            light
                && std::fabs(light->intensity - reflectedIntensity) < 1.0e-5f,
            "Rejected reflected write mutated light state");
    }

    ok &= CheckLight(
        world.DestroyEntity(entity)
            && !world.HasLight(entity),
        "DestroyEntity did not remove LightComponent");

    world.Shutdown();
    reflection.Shutdown();

    NOC_LOG_INFO(
        "Phase16Light",
        "Light foundation tests %s",
        ok ? "PASS" : "FAIL");
    return ok;
}
