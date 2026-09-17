#pragma once

#include "Runtime/ComponentStorage.h"
#include "Runtime/Components/TransformComponent.h"

#include <cstdint>

namespace noc
{
    class EntityRegistry;
    class IAllocator;

    // Owns TransformComponent storage and preserves hierarchy invariants.
    //
    // Structural mutation is main-thread-only in Phase 15.
    // Update() performs deterministic, allocation-free top-down propagation.
    class TransformSystem
    {
    public:
        TransformSystem() = default;
        ~TransformSystem();

        TransformSystem(const TransformSystem&) = delete;
        TransformSystem& operator=(const TransformSystem&) = delete;
        TransformSystem(TransformSystem&&) = delete;
        TransformSystem& operator=(TransformSystem&&) = delete;

        bool Init(
            EntityRegistry& entities,
            IAllocator& allocator,
            uint32_t initialCapacity = 64);
        void Shutdown();

        [[nodiscard]] TransformComponent* Add(EntityHandle entity);

        // Removing a transform promotes direct children to roots and preserves
        // their local TRS. Therefore their world transforms may change.
        [[nodiscard]] bool Remove(EntityHandle entity);

        [[nodiscard]] bool Has(EntityHandle entity) const;
        [[nodiscard]] const TransformComponent* Get(EntityHandle entity) const;

        [[nodiscard]] uint32_t Count() const;

        [[nodiscard]] bool SetLocalTRS(
            EntityHandle entity,
            const Vec3& translation,
            const Quat& rotation,
            const Vec3& scale);

        // parent == EntityHandle::Invalid() detaches child to a root.
        // Self-parenting and direct/indirect cycles are rejected.
        [[nodiscard]] bool SetParent(EntityHandle child, EntityHandle parent);

        [[nodiscard]] EntityHandle ParentOf(EntityHandle entity) const;
        [[nodiscard]] EntityHandle FirstChildOf(EntityHandle entity) const;
        [[nodiscard]] EntityHandle NextSiblingOf(EntityHandle entity) const;

        [[nodiscard]] bool IsDirty(EntityHandle entity) const;

        // Recomputes dirty transforms in deterministic root/entity-index order.
        void Update();

        // Ensures the requested entity's dirty ancestor subtree is current.
        [[nodiscard]] bool GetWorldMatrix(EntityHandle entity, Mat4& outWorld);

    private:
        [[nodiscard]] bool IsUsableEntity_(EntityHandle entity) const;
        [[nodiscard]] bool WouldCreateCycle_(
            EntityHandle child,
            EntityHandle candidateParent) const;

        void Detach_(EntityHandle child);
        void AttachAsLastChild_(EntityHandle child, EntityHandle parent);

        void MarkDirtySubtree_(EntityHandle root);
        void UpdateSubtree_(EntityHandle root);
        void UpdateOne_(EntityHandle entity);

        EntityRegistry* entities_ = nullptr;
        ComponentStorage<TransformComponent> transforms_;
    };
}
