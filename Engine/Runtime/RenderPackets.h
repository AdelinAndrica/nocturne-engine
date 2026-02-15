#pragma once
#include <cstdint>
#include "Core/Math/Math.h"
#include "Resources/ResourceManager.h"

namespace noc
{
    struct RenderPacket
    {
        // Runtime references assets by handles, never raw pointers.
        ResourceHandle mesh;   // Binary mesh (e.g., "Meshes/triangle.nmsh")
        math::Mat4 world;      // World matrix for this instance
    };

    struct RenderQueue
    {
        const RenderPacket* packets = nullptr;
        uint32_t count = 0;

        // Camera for this frame
        math::Mat4 viewProj = math::Mat4::Identity();

        // Debug toggle: renderer can ignore culling (but culling happens in runtime).
        bool cullingDisabled = false;
    };
}
