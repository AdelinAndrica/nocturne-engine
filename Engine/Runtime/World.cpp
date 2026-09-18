#include "Runtime/World.h"

#include "Core/Log.h"
#include "Core/Memory/Allocator.h"
#include "Core/Memory/LinearArena.h"
#include "Render/RenderQueue.h"
#include "Runtime/CameraSystem.h"
#include "Runtime/ComponentRegistry.h"
#include "Runtime/EntityRegistry.h"
#include "Runtime/Frustum.h"
#include "Runtime/NameSystem.h"
#include "Runtime/RenderableSystem.h"
#include "Runtime/TransformSystem.h"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <new>

namespace noc
{
    namespace
    {
        constexpr float kDefaultFovY = 1.04719755f;
        constexpr float kDefaultAspect = 16.0f / 9.0f;
        constexpr float kDefaultNearZ = 0.1f;
        constexpr float kDefaultFarZ = 500.0f;
        constexpr float kPi = 3.14159265358979323846f;

        [[nodiscard]] bool IsValidPerspective(
            float fovYRadians,
            float aspect,
            float nearZ,
            float farZ)
        {
            return fovYRadians > 0.0f
                && fovYRadians < kPi
                && aspect > 0.0f
                && nearZ > 0.0f
                && farZ > nearZ;
        }

        [[nodiscard]] bool AabbBeyondDistance(
            const AABB& bounds,
            const Vec3& eye,
            float maxDistance)
        {
            if (maxDistance <= 0.0f)
                return false;

            const float closestX =
                (std::max)(bounds.min.x, (std::min)(eye.x, bounds.max.x));
            const float closestY =
                (std::max)(bounds.min.y, (std::min)(eye.y, bounds.max.y));
            const float closestZ =
                (std::max)(bounds.min.z, (std::min)(eye.z, bounds.max.z));

            const float dx = closestX - eye.x;
            const float dy = closestY - eye.y;
            const float dz = closestZ - eye.z;

            return dx * dx + dy * dy + dz * dz
                > maxDistance * maxDistance;
        }
    }

    struct World::Impl
    {
        IAllocator* allocator = nullptr;

        EntityRegistry entities;
        ComponentRegistry componentTypes;

        TransformSystem transforms;
        RenderableSystem renderables;
        CameraSystem cameras;
        NameSystem names;

        float defaultFovY = kDefaultFovY;
        float defaultAspect = kDefaultAspect;
        float defaultNearZ = kDefaultNearZ;
        float defaultFarZ = kDefaultFarZ;

        [[nodiscard]] bool Init(IAllocator& inAllocator)
        {
            allocator = &inAllocator;

            if (!entities.Init(inAllocator, 64))
                return false;

            if (!componentTypes.Init(inAllocator, 8))
                return false;

            if (!componentTypes.Register(TransformComponentMetadata())
                || !componentTypes.Register(RenderableComponentMetadata())
                || !componentTypes.Register(CameraComponentMetadata())
                || !componentTypes.Register(NameComponentMetadata()))
            {
                return false;
            }

            if (!transforms.Init(entities, inAllocator, 64))
                return false;

            if (!renderables.Init(entities, inAllocator, 64))
                return false;

            if (!cameras.Init(entities, transforms, inAllocator, 8))
                return false;

            if (!names.Init(entities, inAllocator, 64))
                return false;

            return true;
        }

        void Shutdown()
        {
            names.Shutdown();
            cameras.Shutdown();
            renderables.Shutdown();
            transforms.Shutdown();
            componentTypes.Shutdown();
            entities.Shutdown();
            allocator = nullptr;
        }

        void UpdateDerivedState()
        {
            // TransformSystem marks full descendant subtrees dirty. Mirror that
            // fact into renderable bounds before transform dirty flags are cleared.
            const uint32_t denseRenderableCount = renderables.DenseCount();
            for (uint32_t i = 0; i < denseRenderableCount; ++i)
            {
                const EntityHandle entity = renderables.OwnerAtDenseIndex(i);
                if (entity.IsValid()
                    && transforms.Has(entity)
                    && transforms.IsDirty(entity))
                {
                    (void)renderables.MarkWorldBoundsDirty(entity);
                }
            }

            transforms.Update();

            // Rebuild only bounds caches that are dirty. Entities with a
            // Renderable but no Transform are legal but not render-extractable.
            for (uint32_t i = 0; i < denseRenderableCount; ++i)
            {
                const EntityHandle entity = renderables.OwnerAtDenseIndex(i);
                if (!entity.IsValid() || !transforms.Has(entity))
                    continue;

                Mat4 world{};
                if (transforms.GetWorldMatrix(entity, world))
                    (void)renderables.UpdateWorldBounds(entity, world);
            }
        }

