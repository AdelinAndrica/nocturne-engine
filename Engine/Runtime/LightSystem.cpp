#include "Runtime/LightSystem.h"

#include "Runtime/EntityRegistry.h"

#include <cmath>

namespace noc
{
    namespace
    {
        constexpr float kPiOverTwo = 1.57079632679489661923f;
    }

    LightSystem::~LightSystem()
    {
        Shutdown();
    }

    bool LightSystem::Init(
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

        if (!lights_.Init(allocator, initialCapacity, sparseCapacity))
            return false;

        entities_ = &entities;
        return true;
    }

    void LightSystem::Shutdown()
    {
        lights_.Shutdown();
        entities_ = nullptr;
    }

    LightComponent* LightSystem::Add(
        EntityHandle entity,
        LightType type)
    {
        if (!IsUsableEntity_(entity) || !IsValidType_(type))
            return nullptr;

        LightComponent* light = lights_.Emplace(entity);
        if (!light)
            return nullptr;

        light->type = type;
        return light;
    }

    bool LightSystem::Remove(EntityHandle entity)
    {
        return IsUsableEntity_(entity) && lights_.Remove(entity);
    }

    bool LightSystem::Has(EntityHandle entity) const
    {
        return IsUsableEntity_(entity) && lights_.Has(entity);
    }

    const LightComponent* LightSystem::Get(EntityHandle entity) const
    {
        return IsUsableEntity_(entity) ? lights_.Get(entity) : nullptr;
    }

    uint32_t LightSystem::Count() const
    {
        return lights_.Count();
    }

    bool LightSystem::SetType(EntityHandle entity, LightType type)
    {
        LightComponent* light =
            IsUsableEntity_(entity) ? lights_.Get(entity) : nullptr;
        if (!light || !IsValidType_(type))
            return false;

        light->type = type;
        return true;
    }

    bool LightSystem::SetColor(EntityHandle entity, const Vec3& color)
    {
        LightComponent* light =
            IsUsableEntity_(entity) ? lights_.Get(entity) : nullptr;
        if (!light || !IsValidColor_(color))
            return false;

        light->color = color;
        return true;
    }

    bool LightSystem::SetIntensity(EntityHandle entity, float intensity)
    {
        LightComponent* light =
            IsUsableEntity_(entity) ? lights_.Get(entity) : nullptr;
        if (!light || !std::isfinite(intensity) || intensity < 0.0f)
            return false;

        light->intensity = intensity;
        return true;
    }

    bool LightSystem::SetRange(EntityHandle entity, float range)
    {
        LightComponent* light =
            IsUsableEntity_(entity) ? lights_.Get(entity) : nullptr;
        if (!light || !std::isfinite(range) || range <= 0.0f)
            return false;

        light->range = range;
        return true;
    }

    bool LightSystem::SetSpotAngles(
        EntityHandle entity,
        float innerConeRadians,
        float outerConeRadians)
    {
        LightComponent* light =
            IsUsableEntity_(entity) ? lights_.Get(entity) : nullptr;
        if (!light
            || !IsValidSpotAngles_(
                innerConeRadians,
                outerConeRadians))
        {
            return false;
        }

        light->innerConeRadians = innerConeRadians;
        light->outerConeRadians = outerConeRadians;
        return true;
    }

    bool LightSystem::SetEnabled(EntityHandle entity, bool enabled)
    {
        LightComponent* light =
            IsUsableEntity_(entity) ? lights_.Get(entity) : nullptr;
        if (!light)
            return false;

        light->enabled = enabled;
        return true;
    }

    uint32_t LightSystem::DenseCount() const
    {
        return lights_.Count();
    }

    EntityHandle LightSystem::OwnerAtDenseIndex(uint32_t denseIndex) const
    {
        return lights_.OwnerAtDenseIndex(denseIndex);
    }

    const LightComponent* LightSystem::ComponentAtDenseIndex(
        uint32_t denseIndex) const
    {
        return lights_.ComponentAtDenseIndex(denseIndex);
    }

    bool LightSystem::IsUsableEntity_(EntityHandle entity) const
    {
        return entities_ && entities_->IsAlive(entity);
    }

    bool LightSystem::IsValidType_(LightType type) noexcept
    {
        switch (type)
        {
        case LightType::Directional:
        case LightType::Point:
        case LightType::Spot:
            return true;
        default:
            return false;
        }
    }

    bool LightSystem::IsValidColor_(const Vec3& color) noexcept
    {
        return std::isfinite(color.x)
            && std::isfinite(color.y)
            && std::isfinite(color.z)
            && color.x >= 0.0f
            && color.y >= 0.0f
            && color.z >= 0.0f;
    }

    bool LightSystem::IsValidSpotAngles_(
        float innerConeRadians,
        float outerConeRadians) noexcept
    {
        // Design choice (not directly from the book): authored cone values are
        // half-angles constrained below 90 degrees so a spot light cannot turn
        // into a backward-facing hemisphere.
        return std::isfinite(innerConeRadians)
            && std::isfinite(outerConeRadians)
            && innerConeRadians >= 0.0f
            && outerConeRadians > 0.0f
            && innerConeRadians <= outerConeRadians
            && outerConeRadians < kPiOverTwo;
    }
}
