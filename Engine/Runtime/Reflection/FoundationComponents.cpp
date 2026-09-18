#include "Runtime/Reflection/FoundationComponents.h"

#include "Resources/ResourceHandle.h"
#include "Runtime/Bounds.h"
#include "Runtime/Components/CameraComponent.h"
#include "Runtime/Components/NameComponent.h"
#include "Runtime/Components/RenderableComponent.h"
#include "Runtime/Components/TransformComponent.h"
#include "Runtime/Reflection/BuiltinTypes.h"
#include "Runtime/Reflection/ComponentReflection.h"
#include "Runtime/Reflection/ReflectionMetadata.h"
#include "Runtime/Reflection/ReflectionRegistry.h"
#include "Runtime/Reflection/ReflectionString.h"
#include "Runtime/World.h"

#include <cmath>

namespace noc
{
    namespace
    {
        [[nodiscard]] constexpr TypeId ToReflectionTypeId(
            ComponentTypeId componentTypeId) noexcept
        {
            return TypeId{
                static_cast<uint64_t>(componentTypeId.value)
            };
        }

        [[nodiscard]] const ComponentPropertyRuntimeContext*
        ComponentContext(const PropertyAccessContext& context)
        {
            const auto* runtime =
                static_cast<const ComponentPropertyRuntimeContext*>(
                    context.userContext);

            return runtime
                && runtime->world
                && runtime->entity.IsValid()
                    ? runtime
                    : nullptr;
        }

        [[nodiscard]] ComponentPropertyRuntimeContext*
        ComponentContext(PropertyAccessContext& context)
        {
            auto* runtime =
                static_cast<ComponentPropertyRuntimeContext*>(
                    context.userContext);

            return runtime
                && runtime->world
                && runtime->entity.IsValid()
                    ? runtime
                    : nullptr;
        }

        template <typename Component>
        [[nodiscard]] TypeMetadata MakeFoundationComponentType(
            ComponentTypeId componentTypeId,
            const char* canonicalName,
            uint32_t version,
            const ComponentMetadata& componentMetadata,
            const PropertyMetadata* properties = nullptr,
            uint32_t propertyCount = 0)
        {
            TypeMetadata metadata =
                MakeTypeMetadata<Component>(
                    ToReflectionTypeId(componentTypeId),
                    canonicalName,
                    TypeKind::Component,
                    version,
                    TypeFlags::EditorVisible
                        | TypeFlags::Serializable);
            metadata.componentMetadata = &componentMetadata;
            metadata.properties = properties;
            metadata.propertyCount = propertyCount;
            return metadata;
        }

        bool HasTransform(const World& world, EntityHandle entity)
        {
            return world.HasTransform(entity);
        }
        bool AddTransform(World& world, EntityHandle entity)
        {
            return world.AddTransform(entity);
        }
        bool RemoveTransform(World& world, EntityHandle entity)
        {
            return world.RemoveTransform(entity);
        }
        const void* GetTransform(const World& world, EntityHandle entity)
        {
            return world.GetTransform(entity);
        }

        bool HasRenderable(const World& world, EntityHandle entity)
        {
            return world.HasRenderable(entity);
        }
        bool AddRenderable(World& world, EntityHandle entity)
        {
            return world.AddRenderable(
                entity,
                ResourceHandle{},
                AABB{ Vec3::Zero(), Vec3::Zero() });
        }
        bool RemoveRenderable(World& world, EntityHandle entity)
        {
            return world.RemoveRenderable(entity);
        }
        const void* GetRenderable(const World& world, EntityHandle entity)
        {
            return world.GetRenderable(entity);
        }

        bool HasCamera(const World& world, EntityHandle entity)
        {
            return world.HasCamera(entity);
        }
        bool AddCamera(World& world, EntityHandle entity)
        {
            return world.AddCamera(entity);
        }
        bool RemoveCamera(World& world, EntityHandle entity)
        {
            return world.RemoveCamera(entity);
        }
        const void* GetCamera(const World& world, EntityHandle entity)
        {
            return world.GetCamera(entity);
        }

        bool HasName(const World& world, EntityHandle entity)
        {
            return world.HasName(entity);
        }
        bool AddName(World& world, EntityHandle entity)
        {
            return world.AddName(entity, "");
        }
        bool RemoveName(World& world, EntityHandle entity)
        {
            return world.RemoveName(entity);
        }
        const void* GetName(const World& world, EntityHandle entity)
        {
            return world.GetName(entity);
        }

