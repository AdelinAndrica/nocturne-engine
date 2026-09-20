#include "Runtime/Reflection/LightReflection.h"

#include "Runtime/Components/LightComponent.h"
#include "Runtime/Reflection/BuiltinTypes.h"
#include "Runtime/Reflection/ComponentReflection.h"
#include "Runtime/Reflection/ReflectionMetadata.h"
#include "Runtime/Reflection/ReflectionRegistry.h"
#include "Runtime/World.h"

#include <cmath>

namespace noc
{
    namespace
    {
        [[nodiscard]] constexpr TypeId LightComponentTypeId() noexcept
        {
            return TypeId{ static_cast<uint64_t>(kLightComponentTypeId.value) };
        }

        [[nodiscard]] const ComponentPropertyRuntimeContext*
        ComponentContext(const PropertyAccessContext& context)
        {
            const auto* runtime =
                static_cast<const ComponentPropertyRuntimeContext*>(
                    context.userContext);
            return runtime && runtime->world && runtime->entity.IsValid()
                ? runtime
                : nullptr;
        }

        [[nodiscard]] ComponentPropertyRuntimeContext*
        ComponentContext(PropertyAccessContext& context)
        {
            auto* runtime =
                static_cast<ComponentPropertyRuntimeContext*>(
                    context.userContext);
            return runtime && runtime->world && runtime->entity.IsValid()
                ? runtime
                : nullptr;
        }

        bool HasLight(const World& world, EntityHandle entity)
        {
            return world.HasLight(entity);
        }

        bool AddLight(World& world, EntityHandle entity)
        {
            return world.AddLight(entity, LightType::Point);
        }

        bool RemoveLight(World& world, EntityHandle entity)
        {
            return world.RemoveLight(entity);
        }

        const void* GetLight(const World& world, EntityHandle entity)
        {
            return world.GetLight(entity);
        }

        constexpr ComponentReflectionFlags kLightComponentFlags =
            ComponentReflectionFlags::EditorAddable
            | ComponentReflectionFlags::EditorRemovable;

        const ComponentMetadata kLightOps{
            kLightComponentFlags,
            &HasLight,
            &AddLight,
            &RemoveLight,
            &GetLight,
            nullptr
        };

        constexpr PropertyFlags kAuthorable =
            PropertyFlags::EditorVisible
            | PropertyFlags::Serializable
            | PropertyFlags::ScriptVisible;

        [[nodiscard]] const LightComponent* LightFrom(
            const PropertyAccessContext& context)
        {
            const auto* runtime = ComponentContext(context);
            return runtime
                ? runtime->world->GetLight(runtime->entity)
                : nullptr;
        }

        bool ReadType(const PropertyAccessContext& context, void* destination)
        {
            const LightComponent* light = LightFrom(context);
            if (!light || !destination) return false;
            *static_cast<LightType*>(destination) = light->type;
            return true;
        }

        bool WriteType(PropertyAccessContext& context, const void* source)
        {
            auto* runtime = ComponentContext(context);
            return runtime && source
                && runtime->world->SetLightType(
                    runtime->entity,
                    *static_cast<const LightType*>(source));
        }

        bool ReadColor(const PropertyAccessContext& context, void* destination)
        {
            const LightComponent* light = LightFrom(context);
            if (!light || !destination) return false;
            *static_cast<Vec3*>(destination) = light->color;
            return true;
        }

        bool WriteColor(PropertyAccessContext& context, const void* source)
        {
            auto* runtime = ComponentContext(context);
            return runtime && source
                && runtime->world->SetLightColor(
                    runtime->entity,
                    *static_cast<const Vec3*>(source));
        }

        bool ReadIntensity(const PropertyAccessContext& context, void* destination)
        {
            const LightComponent* light = LightFrom(context);
            if (!light || !destination) return false;
            *static_cast<float*>(destination) = light->intensity;
            return true;
        }

        bool WriteIntensity(PropertyAccessContext& context, const void* source)
        {
            auto* runtime = ComponentContext(context);
            return runtime && source
                && runtime->world->SetLightIntensity(
                    runtime->entity,
                    *static_cast<const float*>(source));
        }

        bool ReadRange(const PropertyAccessContext& context, void* destination)
        {
            const LightComponent* light = LightFrom(context);
            if (!light || !destination) return false;
            *static_cast<float*>(destination) = light->range;
            return true;
        }

        bool WriteRange(PropertyAccessContext& context, const void* source)
        {
            auto* runtime = ComponentContext(context);
            return runtime && source
                && runtime->world->SetLightRange(
                    runtime->entity,
                    *static_cast<const float*>(source));
        }

        bool ReadInnerCone(const PropertyAccessContext& context, void* destination)
        {
            const LightComponent* light = LightFrom(context);
            if (!light || !destination) return false;
            *static_cast<float*>(destination) = light->innerConeRadians;
            return true;
        }

        bool ReadOuterCone(const PropertyAccessContext& context, void* destination)
        {
            const LightComponent* light = LightFrom(context);
            if (!light || !destination) return false;
            *static_cast<float*>(destination) = light->outerConeRadians;
            return true;
        }

        bool WriteInnerCone(PropertyAccessContext& context, const void* source)
        {
            auto* runtime = ComponentContext(context);
            if (!runtime || !source) return false;
            const LightComponent* light =
                runtime->world->GetLight(runtime->entity);
            return light
                && runtime->world->SetLightSpotAngles(
                    runtime->entity,
                    *static_cast<const float*>(source),
                    light->outerConeRadians);
        }