        [[nodiscard]] Mat4 BuildViewProjection(
            uint32_t viewportW,
            uint32_t viewportH,
            Vec3& outEye)
        {
            const float aspect =
                viewportW > 0 && viewportH > 0
                    ? static_cast<float>(viewportW) / static_cast<float>(viewportH)
                    : defaultAspect;

            const EntityHandle active = cameras.ActiveCamera();
            if (active.IsValid())
            {
                (void)cameras.SetAspect(active, aspect);

                Mat4 view{};
                Mat4 projection{};
                Mat4 viewProjection{};

                if (cameras.RebuildActive(view, projection, viewProjection))
                {
                    Mat4 world{};
                    if (transforms.GetWorldMatrix(active, world))
                    {
                        outEye = Vec3{
                            world.m[12],
                            world.m[13],
                            world.m[14]
                        };
                    }

                    return viewProjection;
                }
            }

            // Compatibility fallback for runtime paths that have not selected a
            // CameraComponent yet. This matches the old World default eye.
            outEye = Vec3{ 0.0f, 0.0f, -5.0f };

            const Mat4 view = LookToLH(
                outEye,
                Vec3{ 0.0f, 0.0f, 1.0f },
                Vec3{ 0.0f, 1.0f, 0.0f });

            const Mat4 projection = PerspectiveFovLH(
                defaultFovY,
                aspect,
                defaultNearZ,
                defaultFarZ);

            return Mul(projection, view);
        }
    };

    World::~World()
    {
        Shutdown();
    }

    bool World::Init(IAllocator& persistentAlloc)
    {
        if (impl_)
            return true;

        void* memory =
            persistentAlloc.Allocate(sizeof(Impl), alignof(Impl));
        if (!memory)
            return false;

        impl_ = new (memory) Impl{};

        if (!impl_->Init(persistentAlloc))
        {
            impl_->Shutdown();
            impl_->~Impl();
            persistentAlloc.Deallocate(impl_);
            impl_ = nullptr;

            NOC_LOG_ERROR("World", "%s", "World ECS initialization failed");
            return false;
        }

        lastStats_ = {};
        cullingMaxDistance_ = 0.0f;
        cullingEnabled_ = true;
        debugCullDump_ = false;

        NOC_LOG_INFO(
            "World",
            "%s",
            "World initialized with Phase 15 entity/component runtime");
        return true;
    }

    void World::Shutdown()
    {
        if (!impl_)
            return;

        IAllocator* allocator = impl_->allocator;
        impl_->Shutdown();
        impl_->~Impl();
        allocator->Deallocate(impl_);
        impl_ = nullptr;

        lastStats_ = {};
        debugCullDump_ = false;

        NOC_LOG_INFO("World", "%s", "World shutdown");
    }

    void World::Update()
    {
        if (!impl_)
            return;

        impl_->UpdateDerivedState();
    }

    EntityHandle World::CreateEntity()
    {
        if (!impl_)
            return EntityHandle::Invalid();

        return impl_->entities.Create();
    }

    bool World::DestroyEntity(EntityHandle entity)
    {
        if (!impl_ || !impl_->entities.IsAlive(entity))
            return false;

        // Component teardown occurs while entity identity is still alive because
        // individual systems validate EntityRegistry before structural mutation.
        if (impl_->cameras.Has(entity))
            (void)impl_->cameras.Remove(entity);

        if (impl_->renderables.Has(entity))
            (void)impl_->renderables.Remove(entity);

        if (impl_->names.Has(entity))
            (void)impl_->names.Remove(entity);

        if (impl_->transforms.Has(entity))
            (void)impl_->transforms.Remove(entity);

        return impl_->entities.Destroy(entity);
    }