        constexpr ComponentReflectionFlags kFoundationComponentFlags =
            ComponentReflectionFlags::EditorAddable
            | ComponentReflectionFlags::EditorRemovable;

        const ComponentMetadata kTransformOps{
            kFoundationComponentFlags,
            &HasTransform,
            &AddTransform,
            &RemoveTransform,
            &GetTransform,
            nullptr
        };
        const ComponentMetadata kRenderableOps{
            kFoundationComponentFlags,
            &HasRenderable,
            &AddRenderable,
            &RemoveRenderable,
            &GetRenderable,
            nullptr
        };
        const ComponentMetadata kCameraOps{
            kFoundationComponentFlags,
            &HasCamera,
            &AddCamera,
            &RemoveCamera,
            &GetCamera,
            nullptr
        };
        const ComponentMetadata kNameOps{
            kFoundationComponentFlags,
            &HasName,
            &AddName,
            &RemoveName,
            &GetName,
            nullptr
        };

        constexpr PropertyFlags kAuthorable =
            PropertyFlags::EditorVisible
            | PropertyFlags::Serializable
            | PropertyFlags::ScriptVisible;
        constexpr PropertyFlags kDerived =
            PropertyFlags::ReadOnly
            | PropertyFlags::Transient
            | PropertyFlags::Hidden;

        bool ReadTransformTranslation(
            const PropertyAccessContext& context,
            void* destination)
        {
            const auto* runtime = ComponentContext(context);
            if (!runtime || !destination)
                return false;
            const TransformComponent* component =
                runtime->world->GetTransform(runtime->entity);
            if (!component)
                return false;
            *static_cast<Vec3*>(destination) = component->localTranslation;
            return true;
        }

        bool WriteTransformTranslation(
            PropertyAccessContext& context,
            const void* source)
        {
            auto* runtime = ComponentContext(context);
            if (!runtime || !source)
                return false;
            const TransformComponent* component =
                runtime->world->GetTransform(runtime->entity);
            return component
                && runtime->world->SetLocalTRS(
                    runtime->entity,
                    *static_cast<const Vec3*>(source),
                    component->localRotation,
                    component->localScale);
        }

        bool ReadTransformRotation(
            const PropertyAccessContext& context,
            void* destination)
        {
            const auto* runtime = ComponentContext(context);
            if (!runtime || !destination)
                return false;
            const TransformComponent* component =
                runtime->world->GetTransform(runtime->entity);
            if (!component)
                return false;
            *static_cast<Quat*>(destination) = component->localRotation;
            return true;
        }

        bool WriteTransformRotation(
            PropertyAccessContext& context,
            const void* source)
        {
            auto* runtime = ComponentContext(context);
            if (!runtime || !source)
                return false;
            const TransformComponent* component =
                runtime->world->GetTransform(runtime->entity);
            return component
                && runtime->world->SetLocalTRS(
                    runtime->entity,
                    component->localTranslation,
                    *static_cast<const Quat*>(source),
                    component->localScale);
        }

        bool ReadTransformScale(
            const PropertyAccessContext& context,
            void* destination)
        {
            const auto* runtime = ComponentContext(context);
            if (!runtime || !destination)
                return false;
            const TransformComponent* component =
                runtime->world->GetTransform(runtime->entity);
            if (!component)
                return false;
            *static_cast<Vec3*>(destination) = component->localScale;
            return true;
        }

        bool WriteTransformScale(
            PropertyAccessContext& context,
            const void* source)
        {
            auto* runtime = ComponentContext(context);
            if (!runtime || !source)
                return false;
            const TransformComponent* component =
                runtime->world->GetTransform(runtime->entity);
            return component
                && runtime->world->SetLocalTRS(
                    runtime->entity,
                    component->localTranslation,
                    component->localRotation,
                    *static_cast<const Vec3*>(source));
        }

        bool ReadTransformWorld(
            const PropertyAccessContext& context,
            void* destination)
        {
            const auto* runtime = ComponentContext(context);
            if (!runtime || !destination
                || !runtime->world->HasTransform(runtime->entity))
            {
                return false;
            }

            *static_cast<Mat4*>(destination) =
                runtime->world->GetWorldMatrix(runtime->entity);
            return true;
        }

