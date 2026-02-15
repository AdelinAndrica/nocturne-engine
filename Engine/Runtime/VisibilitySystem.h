#pragma once
#include <cstdint>

#include "Core/Math/MathTypes.h"
#include "Render/RenderPacket.h"

namespace noc
{
    class World;
    struct Camera;
    struct LinearArena; // forward declared in Core/Memory

    class VisibilitySystem
    {
    public:
        VisibilitySystem() = default;
        ~VisibilitySystem();

        bool Init();
        void Shutdown();

        // Broadphase maintenance
        void RebuildBroadphase(const World& world); // safe baseline; can be optimized later
        void UpdateBroadphase(const World& world);  // uses transforms + renderables

        // Visibility build (allocates VisibleSet + items from FrameArena)
        VisibleSet BuildVisibleSet(const World& world, const Camera& camera, void* frameArenaMem, uint32_t frameArenaBytes);

        // Design choice: uniform grid parameters
        void SetGridCellSize(float meters);

    private:
        struct Impl;
        Impl* impl_ = nullptr;
    };
}
