#pragma once

#include "Runtime/ComponentStorage.h"
#include "Runtime/Components/RenderableComponent.h"

#include <cstdint>

namespace noc
{
    class EntityRegistry;
    class IAllocator;

    // Owns RenderableComponent storage and derived world-bounds cache.
    //
    // Structural mutation is main-thread-only in Phase 15.
    // Design choice (not directly from the book): render extraction will later
    // iterate this storage, but Phase 15 keeps renderer ownership separate and
    // only exposes runtime data operations here.
    class RenderableSystem
    {
    public:
        RenderableSystem() = default;
        ~RenderableSystem();

        RenderableSystem(const RenderableSystem&) = delete;
        RenderableSystem& operator=(const RenderableSystem&) = delete;
        RenderableSystem(RenderableSystem&&) = delete;
        RenderableSystem& operator=(RenderableSystem&&) = delete;

        bool Init(
            EntityRegistry& entities,
            IAllocator& allocator,
            uint32_t initialCapacity = 64);
        void Shutdown();

        [[nodiscard]] RenderableComponent* Add(EntityHandle entity);
        [[nodiscard]] bool Remove(EntityHandle entity);

        [[nodiscard]] bool Has(EntityHandle entity) const;
        [[nodiscard]] const RenderableComponent* Get(EntityHandle entity) const;

        [[nodiscard]] uint32_t Count() const;

        [[nodiscard]] bool SetMesh(EntityHandle entity, ResourceHandle mesh);
        [[nodiscard]] bool SetLocalBounds(EntityHandle entity, const AABB& localBounds);
        [[nodiscard]] bool SetEnabled(EntityHandle entity, bool enabled);

        [[nodiscard]] bool MarkWorldBoundsDirty(EntityHandle entity);

        // Rebuilds derived world-space bounds only when dirty.
        [[nodiscard]] bool UpdateWorldBounds(
            EntityHandle entity,
            const Mat4& worldMatrix);

        [[nodiscard]] uint32_t DenseCount() const;
        [[nodiscard]] EntityHandle OwnerAtDenseIndex(uint32_t denseIndex) const;
        [[nodiscard]] const RenderableComponent* ComponentAtDenseIndex(
            uint32_t denseIndex) const;

    private:
        [[nodiscard]] bool IsUsableEntity_(EntityHandle entity) const;

        EntityRegistry* entities_ = nullptr;
        ComponentStorage<RenderableComponent> renderables_;
    };
}