        bool ReadRenderableMesh(
            const PropertyAccessContext& context,
            void* destination)
        {
            const auto* runtime = ComponentContext(context);
            if (!runtime || !destination)
                return false;
            const RenderableComponent* component =
                runtime->world->GetRenderable(runtime->entity);
            if (!component)
                return false;
            *static_cast<ResourceHandle*>(destination) = component->mesh;
            return true;
        }

        bool WriteRenderableMesh(
            PropertyAccessContext& context,
            const void* source)
        {
            auto* runtime = ComponentContext(context);
            return runtime
                && source
                && runtime->world->SetRenderableMesh(
                    runtime->entity,
                    *static_cast<const ResourceHandle*>(source));
        }

        bool ReadRenderableLocalBounds(
            const PropertyAccessContext& context,
            void* destination)
        {
            const auto* runtime = ComponentContext(context);
            if (!runtime || !destination)
                return false;
            const RenderableComponent* component =
                runtime->world->GetRenderable(runtime->entity);
            if (!component)
                return false;
            *static_cast<AABB*>(destination) = component->localBounds;
            return true;
        }

        bool WriteRenderableLocalBounds(
            PropertyAccessContext& context,
            const void* source)
        {
            auto* runtime = ComponentContext(context);
            return runtime
                && source
                && runtime->world->SetRenderableLocalBounds(
                    runtime->entity,
                    *static_cast<const AABB*>(source));
        }

        bool ReadRenderableEnabled(
            const PropertyAccessContext& context,
            void* destination)
        {
            const auto* runtime = ComponentContext(context);
            if (!runtime || !destination)
                return false;
            const RenderableComponent* component =
                runtime->world->GetRenderable(runtime->entity);
            if (!component)
                return false;
            *static_cast<bool*>(destination) = component->enabled;
            return true;
        }

        bool WriteRenderableEnabled(
            PropertyAccessContext& context,
            const void* source)
        {
            auto* runtime = ComponentContext(context);
            return runtime
                && source
                && runtime->world->SetRenderableEnabled(
                    runtime->entity,
                    *static_cast<const bool*>(source));
        }

        bool ReadRenderableWorldBounds(
            const PropertyAccessContext& context,
            void* destination)
        {
            const auto* runtime = ComponentContext(context);
            if (!runtime || !destination)
                return false;
            const RenderableComponent* component =
                runtime->world->GetRenderable(runtime->entity);
            if (!component)
                return false;
            *static_cast<AABB*>(destination) = component->worldBounds;
            return true;
        }

        const CameraComponent* CameraFrom(
            const PropertyAccessContext& context)
        {
            const auto* runtime = ComponentContext(context);
            return runtime
                ? runtime->world->GetCamera(runtime->entity)
                : nullptr;
        }

        bool ReadCameraFloat(
            const PropertyAccessContext& context,
            void* destination,
            float CameraComponent::*member)
        {
            const CameraComponent* component = CameraFrom(context);
            if (!component || !destination)
                return false;
            *static_cast<float*>(destination) = component->*member;
            return true;
        }

        bool WriteCameraPerspectiveMember(
            PropertyAccessContext& context,
            const void* source,
            float CameraComponent::*member)
        {
            auto* runtime = ComponentContext(context);
            if (!runtime || !source)
                return false;

            const CameraComponent* component =
                runtime->world->GetCamera(runtime->entity);
            if (!component)
                return false;

            float fov = component->fovYRadians;
            float aspect = component->aspect;
            float nearZ = component->nearZ;
            float farZ = component->farZ;

            const float value = *static_cast<const float*>(source);
            if (member == &CameraComponent::fovYRadians) fov = value;
            else if (member == &CameraComponent::aspect) aspect = value;
            else if (member == &CameraComponent::nearZ) nearZ = value;
            else if (member == &CameraComponent::farZ) farZ = value;
            else return false;

            return runtime->world->SetCameraPerspective(
                runtime->entity,
                fov,
                aspect,
                nearZ,
                farZ);
        }

        bool ValidateCameraFov(
            const PropertyAccessContext&,
            const void* candidate)
        {
            if (!candidate)
                return false;
            const float value = *static_cast<const float*>(candidate);
            return std::isfinite(value)
                && value > 0.0f
                && value < 3.14159265358979323846f;
        }

        bool ValidateCameraAspect(
            const PropertyAccessContext&,
            const void* candidate)
        {
            if (!candidate)
                return false;
            const float value = *static_cast<const float*>(candidate);
            return std::isfinite(value) && value > 0.0f;
        }

