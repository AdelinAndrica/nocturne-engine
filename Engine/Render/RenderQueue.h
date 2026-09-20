#pragma once
#include <cstdint>

#include "Core/Math/MathTypes.h"
#include "Resources/ResourceHandle.h"

namespace noc
{
    /**
     * @brief Camera/view information consumed by the renderer for one frame.
     *
     * This is extracted frame data, not authoritative camera-component storage.
     * @ingroup rendering
     */
    struct RenderView
    {
        /** Combined projection * view matrix used for the submitted frame. */
        Mat4 viewProj;

        /** Current render-target width in pixels. */
        uint32_t viewportWidth = 0;

        /** Current render-target height in pixels. */
        uint32_t viewportHeight = 0;
    };

    /**
     * @brief One renderer-facing mesh instance for the current frame.
     *
     * The mesh is referenced by ResourceHandle rather than by a raw resource pointer,
     * preserving the Resources/Rendering boundary.
     *
     * @ingroup rendering
     */
    struct RenderInstance
    {
        /** Non-owning ResourceManager mesh handle. */
        ResourceHandle mesh;

        /** Object-to-world transform for this submitted instance. */
        Mat4 world;
    };

    /**
     * @brief Optional editor/development selection visualization payload.
     *
     * This tiny bridge lets rendering depth-test selection bounds without depending
     * on editor-specific Win32 classes.
     *
     * @note This is development/editor scaffolding, not gameplay selection state.
     * @ingroup rendering
     */
    struct RenderDebugSelection
    {
        Vec3 localBoundsMin = Vec3::Zero();
        Vec3 localBoundsMax = Vec3::Zero();
        Mat4 world = Mat4::Identity();

        /** Non-zero means the debug bounds should be rendered. */
        uint32_t enabled = 0;
    };

    /**
     * @brief POD submission packet describing one renderable frame.
     *
     * World::BuildRenderQueue() creates this packet from authoritative ECS state.
     * RenderSystem consumes it during the same frame.
     *
     * @par Ownership
     * RenderQueue does not own @c instances. In the normal engine path, the array is
     * allocated from Engine::FrameArena().
     *
     * @par Lifetime
     * Treat the queue and instance array as same-frame data only. The frame arena is
     * reset by the next Engine::BeginFrame().
     *
     * @par Why this boundary exists
     * The renderer receives compact renderer-facing data instead of direct mutable ECS
     * storage. Simulation/editor authority therefore stays in World.
     *
     * @see World::BuildRenderQueue
     * @see RenderSystem::SetFrameRenderQueue
     * @ingroup rendering
     */
    struct RenderQueue
    {
        /** Camera/view data for this submission. */
        RenderView view{};

        /** Caller-owned array of visible frame instances. */
        const RenderInstance* instances = nullptr;

        /** Number of entries in @c instances. */
        uint32_t instanceCount = 0;

        /** Number of eligible renderables before visibility culling. */
        uint32_t totalRenderables = 0;

        /** Optional development/editor selection rendering data. */
        RenderDebugSelection debugSelection{};
    };
}
