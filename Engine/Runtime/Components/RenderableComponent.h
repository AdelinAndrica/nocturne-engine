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

    // Render-facing authored state plus derived bounds cache.
    //
    // Design choice (not directly from the book): localBounds is authoritative
    // authored data; worldBounds is derived/cache data and never owns transform
    // state. Transform changes must invalidate the cache through RenderableSystem.
    struct RenderableComponent
    {
        ResourceHandle mesh{};

        AABB localBounds{
            Vec3::Zero(),
            Vec3::Zero()
        };

        AABB worldBounds{
            Vec3::Zero(),
            Vec3::Zero()
        };

        bool enabled = true;
        bool worldBoundsDirty = true;
    };

    [[nodiscard]] constexpr ComponentTypeMetadata RenderableComponentMetadata() noexcept
    {
        return MakeComponentTypeMetadata<RenderableComponent>(
            kRenderableComponentTypeId,
            kRenderableComponentCanonicalName,
            kRenderableComponentVersion,
            ComponentTypeFlags::EditorVisible | ComponentTypeFlags::Serializable);
    }
}