        bool ValidateCameraNear(
            const PropertyAccessContext& context,
            const void* candidate)
        {
            const CameraComponent* component = CameraFrom(context);
            if (!component || !candidate)
                return false;
            const float value = *static_cast<const float*>(candidate);
            return std::isfinite(value)
                && value > 0.0f
                && component->farZ > value;
        }

        bool ValidateCameraFar(
            const PropertyAccessContext& context,
            const void* candidate)
        {
            const CameraComponent* component = CameraFrom(context);
            if (!component || !candidate)
                return false;
            const float value = *static_cast<const float*>(candidate);
            return std::isfinite(value)
                && value > component->nearZ;
        }

        bool ReadCameraFov(
            const PropertyAccessContext& c, void* d)
        { return ReadCameraFloat(c, d, &CameraComponent::fovYRadians); }
        bool ReadCameraAspect(
            const PropertyAccessContext& c, void* d)
        { return ReadCameraFloat(c, d, &CameraComponent::aspect); }
        bool ReadCameraNear(
            const PropertyAccessContext& c, void* d)
        { return ReadCameraFloat(c, d, &CameraComponent::nearZ); }
        bool ReadCameraFar(
            const PropertyAccessContext& c, void* d)
        { return ReadCameraFloat(c, d, &CameraComponent::farZ); }

        bool WriteCameraFov(PropertyAccessContext& c, const void* s)
        { return WriteCameraPerspectiveMember(c, s, &CameraComponent::fovYRadians); }
        bool WriteCameraAspect(PropertyAccessContext& c, const void* s)
        { return WriteCameraPerspectiveMember(c, s, &CameraComponent::aspect); }
        bool WriteCameraNear(PropertyAccessContext& c, const void* s)
        { return WriteCameraPerspectiveMember(c, s, &CameraComponent::nearZ); }
        bool WriteCameraFar(PropertyAccessContext& c, const void* s)
        { return WriteCameraPerspectiveMember(c, s, &CameraComponent::farZ); }

        bool ReadCameraEnabled(
            const PropertyAccessContext& context,
            void* destination)
        {
            const CameraComponent* component = CameraFrom(context);
            if (!component || !destination)
                return false;
            *static_cast<bool*>(destination) = component->enabled;
            return true;
        }

        bool WriteCameraEnabled(
            PropertyAccessContext& context,
            const void* source)
        {
            auto* runtime = ComponentContext(context);
            return runtime
                && source
                && runtime->world->SetCameraEnabled(
                    runtime->entity,
                    *static_cast<const bool*>(source));
        }

        bool ReadCameraView(
            const PropertyAccessContext& context,
            void* destination)
        {
            const CameraComponent* component = CameraFrom(context);
            if (!component || !destination)
                return false;
            *static_cast<Mat4*>(destination) = component->view;
            return true;
        }
        bool ReadCameraProj(
            const PropertyAccessContext& context,
            void* destination)
        {
            const CameraComponent* component = CameraFrom(context);
            if (!component || !destination)
                return false;
            *static_cast<Mat4*>(destination) = component->proj;
            return true;
        }
        bool ReadCameraViewProj(
            const PropertyAccessContext& context,
            void* destination)
        {
            const CameraComponent* component = CameraFrom(context);
            if (!component || !destination)
                return false;
            *static_cast<Mat4*>(destination) = component->viewProj;
            return true;
        }

        [[nodiscard]] bool RegisterTransform(ReflectionRegistry& registry)
        {
            const AttributeMetadata translationAttributes[] = {
                MakeStringAttribute(AttributeKind::Units, "meters")
            };
            PropertyMetadata properties[] = {
                {
                    MakePropertyId("Nocturne.Transform.localTranslation"),
                    "localTranslation",
                    ToReflectionTypeId(kTransformComponentTypeId),
                    BuiltinTypeIds::Vec3,
                    kAuthorable,
                    &ReadTransformTranslation,
                    &WriteTransformTranslation,
                    nullptr, nullptr, nullptr, nullptr,
                    translationAttributes, 1
                },
                {
                    MakePropertyId("Nocturne.Transform.localRotation"),
                    "localRotation",
                    ToReflectionTypeId(kTransformComponentTypeId),
                    BuiltinTypeIds::Quat,
                    kAuthorable,
                    &ReadTransformRotation,
                    &WriteTransformRotation
                },
                {
                    MakePropertyId("Nocturne.Transform.localScale"),
                    "localScale",
                    ToReflectionTypeId(kTransformComponentTypeId),
                    BuiltinTypeIds::Vec3,
                    kAuthorable,
                    &ReadTransformScale,
                    &WriteTransformScale
                },
                {
                    MakePropertyId("Nocturne.Transform.world"),
                    "world",
                    ToReflectionTypeId(kTransformComponentTypeId),
                    BuiltinTypeIds::Mat4,
                    kDerived,
                    &ReadTransformWorld,
                    nullptr
                }
            };

            TypeMetadata metadata =
                MakeFoundationComponentType<TransformComponent>(
                    kTransformComponentTypeId,
                    kTransformComponentCanonicalName,
                    kTransformComponentVersion,
                    kTransformOps,
                    properties,
                    4);
            return registry.RegisterType(metadata);
        }