        bool WriteOuterCone(PropertyAccessContext& context, const void* source)
        {
            auto* runtime = ComponentContext(context);
            if (!runtime || !source) return false;
            const LightComponent* light =
                runtime->world->GetLight(runtime->entity);
            return light
                && runtime->world->SetLightSpotAngles(
                    runtime->entity,
                    light->innerConeRadians,
                    *static_cast<const float*>(source));
        }

        bool ReadEnabled(const PropertyAccessContext& context, void* destination)
        {
            const LightComponent* light = LightFrom(context);
            if (!light || !destination) return false;
            *static_cast<bool*>(destination) = light->enabled;
            return true;
        }

        bool WriteEnabled(PropertyAccessContext& context, const void* source)
        {
            auto* runtime = ComponentContext(context);
            return runtime && source
                && runtime->world->SetLightEnabled(
                    runtime->entity,
                    *static_cast<const bool*>(source));
        }

        bool ValidateNonNegativeFloat(
            const PropertyAccessContext&,
            const void* candidate)
        {
            if (!candidate) return false;
            const float value = *static_cast<const float*>(candidate);
            return std::isfinite(value) && value >= 0.0f;
        }

        bool ValidatePositiveFloat(
            const PropertyAccessContext&,
            const void* candidate)
        {
            if (!candidate) return false;
            const float value = *static_cast<const float*>(candidate);
            return std::isfinite(value) && value > 0.0f;
        }

        bool RegisterLightType(ReflectionRegistry& registry)
        {
            const EnumValueMetadata values[] = {
                MakeEnumValueMetadata<LightType>(
                    MakeEnumValueId("Nocturne.LightType.Directional"),
                    "Directional", LightType::Directional),
                MakeEnumValueMetadata<LightType>(
                    MakeEnumValueId("Nocturne.LightType.Point"),
                    "Point", LightType::Point),
                MakeEnumValueMetadata<LightType>(
                    MakeEnumValueId("Nocturne.LightType.Spot"),
                    "Spot", LightType::Spot)
            };
            const EnumMetadata enumMetadata{
                BuiltinTypeIds::UInt32, values, 3, false
            };
            TypeMetadata metadata =
                MakeTypeMetadata<LightType>(
                    FoundationTypeIds::LightType,
                    "Nocturne.LightType",
                    TypeKind::Enum,
                    1,
                    TypeFlags::EditorVisible
                        | TypeFlags::Serializable
                        | TypeFlags::ScriptVisible);
            metadata.enumMetadata = &enumMetadata;
            return registry.RegisterType(metadata);
        }

        bool RegisterLightComponent(ReflectionRegistry& registry)
        {
            const AttributeMetadata colorAttributes[] = {
                MakeBooleanAttribute(AttributeKind::Color)
            };
            const AttributeMetadata distanceAttributes[] = {
                MakeStringAttribute(AttributeKind::Units, "meters")
            };
            const AttributeMetadata angleAttributes[] = {
                MakeBooleanAttribute(AttributeKind::Angle),
                MakeStringAttribute(AttributeKind::Units, "radians")
            };

            PropertyMetadata properties[] = {
                {
                    MakePropertyId("Nocturne.Light.type"), "type",
                    LightComponentTypeId(), FoundationTypeIds::LightType,
                    kAuthorable, &ReadType, &WriteType
                },
                {
                    MakePropertyId("Nocturne.Light.color"), "color",
                    LightComponentTypeId(), BuiltinTypeIds::Vec3,
                    kAuthorable, &ReadColor, &WriteColor,
                    nullptr, nullptr, nullptr, nullptr,
                    colorAttributes, 1
                },
                {
                    MakePropertyId("Nocturne.Light.intensity"), "intensity",
                    LightComponentTypeId(), BuiltinTypeIds::Float32,
                    kAuthorable, &ReadIntensity, &WriteIntensity,
                    nullptr, nullptr, &ValidateNonNegativeFloat
                },
                {
                    MakePropertyId("Nocturne.Light.range"), "range",
                    LightComponentTypeId(), BuiltinTypeIds::Float32,
                    kAuthorable, &ReadRange, &WriteRange,
                    nullptr, nullptr, &ValidatePositiveFloat, nullptr,
                    distanceAttributes, 1
                },
                {
                    MakePropertyId("Nocturne.Light.innerConeRadians"), "innerConeRadians",
                    LightComponentTypeId(), BuiltinTypeIds::Float32,
                    kAuthorable, &ReadInnerCone, &WriteInnerCone,
                    nullptr, nullptr, nullptr, nullptr,
                    angleAttributes, 2
                },
                {
                    MakePropertyId("Nocturne.Light.outerConeRadians"), "outerConeRadians",
                    LightComponentTypeId(), BuiltinTypeIds::Float32,
                    kAuthorable, &ReadOuterCone, &WriteOuterCone,
                    nullptr, nullptr, nullptr, nullptr,
                    angleAttributes, 2
                },
                {
                    MakePropertyId("Nocturne.Light.enabled"), "enabled",
                    LightComponentTypeId(), BuiltinTypeIds::Bool,
                    kAuthorable, &ReadEnabled, &WriteEnabled
                }
            };

            TypeMetadata metadata =
                MakeTypeMetadata<LightComponent>(
                    LightComponentTypeId(),
                    kLightComponentCanonicalName,
                    TypeKind::Component,
                    kLightComponentVersion,
                    TypeFlags::EditorVisible
                        | TypeFlags::Serializable);
            metadata.componentMetadata = &kLightOps;
            metadata.properties = properties;
            metadata.propertyCount = 7;
            return registry.RegisterType(metadata);
        }
    }

    bool RegisterLightReflectionTypes(ReflectionRegistry& registry)
    {
        return RegisterLightType(registry)
            && RegisterLightComponent(registry);
    }
}
