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

    // Spatial state owned by one runtime entity.
    //
    // Lengyel, Foundations of Game Engine Development Vol. 1, transform
    // hierarchy material grounds the parent/child tree model.
    //
    // Design choice (not directly from the book): sibling links are stored as
    // EntityHandles so hierarchy operations never depend on component-storage
    // addresses and remain valid when dense storage relocates.
    struct TransformComponent
    {
        Vec3 localTranslation = Vec3::Zero();
        Quat localRotation = Quat::Identity();
        Vec3 localScale = Vec3::One();

        Mat4 world = Mat4::Identity();

        EntityHandle parent{};
        EntityHandle firstChild{};
        EntityHandle lastChild{};
        EntityHandle previousSibling{};
        EntityHandle nextSibling{};

        bool dirty = true;
    };

    [[nodiscard]] constexpr ComponentTypeMetadata TransformComponentMetadata() noexcept
    {
        return MakeComponentTypeMetadata<TransformComponent>(
            kTransformComponentTypeId,
            kTransformComponentCanonicalName,
            kTransformComponentVersion,
            ComponentTypeFlags::EditorVisible | ComponentTypeFlags::Serializable);
    }
}