    bool World::IsAlive(EntityHandle entity) const
    {
        return impl_ && impl_->entities.IsAlive(entity);
    }

    uint32_t World::AliveCount() const
    {
        return impl_ ? impl_->entities.AliveCount() : 0u;
    }

    uint32_t World::EntityCapacity() const
    {
        return impl_ ? impl_->entities.Capacity() : 0u;
    }

    EntityHandle World::EntityAtIndex(uint32_t index) const
    {
        return impl_
            ? impl_->entities.EntityAtIndex(index)
            : EntityHandle::Invalid();
    }

    SceneObjectHandle World::CreateObject()
    {
        const EntityHandle entity = CreateEntity();
        if (!entity.IsValid())
            return EntityHandle::Invalid();

        if (!AddTransform(entity))
        {
            (void)DestroyEntity(entity);
            return EntityHandle::Invalid();
        }

        return entity;
    }

    void World::DestroyObject(SceneObjectHandle entity)
    {
        (void)DestroyEntity(entity);
    }

    bool World::AddTransform(EntityHandle entity)
    {
        return impl_
            && impl_->entities.IsAlive(entity)
            && impl_->transforms.Add(entity) != nullptr;
    }

    bool World::RemoveTransform(EntityHandle entity)
    {
        if (!impl_ || !impl_->entities.IsAlive(entity))
            return false;

        if (impl_->cameras.ActiveCamera() == entity)
            impl_->cameras.ClearActive();

        return impl_->transforms.Remove(entity);
    }

    const TransformComponent* World::GetTransform(EntityHandle entity) const
    {
        if (!impl_ || !impl_->entities.IsAlive(entity))
            return nullptr;

        return impl_->transforms.Get(entity);
    }

    bool World::SetLocalTRS(
        SceneObjectHandle entity,
        const Vec3& translation,
        const Quat& rotation,
        const Vec3& scale)
    {
        if (!impl_)
            return false;

        return impl_->transforms.SetLocalTRS(
            entity,
            translation,
            rotation,
            scale);
    }

    bool World::SetParent(
        SceneObjectHandle child,
        SceneObjectHandle parent)
    {
        if (!impl_ || !impl_->entities.IsAlive(child))
            return false;

        // Compatibility with Phase 14: any invalid/dead parent means detach.
        const EntityHandle targetParent =
            impl_->entities.IsAlive(parent)
                ? parent
                : EntityHandle::Invalid();

        return impl_->transforms.SetParent(child, targetParent);
    }

    Mat4 World::GetWorldMatrix(SceneObjectHandle entity)
    {
        Mat4 world = Mat4::Identity();

        if (!impl_)
            return world;

        if (!impl_->transforms.GetWorldMatrix(entity, world))
            return Mat4::Identity();

        return world;
    }

    bool World::AddRenderable(
        EntityHandle entity,
        ResourceHandle mesh,
        const AABB& localBounds)
    {
        if (!impl_ || !impl_->entities.IsAlive(entity))
            return false;

        if (!impl_->renderables.Add(entity))
            return false;

        if (!impl_->renderables.SetMesh(entity, mesh)
            || !impl_->renderables.SetLocalBounds(entity, localBounds))
        {
            (void)impl_->renderables.Remove(entity);
            return false;
        }

        return true;
    }

    bool World::RemoveRenderable(EntityHandle entity)
    {
        return impl_
            && impl_->entities.IsAlive(entity)
            && impl_->renderables.Remove(entity);
    }

    const RenderableComponent* World::GetRenderable(EntityHandle entity) const
    {
        if (!impl_)
            return nullptr;

        return impl_->renderables.Get(entity);
    }

    bool World::SetRenderable(
        SceneObjectHandle entity,
        ResourceHandle mesh,
        const AABB& localBounds)
    {
        if (!impl_ || !impl_->entities.IsAlive(entity))
            return false;

        if (!impl_->renderables.Has(entity))
            return AddRenderable(entity, mesh, localBounds);

        return impl_->renderables.SetMesh(entity, mesh)
            && impl_->renderables.SetLocalBounds(entity, localBounds);
    }

