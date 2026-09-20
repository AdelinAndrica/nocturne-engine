#pragma once

#include "Core/Math/MathTypes.h"
#include "Runtime/ComponentType.h"

#include <cstdint>

namespace noc
{
    // Book grounding:
    // - Frank D. Luna, Introduction to 3D Game Programming with DirectX 12,
    //   "Lighting" distinguishes directional, point, and spot lights.
    // - Eric Lengyel, Foundations of Game Engine Development Vol. 2,
    //   "Lighting and Shadows" covers the same principal light classes.
    enum class LightType : uint32_t
    {
        Directional = 0,
        Point = 1,
        Spot = 2
    };

    inline constexpr ComponentTypeId kLightComponentTypeId{ 5 };
    inline constexpr uint32_t kLightComponentVersion = 1;
    inline constexpr const char* kLightComponentCanonicalName =
        "Nocturne.Light";

    // Design choice (not directly from the book): Nocturne stores the three
    // supported analytic light classes in one component schema. The light type
    // selects which subset of parameters participates in render extraction.
    // TransformComponent remains the authoritative source of light position and
    // orientation.
    struct LightComponent
    {
        LightType type = LightType::Point;
        Vec3 color = Vec3::One();
        float intensity = 1.0f;
        float range = 10.0f;
        float innerConeRadians = 0.34906585f; // 20 degrees
        float outerConeRadians = 0.52359878f; // 30 degrees
        bool enabled = true;
    };

    [[nodiscard]] constexpr ComponentTypeMetadata
    LightComponentMetadata() noexcept
    {
        return MakeComponentTypeMetadata<LightComponent>(
            kLightComponentTypeId,
            kLightComponentCanonicalName,
            kLightComponentVersion,
            ComponentTypeFlags::EditorVisible
                | ComponentTypeFlags::Serializable);
    }
}