        [[nodiscard]] bool RegisterRenderable(ReflectionRegistry& registry)
        {
            const AttributeMetadata meshAttributes[] = {
                MakeTypeIdAttribute(
                    AttributeKind::ResourceTypeConstraint,
                    BuiltinTypeIds::MeshResource)
            };

            PropertyMetadata properties[] = {
                {
                    MakePropertyId("Nocturne.Renderable.mesh"),
                    "mesh",
                    ToReflectionTypeId(kRenderableComponentTypeId),
                    BuiltinTypeIds::ResourceHandle,
                    kAuthorable | PropertyFlags::ResourceReference,
                    &ReadRenderableMesh,
                    &WriteRenderableMesh,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr,
                    meshAttributes,
                    1
                },
                {
                    MakePropertyId("Nocturne.Renderable.localBounds"),
                    "localBounds",
                    ToReflectionTypeId(kRenderableComponentTypeId),
                    BuiltinTypeIds::AABB,
                    kAuthorable,
                    &ReadRenderableLocalBounds,
                    &WriteRenderableLocalBounds
                },
                {
                    MakePropertyId("Nocturne.Renderable.enabled"),
                    "enabled",
                    ToReflectionTypeId(kRenderableComponentTypeId),
                    BuiltinTypeIds::Bool,
                    kAuthorable,
                    &ReadRenderableEnabled,
                    &WriteRenderableEnabled
                },
                {
                    MakePropertyId("Nocturne.Renderable.worldBounds"),
                    "worldBounds",
                    ToReflectionTypeId(kRenderableComponentTypeId),
                    BuiltinTypeIds::AABB,
                    kDerived,
                    &ReadRenderableWorldBounds,
                    nullptr
                }
            };

            TypeMetadata metadata =
                MakeFoundationComponentType<RenderableComponent>(
                    kRenderableComponentTypeId,
                    kRenderableComponentCanonicalName,
                    kRenderableComponentVersion,
                    kRenderableOps,
                    properties,
                    4);
            return registry.RegisterType(metadata);
        }