    bool World::SetRenderableEnabled(EntityHandle entity, bool enabled)
    {
        return impl_
            && impl_->renderables.SetEnabled(entity, enabled);
    }

    bool World::AddCamera(EntityHandle entity)
    {
        return impl_
            && impl_->entities.IsAlive(entity)
            && impl_->cameras.Add(entity) != nullptr;
    }

    bool World::RemoveCamera(EntityHandle entity)
    {
        return impl_
            && impl_->entities.IsAlive(entity)
            && impl_->cameras.Remove(entity);
    }

    const CameraComponent* World::GetCamera(EntityHandle entity) const
    {
        if (!impl_)
            return nullptr;

        return impl_->cameras.Get(entity);
    }

    bool World::SetCameraParams(
        float fovYRadians,
        float aspect,
        float nearZ,
        float farZ)
    {
        if (!impl_
            || !IsValidPerspective(
                fovYRadians,
                aspect,
                nearZ,
                farZ))
        {
            return false;
        }

        impl_->defaultFovY = fovYRadians;
        impl_->defaultAspect = aspect;
        impl_->defaultNearZ = nearZ;
        impl_->defaultFarZ = farZ;

        const EntityHandle active = impl_->cameras.ActiveCamera();
        if (!active.IsValid())
            return true;

        return impl_->cameras.SetPerspective(
            active,
            fovYRadians,
            aspect,
            nearZ,
            farZ);
    }

    bool World::SetCameraFromObject(SceneObjectHandle entity)
    {
        if (!impl_
            || !impl_->entities.IsAlive(entity)
            || !impl_->transforms.Has(entity))
        {
            return false;
        }

        if (!impl_->cameras.Has(entity))
        {
            if (!impl_->cameras.Add(entity))
                return false;
        }

        if (!impl_->cameras.SetPerspective(
                entity,
                impl_->defaultFovY,
                impl_->defaultAspect,
                impl_->defaultNearZ,
                impl_->defaultFarZ))
        {
            return false;
        }

        return impl_->cameras.SetActive(entity);
    }

    EntityHandle World::ActiveCamera() const
    {
        return impl_
            ? impl_->cameras.ActiveCamera()
            : EntityHandle::Invalid();
    }

    bool World::AddName(EntityHandle entity, const char* name)
    {
        return impl_
            && impl_->entities.IsAlive(entity)
            && impl_->names.Add(entity, name) != nullptr;
    }

    bool World::RemoveName(EntityHandle entity)
    {
        return impl_
            && impl_->entities.IsAlive(entity)
            && impl_->names.Remove(entity);
    }

    bool World::SetName(EntityHandle entity, const char* name)
    {
        return impl_ && impl_->names.SetName(entity, name);
    }

    const NameComponent* World::GetName(EntityHandle entity) const
    {
        if (!impl_)
            return nullptr;

        return impl_->names.Get(entity);
    }

    const ComponentTypeMetadata* World::FindComponentType(
        ComponentTypeId typeId) const
    {
        return impl_
            ? impl_->componentTypes.Find(typeId)
            : nullptr;
    }

    void World::SetCullingMaxDistance(float meters)
    {
        cullingMaxDistance_ = meters < 0.0f ? 0.0f : meters;
    }

