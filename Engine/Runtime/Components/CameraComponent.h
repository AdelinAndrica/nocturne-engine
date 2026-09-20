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

    /**
     * @brief Camera lens state plus cached derived view/projection matrices.
     *
     * CameraComponent owns lens/projection parameters. Spatial position/orientation are
     * not duplicated here; TransformComponent is authoritative for camera transform.
     *
     * @par Authored values
     * @c fovYRadians, @c aspect, @c nearZ, @c farZ and @c enabled are camera state.
     *
     * @par Derived values
     * @c view, @c proj and @c viewProj are rebuilt from lens + Transform state.
     *
     * @par Valid perspective
     * World currently accepts perspective settings only when:
     * - 0 < fovYRadians < pi
     * - aspect > 0
     * - nearZ > 0
     * - farZ > nearZ
     *
     * @par Book grounding
     * Camera/frustum/projection concepts are grounded in Frank Luna,
     * Introduction to 3D Game Programming with DirectX 12, and Eric Lengyel,
     * Foundations of Game Engine Development, Volume 2.
     *
     * @ingroup world_ecs
     */
    struct CameraComponent
    {
        /** Vertical field of view in radians. Default: 60 degrees. */
        float fovYRadians = 1.04719755f;

        /** Width / height aspect ratio. */
        float aspect = 16.0f / 9.0f;

        /** Positive near clipping distance. */
        float nearZ = 0.1f;

        /** Far clipping distance, greater than nearZ. */
        float farZ = 500.0f;

        /** Camera-system enable state. */
        bool enabled = true;

        /** Derived world-to-view matrix. */
        Mat4 view = Mat4::Identity();

        /** Derived perspective projection matrix. */
        Mat4 proj = Mat4::Identity();

        /** Derived combined projection * view matrix. */
        Mat4 viewProj = Mat4::Identity();
    };

    /** @brief Returns canonical metadata for CameraComponent. */
    [[nodiscard]] constexpr ComponentTypeMetadata CameraComponentMetadata() noexcept
    {
        return MakeComponentTypeMetadata<CameraComponent>(
            kCameraComponentTypeId,
            kCameraComponentCanonicalName,
            kCameraComponentVersion,
            ComponentTypeFlags::EditorVisible | ComponentTypeFlags::Serializable);
    }
}
