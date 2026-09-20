#pragma once

#include "Runtime/ComponentStorage.h"
#include "Runtime/Components/LightComponent.h"

#include <cstdint>

namespace noc
{
    class EntityRegistry;
    class IAllocator;

    class LightSystem
    {
    public:
        LightSystem() = default;
        ~LightSystem();

        LightSystem(const LightSystem&) = delete;
        LightSystem& operator=(const LightSystem&) = delete;
        LightSystem(LightSystem&&) = delete;
        LightSystem& operator=(LightSystem&&) = delete;

        bool Init(
            EntityRegistry& entities,
            IAllocator& allocator,
            uint32_t initialCapacity = 32);
        void Shutdown();

        [[nodiscard]] LightComponent* Add(
            EntityHandle entity,
            LightType type = LightType::Point);
        [[nodiscard]] bool Remove(EntityHandle entity);
        [[nodiscard]] bool Has(EntityHandle entity) const;
        [[nodiscard]] const LightComponent* Get(EntityHandle entity) const;
        [[nodiscard]] uint32_t Count() const;

        [[nodiscard]] bool SetType(EntityHandle entity, LightType type);
        [[nodiscard]] bool SetColor(EntityHandle entity, const Vec3& color);
        [[nodiscard]] bool SetIntensity(EntityHandle entity, float intensity);
        [[nodiscard]] bool SetRange(EntityHandle entity, float range);
        [[nodiscard]] bool SetSpotAngles(
            EntityHandle entity,
            float innerConeRadians,
            float outerConeRadians);
        [[nodiscard]] bool SetEnabled(EntityHandle entity, bool enabled);

        [[nodiscard]] uint32_t DenseCount() const;
        [[nodiscard]] EntityHandle OwnerAtDenseIndex(uint32_t denseIndex) const;
        [[nodiscard]] const LightComponent* ComponentAtDenseIndex(
            uint32_t denseIndex) const;

    private:
        [[nodiscard]] bool IsUsableEntity_(EntityHandle entity) const;
        [[nodiscard]] static bool IsValidType_(LightType type) noexcept;
        [[nodiscard]] static bool IsValidColor_(const Vec3& color) noexcept;
        [[nodiscard]] static bool IsValidSpotAngles_(
            float innerConeRadians,
            float outerConeRadians) noexcept;

        EntityRegistry* entities_ = nullptr;
        ComponentStorage<LightComponent> lights_;
    };
}
