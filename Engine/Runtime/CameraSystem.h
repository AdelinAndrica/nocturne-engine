#pragma once

#include "Runtime/ComponentStorage.h"
#include "Runtime/Components/CameraComponent.h"

#include <cstdint>

namespace noc
{
    class EntityRegistry;
    class IAllocator;
    class TransformSystem;

    // Owns CameraComponent storage and active-camera selection.
    //
    // CameraComponent may exist without TransformComponent, but it cannot be
    // activated or rebuilt until a transform exists.
    //
    // Design choice (not directly from the book): active camera identity is
    // stored as EntityHandle, not as a special camera index or raw pointer.
    class CameraSystem
    {
    public:
        CameraSystem() = default;
        ~CameraSystem();

        CameraSystem(const CameraSystem&) = delete;
        CameraSystem& operator=(const CameraSystem&) = delete;
        CameraSystem(CameraSystem&&) = delete;
        CameraSystem& operator=(CameraSystem&&) = delete;

        bool Init(
            EntityRegistry& entities,
            TransformSystem& transforms,
            IAllocator& allocator,
            uint32_t initialCapacity = 8);
        void Shutdown();

        [[nodiscard]] CameraComponent* Add(EntityHandle entity);
        [[nodiscard]] bool Remove(EntityHandle entity);

        [[nodiscard]] bool Has(EntityHandle entity) const;
        [[nodiscard]] const CameraComponent* Get(EntityHandle entity) const;
        [[nodiscard]] uint32_t Count() const;

        [[nodiscard]] bool SetPerspective(
            EntityHandle entity,
            float fovYRadians,
            float aspect,
            float nearZ,
            float farZ);

        [[nodiscard]] bool SetAspect(EntityHandle entity, float aspect);
        [[nodiscard]] bool SetEnabled(EntityHandle entity, bool enabled);

        [[nodiscard]] bool SetActive(EntityHandle entity);
        void ClearActive();

        // Returns Invalid when the stored active handle is stale, disabled, lacks
        // CameraComponent, or lacks TransformComponent.
        [[nodiscard]] EntityHandle ActiveCamera() const;

        // Rebuilds view/projection from the entity's current world transform and
        // lens parameters.
        [[nodiscard]] bool Rebuild(EntityHandle entity);

        // Rebuilds the active camera and returns its current matrices.
        [[nodiscard]] bool RebuildActive(
            Mat4& outView,
            Mat4& outProjection,
            Mat4& outViewProjection);

        [[nodiscard]] uint32_t DenseCount() const;
        [[nodiscard]] EntityHandle OwnerAtDenseIndex(uint32_t denseIndex) const;
        [[nodiscard]] const CameraComponent* ComponentAtDenseIndex(
            uint32_t denseIndex) const;

    private:
        [[nodiscard]] bool IsUsableEntity_(EntityHandle entity) const;
        [[nodiscard]] bool HasUsableTransform_(EntityHandle entity) const;
        [[nodiscard]] static bool IsValidPerspective_(
            float fovYRadians,
            float aspect,
            float nearZ,
            float farZ);

        EntityRegistry* entities_ = nullptr;
        TransformSystem* transforms_ = nullptr;
        ComponentStorage<CameraComponent> cameras_;
        EntityHandle activeCamera_{};
    };
}