    RenderQueue World::BuildRenderQueue(
        LinearArena& frameArena,
        uint32_t viewportW,
        uint32_t viewportH)
    {
        RenderQueue queue{};

        if (!impl_)
            return queue;

        impl_->UpdateDerivedState();

        Vec3 eye{};
        const Mat4 viewProjection =
            impl_->BuildViewProjection(viewportW, viewportH, eye);
        const Frustum frustum = FrustumFromViewProj(viewProjection);

        queue.view.viewProj = viewProjection;
        queue.view.viewportWidth = viewportW;
        queue.view.viewportHeight = viewportH;

        uint32_t visibleCount = 0;
        uint32_t totalCount = 0;

        const uint32_t capacity = impl_->entities.Capacity();

        for (uint32_t index = 0; index < capacity; ++index)
        {
            const EntityHandle entity =
                impl_->entities.EntityAtIndex(index);
            if (!entity.IsValid())
                continue;

            const RenderableComponent* renderable =
                impl_->renderables.Get(entity);
            const TransformComponent* transform =
                impl_->transforms.Get(entity);

            if (!renderable
                || !transform
                || !renderable->enabled)
            {
                continue;
            }

            ++totalCount;

            bool keep = true;
            bool frustumHit = true;
            bool distanceHit = true;

            if (cullingEnabled_)
            {
                frustumHit =
                    AabbIntersectsFrustum(
                        renderable->worldBounds,
                        frustum);

                distanceHit =
                    !AabbBeyondDistance(
                        renderable->worldBounds,
                        eye,
                        cullingMaxDistance_);

                keep = frustumHit && distanceHit;
            }

            if (debugCullDump_)
            {
                const AABB& bounds = renderable->worldBounds;

                NOC_LOG_INFO(
                    "World",
                    "CullDump entity=%u:%u keep=%s frustum=%s distance=%s "
                    "bounds min(%.2f %.2f %.2f) max(%.2f %.2f %.2f)",
                    entity.index,
                    entity.generation,
                    keep ? "YES" : "NO",
                    frustumHit ? "YES" : "NO",
                    distanceHit ? "YES" : "NO",
                    bounds.min.x,
                    bounds.min.y,
                    bounds.min.z,
                    bounds.max.x,
                    bounds.max.y,
                    bounds.max.z);
            }

            if (keep)
                ++visibleCount;
        }

        queue.totalRenderables = totalCount;

        if (visibleCount == 0)
        {
            lastStats_.visible = 0;
            lastStats_.total = totalCount;

            if (debugCullDump_)
            {
                NOC_LOG_INFO(
                    "World",
                    "CullDump summary: visible=%u total=%u (culling=%s)",
                    lastStats_.visible,
                    lastStats_.total,
                    cullingEnabled_ ? "ON" : "OFF");
                debugCullDump_ = false;
            }

            return queue;
        }

        void* memory =
            frameArena.Allocate(
                sizeof(RenderInstance) * visibleCount,
                alignof(RenderInstance));

        if (!memory)
        {
            lastStats_.visible = 0;
            lastStats_.total = totalCount;

            NOC_LOG_ERROR(
                "World",
                "Render extraction failed: FrameArena could not allocate %zu bytes",
                sizeof(RenderInstance)
                    * static_cast<std::size_t>(visibleCount));

            debugCullDump_ = false;
            return queue;
        }

        auto* instances =
            static_cast<RenderInstance*>(memory);

        uint32_t writeIndex = 0;

        // Deterministic entity-index order. Dense component swap-remove does not
        // affect renderer submission order.
        for (uint32_t index = 0; index < capacity; ++index)
        {
            const EntityHandle entity =
                impl_->entities.EntityAtIndex(index);
            if (!entity.IsValid())
                continue;

            const RenderableComponent* renderable =
                impl_->renderables.Get(entity);
            const TransformComponent* transform =
                impl_->transforms.Get(entity);

            if (!renderable
                || !transform
                || !renderable->enabled)
            {
                continue;
            }

            bool keep = true;

            if (cullingEnabled_)
            {
                keep =
                    AabbIntersectsFrustum(
                        renderable->worldBounds,
                        frustum)
                    && !AabbBeyondDistance(
                        renderable->worldBounds,
                        eye,
                        cullingMaxDistance_);
            }

            if (!keep)
                continue;

            instances[writeIndex].mesh = renderable->mesh;
            instances[writeIndex].world = transform->world;
            ++writeIndex;
        }

        queue.instances = instances;
        queue.instanceCount = writeIndex;

        lastStats_.visible = writeIndex;
        lastStats_.total = totalCount;

        if (debugCullDump_)
        {
            NOC_LOG_INFO(
                "World",
                "CullDump summary: visible=%u total=%u (culling=%s)",
                lastStats_.visible,
                lastStats_.total,
                cullingEnabled_ ? "ON" : "OFF");
            debugCullDump_ = false;
        }

        return queue;
    }

    const WorldStats& World::GetLastStats() const
    {
        return lastStats_;
    }

    void World::DebugRequestCullDump()
    {
        debugCullDump_ = true;
    }
}
