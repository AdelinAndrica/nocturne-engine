#include "Runtime/RenderableSystem.h"

#include "Runtime/EntityRegistry.h"

namespace noc
{
    RenderableSystem::~RenderableSystem()
    {
        Shutdown();
    }

    bool RenderableSystem::Init(
        EntityRegistry& entities,
        IAllocator& allocator,
        uint32_t initialCapacity)
    {
        if (entities_)
            return true;

        const uint32_t sparseCapacity =
            entities.Capacity() > initialCapacity
                ? entities.Capacity()
                : initialCapacity;

        if (!renderables_.Init(
                allocator,
                initialCapacity,
                sparseCapacity))
        {
            return false;
        }

        entities_ = &entities;
        return true;
    }

    void RenderableSystem::Shutdown()
    {
        renderables_.Shutdown();
        entities_ = nullptr;
    }

    RenderableComponent* RenderableSystem::Add(EntityHandle entity)
    {
        if (!IsUsableEntity_(entity))
            return nullptr;

        return renderables_.Emplace(entity);
    }

    bool RenderableSystem::Remove(EntityHandle entity)
    {
        if (!IsUsableEntity_(entity))
            return false;

        return renderables_.Remove(entity);
    }

    bool RenderableSystem::Has(EntityHandle entity) const
    {
        return IsUsableEntity_(entity) && renderables_.Has(entity);
    }

    const RenderableComponent* RenderableSystem::Get(EntityHandle entity) const
    {
        if (!IsUsableEntity_(entity))
            return nullptr;

        return renderables_.Get(entity);
    }

    uint32_t RenderableSystem::Count() const
    {
        return renderables_.Count();
    }

    bool RenderableSystem::SetMesh(EntityHandle entity, ResourceHandle mesh)
    {
        if (!IsUsableEntity_(entity))
            return false;

        RenderableComponent* renderable = renderables_.Get(entity);
        if (!renderable)
            return false;

        renderable->mesh = mesh;
        return true;
    }

    bool RenderableSystem::SetLocalBounds(
        EntityHandle entity,
        const AABB& localBounds)
    {
        if (!IsUsableEntity_(entity))
            return false;

        RenderableComponent* renderable = renderables_.Get(entity);
        if (!renderable)
            return false;

        renderable->localBounds = localBounds;
        renderable->worldBoundsDirty = true;
        return true;
    }

    bool RenderableSystem::SetEnabled(EntityHandle entity, bool enabled)
    {
        if (!IsUsableEntity_(entity))
            return false;

        RenderableComponent* renderable = renderables_.Get(entity);
        if (!renderable)
            return false;

        renderable->enabled = enabled;
        return true;
    }

    bool RenderableSystem::MarkWorldBoundsDirty(EntityHandle entity)
    {
        if (!IsUsableEntity_(entity))
            return false;

        RenderableComponent* renderable = renderables_.Get(entity);
        if (!renderable)
            return false;

        renderable->worldBoundsDirty = true;
        return true;
    }

    bool RenderableSystem::UpdateWorldBounds(
        EntityHandle entity,
        const Mat4& worldMatrix)
    {
        if (!IsUsableEntity_(entity))
            return false;

        RenderableComponent* renderable = renderables_.Get(entity);
        if (!renderable)
            return false;

        if (!renderable->worldBoundsDirty)
            return true;

        renderable->worldBounds =
            TransformAabb(renderable->localBounds, worldMatrix);
        renderable->worldBoundsDirty = false;
        return true;
    }

    uint32_t RenderableSystem::DenseCount() const
    {
        return renderables_.Count();
    }

    EntityHandle RenderableSystem::OwnerAtDenseIndex(uint32_t denseIndex) const
    {
        return renderables_.OwnerAtDenseIndex(denseIndex);
    }

    const RenderableComponent* RenderableSystem::ComponentAtDenseIndex(
        uint32_t denseIndex) const
    {
        return renderables_.ComponentAtDenseIndex(denseIndex);
    }

    bool RenderableSystem::IsUsableEntity_(EntityHandle entity) const
    {
        return entities_ && entities_->IsAlive(entity);
    }
}
