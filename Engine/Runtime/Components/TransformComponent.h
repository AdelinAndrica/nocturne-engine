#pragma once

#include "Core/Math/MathTypes.h"
#include "Runtime/ComponentType.h"
#include "Runtime/Entity.h"

#include <cstdint>

namespace noc
{
    inline constexpr ComponentTypeId kTransformComponentTypeId{ 1 };
    inline constexpr uint32_t kTransformComponentVersion = 1;
    inline constexpr const char* kTransformComponentCanonicalName =
        "Nocturne.Transform";

    /**
     * @brief Spatial component whose local TRS is authoritative and world matrix is derived.
     *
     * TransformComponent stores local translation/rotation/scale plus hierarchy links.
     * World/TransformSystem derive @c world from the local state and parent chain.
     *
     * @par Beginner rule
     * Edit localTranslation/localRotation/localScale through World/TransformSystem APIs.
     * Treat @c world and @c dirty as derived/cache state rather than independent authored
     * values.
     *
     * @par Hierarchy
     * Parent/child/sibling relationships are EntityHandle values, not pointers into dense
     * component storage. This keeps hierarchy identity stable when storage relocates.
     *
     * @par Book grounding
     * Transform hierarchy concepts are grounded in Eric Lengyel,
     * Foundations of Game Engine Development, Volume 1.
     * The exact sibling-link storage is a Nocturne design choice.
     *
     * @ingroup world_ecs
     */
    struct TransformComponent
    {
        /** Authoritative local-space translation. */
        Vec3 localTranslation = Vec3::Zero();

        /** Authoritative local-space rotation. */
        Quat localRotation = Quat::Identity();

        /** Authoritative local-space scale. */
        Vec3 localScale = Vec3::One();

        /** Derived object-to-world matrix. Do not treat as authored state. */
        Mat4 world = Mat4::Identity();

        /** Parent entity, or invalid for a root transform. */
        EntityHandle parent{};

        /** First child in the sibling chain, or invalid when there are no children. */
        EntityHandle firstChild{};

        /** Last child in the sibling chain, or invalid when there are no children. */
        EntityHandle lastChild{};

        /** Previous sibling under the same parent, or invalid. */
        EntityHandle previousSibling{};

        /** Next sibling under the same parent, or invalid. */
        EntityHandle nextSibling{};

        /** True when derived transform state requires recomputation. */
        bool dirty = true;
    };

    /** @brief Returns canonical metadata for TransformComponent. */
    [[nodiscard]] constexpr ComponentTypeMetadata TransformComponentMetadata() noexcept
    {
        return MakeComponentTypeMetadata<TransformComponent>(
            kTransformComponentTypeId,
            kTransformComponentCanonicalName,
            kTransformComponentVersion,
            ComponentTypeFlags::EditorVisible | ComponentTypeFlags::Serializable);
    }
}