        [[nodiscard]] bool RegisterCamera(ReflectionRegistry& registry)
        {
            const AttributeMetadata fovAttributes[] = {
                MakeStringAttribute(AttributeKind::DisplayName, "Vertical FOV"),
                MakeBooleanAttribute(AttributeKind::Angle),
                MakeStringAttribute(AttributeKind::Units, "radians")
            };
            const AttributeMetadata distanceAttributes[] = {
                MakeStringAttribute(AttributeKind::Units, "meters")
            };

            PropertyMetadata properties[] = {
                {
                    MakePropertyId("Nocturne.Camera.fovYRadians"),
                    "fovYRadians",
                    ToReflectionTypeId(kCameraComponentTypeId),
                    BuiltinTypeIds::Float32,
                    kAuthorable,
                    &ReadCameraFov,
                    &WriteCameraFov,
                    nullptr, nullptr,
                    &ValidateCameraFov, nullptr,
                    fovAttributes, 3
                },
                {
                    MakePropertyId("Nocturne.Camera.aspect"),
                    "aspect",
                    ToReflectionTypeId(kCameraComponentTypeId),
                    BuiltinTypeIds::Float32,
                    kAuthorable,
                    &ReadCameraAspect,
                    &WriteCameraAspect,
                    nullptr, nullptr,
                    &ValidateCameraAspect
                },
                {
                    MakePropertyId("Nocturne.Camera.nearZ"),
                    "nearZ",
                    ToReflectionTypeId(kCameraComponentTypeId),
                    BuiltinTypeIds::Float32,
                    kAuthorable,
                    &ReadCameraNear,
                    &WriteCameraNear,
                    nullptr, nullptr,
                    &ValidateCameraNear, nullptr,
                    distanceAttributes, 1
                },
                {
                    MakePropertyId("Nocturne.Camera.farZ"),
                    "farZ",
                    ToReflectionTypeId(kCameraComponentTypeId),
                    BuiltinTypeIds::Float32,
                    kAuthorable,
                    &ReadCameraFar,
                    &WriteCameraFar,
                    nullptr, nullptr,
                    &ValidateCameraFar, nullptr,
                    distanceAttributes, 1
                },
                {
                    MakePropertyId("Nocturne.Camera.enabled"),
                    "enabled",
                    ToReflectionTypeId(kCameraComponentTypeId),
                    BuiltinTypeIds::Bool,
                    kAuthorable,
                    &ReadCameraEnabled,
                    &WriteCameraEnabled
                },
                {
                    MakePropertyId("Nocturne.Camera.view"),
                    "view",
                    ToReflectionTypeId(kCameraComponentTypeId),
                    BuiltinTypeIds::Mat4,
                    kDerived,
                    &ReadCameraView,
                    nullptr
                },
                {
                    MakePropertyId("Nocturne.Camera.proj"),
                    "proj",
                    ToReflectionTypeId(kCameraComponentTypeId),
                    BuiltinTypeIds::Mat4,
                    kDerived,
                    &ReadCameraProj,
                    nullptr
                },
                {
                    MakePropertyId("Nocturne.Camera.viewProj"),
                    "viewProj",
                    ToReflectionTypeId(kCameraComponentTypeId),
                    BuiltinTypeIds::Mat4,
                    kDerived,
                    &ReadCameraViewProj,
                    nullptr
                }
            };

            TypeMetadata metadata =
                MakeFoundationComponentType<CameraComponent>(
                    kCameraComponentTypeId,
                    kCameraComponentCanonicalName,
                    kCameraComponentVersion,
                    kCameraOps,
                    properties,
                    8);
            return registry.RegisterType(metadata);
        }

        bool ReadNameValue(
            const PropertyAccessContext& context,
            void* destination)
        {
            const auto* runtime = ComponentContext(context);
            if (!runtime || !destination)
                return false;

            const NameComponent* component =
                runtime->world->GetName(runtime->entity);
            if (!component)
                return false;

            return static_cast<ReflectionString*>(destination)
                ->Assign(component->value);
        }

        bool ValidateNameValue(
            const PropertyAccessContext&,
            const void* candidate)
        {
            if (!candidate)
                return false;

            return static_cast<const ReflectionString*>(candidate)->Size()
                <= kNameComponentMaxBytes;
        }

        bool WriteNameValue(
            PropertyAccessContext& context,
            const void* source)
        {
            auto* runtime = ComponentContext(context);
            if (!runtime || !source)
                return false;

            return runtime->world->SetName(
                runtime->entity,
                static_cast<const ReflectionString*>(source)->CStr());
        }

        [[nodiscard]] bool RegisterName(ReflectionRegistry& registry)
        {
            const AttributeMetadata attributes[] = {
                MakeStringAttribute(
                    AttributeKind::DisplayName,
                    "Name")
            };

            PropertyMetadata properties[] = {
                {
                    MakePropertyId("Nocturne.Name.value"),
                    "value",
                    ToReflectionTypeId(kNameComponentTypeId),
                    BuiltinTypeIds::String,
                    kAuthorable,
                    &ReadNameValue,
                    &WriteNameValue,
                    nullptr,
                    nullptr,
                    &ValidateNameValue,
                    nullptr,
                    attributes,
                    1
                }
            };

            TypeMetadata metadata =
                MakeFoundationComponentType<NameComponent>(
                    kNameComponentTypeId,
                    kNameComponentCanonicalName,
                    kNameComponentVersion,
                    kNameOps,
                    properties,
                    1);

            return registry.RegisterType(metadata);
        }
    }

    bool RegisterFoundationComponentReflectionTypes(
        ReflectionRegistry& registry)
    {
        return RegisterTransform(registry)
            && RegisterRenderable(registry)
            && RegisterCamera(registry)
            && RegisterName(registry);
    }
}
