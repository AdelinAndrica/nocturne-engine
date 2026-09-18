#include "Runtime/Reflection/FoundationComponents.h"

#include "Resources/ResourceHandle.h"
#include "Runtime/Bounds.h"
#include "Runtime/Components/CameraComponent.h"
#include "Runtime/Components/NameComponent.h"
#include "Runtime/Components/RenderableComponent.h"
#include "Runtime/Components/TransformComponent.h"
#include "Runtime/Reflection/ReflectionMetadata.h"
#include "Runtime/Reflection/ReflectionRegistry.h"
#include "Runtime/World.h"

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

        template <typename Component>
        [[nodiscard]] TypeMetadata MakeFoundationComponentType(
            ComponentTypeId componentTypeId,
            const char* canonicalName,
            uint32_t version,
            const ComponentMetadata& componentMetadata)
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
    }

    bool RegisterFoundationComponentReflectionTypes(
        ReflectionRegistry& registry)
    {
        return registry.RegisterType(
                MakeFoundationComponentType<TransformComponent>(
                    kTransformComponentTypeId,
                    kTransformComponentCanonicalName,
                    kTransformComponentVersion,
                    kTransformOps))
            && registry.RegisterType(
                MakeFoundationComponentType<RenderableComponent>(
                    kRenderableComponentTypeId,
                    kRenderableComponentCanonicalName,
                    kRenderableComponentVersion,
                    kRenderableOps))
            && registry.RegisterType(
                MakeFoundationComponentType<CameraComponent>(
                    kCameraComponentTypeId,
                    kCameraComponentCanonicalName,
                    kCameraComponentVersion,
                    kCameraOps))
            && registry.RegisterType(
                MakeFoundationComponentType<NameComponent>(
                    kNameComponentTypeId,
                    kNameComponentCanonicalName,
                    kNameComponentVersion,
                    kNameOps));
    }
}
