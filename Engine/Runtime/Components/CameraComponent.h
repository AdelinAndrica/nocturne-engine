#pragma once

#include "Core/Math/MathTypes.h"
#include "Runtime/ComponentType.h"

#include <cstdint>

namespace noc
{
    inline constexpr ComponentTypeId kCameraComponentTypeId{ 3 };
    inline constexpr uint32_t kCameraComponentVersion = 1;
    inline constexpr const char* kCameraComponentCanonicalName =
        "Nocturne.Camera";

    // Camera lens state plus derived matrix cache.
    //
    // Book grounding:
    // - Luna, Introduction to 3D Game Programming with DirectX 12, Camera
    //   chapter: camera position/basis and frustum properties are the essential
    //   camera data, including FOV, aspect, near and far distances.
    // - Lengyel, Foundations Vol. 2, Chapter 6: perspective projection depends
    //   on vertical FOV, viewport aspect ratio, near distance and far distance.
    //
    // Design choice (not directly from the book): view/proj/viewProj are cached
    // derived values. TransformComponent remains the authoritative spatial state.
    struct CameraComponent
    {
        float fovYRadians = 1.04719755f; // 60 degrees
        float aspect = 16.0f / 9.0f;
        float nearZ = 0.1f;
        float farZ = 500.0f;

        bool enabled = true;

        Mat4 view = Mat4::Identity();
        Mat4 proj = Mat4::Identity();
        Mat4 viewProj = Mat4::Identity();
    };

    [[nodiscard]] constexpr ComponentTypeMetadata CameraComponentMetadata() noexcept
    {
        return MakeComponentTypeMetadata<CameraComponent>(
            kCameraComponentTypeId,
            kCameraComponentCanonicalName,
            kCameraComponentVersion,
            ComponentTypeFlags::EditorVisible | ComponentTypeFlags::Serializable);
    }
}
