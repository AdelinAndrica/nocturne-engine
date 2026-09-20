#pragma once

#include "Resources/ResourceHandle.h"
#include "Runtime/Bounds.h"
#include "Runtime/ComponentType.h"

#include <cstdint>

namespace noc
{
    inline constexpr ComponentTypeId kRenderableComponentTypeId{ 2 };
    inline constexpr uint32_t kRenderableComponentVersion = 1;
    inline constexpr const char* kRenderableComponentCanonicalName =
        "Nocturne.Renderable";

    /**
     * @brief Render-authoring component containing mesh identity plus local/derived bounds.
     *
     * @par Authoritative vs derived data
     * @c mesh, @c localBounds and @c enabled are authored/runtime state.
     * @c worldBounds and @c worldBoundsDirty are derived/cache state updated from Transform.
     *
     * @par Ownership
     * @c mesh is a non-owning ResourceHandle. ResourceManager owns the resource.
     *
     * @par Extraction requirement
     * An entity may legally store Renderable without Transform, but World render extraction
     * requires both components before an instance can be submitted.
     *
     * @ingroup world_ecs
     */
    struct RenderableComponent
    {
        /** Non-owning mesh resource handle. */
        ResourceHandle mesh{};

        /** Authoritative mesh/object bounds in local space. */
        AABB localBounds{
            Vec3::Zero(),
            Vec3::Zero()
        };

        /** Derived bounds after applying the entity world transform. */
        AABB worldBounds{
            Vec3::Zero(),
            Vec3::Zero()
        };

        /** False removes the component from render extraction without removing it. */
        bool enabled = true;

        /** True when worldBounds must be rebuilt from localBounds + Transform. */
        bool worldBoundsDirty = true;
    };

    /** @brief Returns canonical metadata for RenderableComponent. */
    [[nodiscard]] constexpr ComponentTypeMetadata RenderableComponentMetadata() noexcept
    {
        return MakeComponentTypeMetadata<RenderableComponent>(
            kRenderableComponentTypeId,
            kRenderableComponentCanonicalName,
            kRenderableComponentVersion,
            ComponentTypeFlags::EditorVisible | ComponentTypeFlags::Serializable);
    }
}
