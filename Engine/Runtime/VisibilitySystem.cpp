#include "Runtime/VisibilitySystem.h"
#include "Runtime/World.h"
#include "Runtime/Transform.h"

#include "Core/Assert.h"
#include "Core/Log.h"

#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cstring>

namespace noc
{
    // Simple arena wrapper (to avoid including the full engine arena API here).
    // Engine will pass FrameArena memory and size.
    struct TempArena
    {
        uint8_t* base = nullptr;
        uint32_t cap = 0;
        uint32_t off = 0;

        void Init(void* mem, uint32_t bytes)
        {
            base = (uint8_t*)mem;
            cap = bytes;
            off = 0;
        }

        void* Alloc(uint32_t bytes, uint32_t align = 16)
        {
            const uint32_t mask = align - 1;
            uint32_t p = (off + mask) & ~mask;
            if (p + bytes > cap) return nullptr;
            void* r = base + p;
            off = p + bytes;
            return r;
        }
    };

    struct Plane
    {
        math::Vec3 n{};
        float d = 0.0f; // plane equation: dot(n, x) + d >= 0 inside
    };

    static Plane NormalizePlane(const Plane& p)
    {
        const float len = math::Length(p.n);
        if (len <= 0.0f) return p;
        Plane r = p;
        r.n = r.n * (1.0f / len);
        r.d = r.d * (1.0f / len);
        return r;
    }

    static void ExtractFrustumPlanes(const math::Mat4& vp, Plane out[6])
    {
        // Row-major extraction from VP
        // Planes: left, right, bottom, top, near, far
        // Each plane from combinations of rows.
        const float* m = vp.m;

        // row0 = m[0..3], row1 = m[4..7], row2 = m[8..11], row3 = m[12..15]
        auto row = [&](int r, int c) -> float { return m[r * 4 + c]; };

        Plane left;
        left.n = { row(3,0) + row(0,0), row(3,1) + row(0,1), row(3,2) + row(0,2) };
        left.d = row(3, 3) + row(0, 3);

        Plane right;
        right.n = { row(3,0) - row(0,0), row(3,1) - row(0,1), row(3,2) - row(0,2) };
        right.d = row(3, 3) - row(0, 3);

        Plane bottom;
        bottom.n = { row(3,0) + row(1,0), row(3,1) + row(1,1), row(3,2) + row(1,2) };
        bottom.d = row(3, 3) + row(1, 3);

        Plane top;
        top.n = { row(3,0) - row(1,0), row(3,1) - row(1,1), row(3,2) - row(1,2) };
        top.d = row(3, 3) - row(1, 3);

        Plane nearP;
        nearP.n = { row(3,0) + row(2,0), row(3,1) + row(2,1), row(3,2) + row(2,2) };
        nearP.d = row(3, 3) + row(2, 3);

        Plane farP;
        farP.n = { row(3,0) - row(2,0), row(3,1) - row(2,1), row(3,2) - row(2,2) };
        farP.d = row(3, 3) - row(2, 3);

        out[0] = NormalizePlane(left);
        out[1] = NormalizePlane(right);
        out[2] = NormalizePlane(bottom);
        out[3] = NormalizePlane(top);
        out[4] = NormalizePlane(nearP);
        out[5] = NormalizePlane(farP);
    }

    static bool SphereInFrustum(const Plane planes[6], const math::Vec3& c, float r)
    {
        for (int i = 0; i < 6; ++i)
        {
            const float dist = math::Dot(planes[i].n, c) + planes[i].d;
            if (dist < -r) return false;
        }
        return true;
    }

    // ----- Design choice: Uniform grid broadphase -----
    // Justification: simple, stable, fast for horror-scale scenes (rooms/corridors),
    // cheap to update for moving objects, easy to debug.
    struct CellKey
    {
        int32_t x = 0, y = 0, z = 0;
        bool operator==(const CellKey& o) const { return x == o.x && y == o.y && z == o.z; }
    };

    struct CellKeyHash
    {
        size_t operator()(const CellKey& k) const noexcept
        {
            // Mix 3 ints into size_t
            uint32_t hx = (uint32_t)k.x * 73856093u;
            uint32_t hy = (uint32_t)k.y * 19349663u;
            uint32_t hz = (uint32_t)k.z * 83492791u;
            return (size_t)(hx ^ hy ^ hz);
        }
    };

    struct SpatialEntry
    {
        EntityHandle e{};
        math::Vec3 worldCenter{};
        float worldRadius = 1.0f;
        CellKey cell{};
    };

    struct VisibilitySystem::Impl
    {
        float cellSize = 4.0f;

        std::vector<SpatialEntry> entries; // one per entity with renderable+transform (dense list)
        std::unordered_map<CellKey, std::vector<uint32_t>, CellKeyHash> cells; // cell -> indices into entries

        bool initialized = false;

        static CellKey ComputeCell(const math::Vec3& p, float cellSize)
        {
            const float inv = 1.0f / cellSize;
            return {
                (int32_t)std::floor(p.x * inv),
                (int32_t)std::floor(p.y * inv),
                (int32_t)std::floor(p.z * inv)
            };
        }

        void Clear()
        {
            entries.clear();
            cells.clear();
        }

        void Insert(uint32_t entryIndex)
        {
            const CellKey ck = entries[entryIndex].cell;
            cells[ck].push_back(entryIndex);
        }

        void RemoveFromCell(uint32_t entryIndex, const CellKey& ck)
        {
            auto it = cells.find(ck);
            if (it == cells.end()) return;
            auto& v = it->second;
            v.erase(std::remove(v.begin(), v.end(), entryIndex), v.end());
            if (v.empty()) cells.erase(it);
        }

        void Rebuild(const World& world)
        {
            Clear();

            const uint32_t cap = world.EntityCapacity();
            entries.reserve(cap / 2);

            for (uint32_t i = 0; i < cap; ++i)
            {
                EntityHandle e = world.EntityAtIndex(i);
                if (!e.IsValid()) continue;
                if (!world.HasTransform(e) || !world.HasRenderable(e)) continue;

                const auto* t = world.GetTransform(e);
                const auto* r = world.GetRenderable(e);
                if (!t || !r || !r->enabled) continue;

                SpatialEntry se;
                se.e = e;

                // Local sphere -> world: center transforms as point; radius scales by max axis scale
                const math::Vec3 wc = math::TransformPoint(t->worldMatrix, r->localCenter);
                const float sx = t->worldMatrix(0, 0);
                const float sy = t->worldMatrix(1, 1);
                const float sz = t->worldMatrix(2, 2);
                const float maxScale = std::max(std::max(std::fabs(sx), std::fabs(sy)), std::fabs(sz));

                se.worldCenter = wc;
                se.worldRadius = r->localRadius * maxScale;
                se.cell = ComputeCell(wc, cellSize);

                const uint32_t idx = (uint32_t)entries.size();
                entries.push_back(se);
                Insert(idx);
            }
        }

        void Update(const World& world)
        {
            // For Phase 10: refresh all entries (still cheap). Later: only update dirty transforms.
            // Keep entry order stable by sorting by entity index each update.
            Rebuild(world);
            std::sort(entries.begin(), entries.end(), [](const SpatialEntry& a, const SpatialEntry& b)
                {
                    return a.e.index < b.e.index;
                });
            // Need to rebuild cells due to sort:
            cells.clear();
            for (uint32_t i = 0; i < (uint32_t)entries.size(); ++i)
                Insert(i);
        }
    };

    VisibilitySystem::~VisibilitySystem()
    {
        Shutdown();
    }

    bool VisibilitySystem::Init()
    {
        if (impl_) return true;
        impl_ = new Impl();
        impl_->initialized = true;
        NOC_LOG_INFO("Vis", "VisibilitySystem initialized (grid cell=%.2fm)", impl_->cellSize);
        return true;
    }

    void VisibilitySystem::Shutdown()
    {
        if (!impl_) return;
        impl_->Clear();
        delete impl_;
        impl_ = nullptr;
        NOC_LOG_INFO("Vis", "VisibilitySystem shutdown");
    }

    void VisibilitySystem::SetGridCellSize(float meters)
    {
        if (!impl_) return;
        impl_->cellSize = (meters <= 0.01f) ? 0.01f : meters;
    }

    void VisibilitySystem::RebuildBroadphase(const World& world)
    {
        NOC_ASSERT(impl_);
        impl_->Rebuild(world);
    }

    void VisibilitySystem::UpdateBroadphase(const World& world)
    {
        NOC_ASSERT(impl_);
        impl_->Update(world);
    }

    VisibleSet VisibilitySystem::BuildVisibleSet(const World& world, const Camera& camera, void* frameArenaMem, uint32_t frameArenaBytes)
    {
        NOC_ASSERT(impl_);

        TempArena arena;
        arena.Init(frameArenaMem, frameArenaBytes);

        Plane planes[6];
        ExtractFrustumPlanes(camera.ViewProj(), planes);

        // Conservative candidate list: all entries (Phase 10); later we can query grid by frustum AABB.
        // Still uses frustum tests per candidate.
        const uint32_t maxItems = (uint32_t)impl_->entries.size();
        RenderItem* items = (RenderItem*)arena.Alloc(sizeof(RenderItem) * maxItems, 16);
        if (!items)
        {
            NOC_LOG_WARN("Vis", "FrameArena too small for VisibleSet (%u items)", maxItems);
            return {};
        }

        uint32_t count = 0;

        for (const auto& se : impl_->entries)
        {
            if (!world.IsAlive(se.e)) continue;

            const auto* t = world.GetTransform(se.e);
            const auto* r = world.GetRenderable(se.e);
            if (!t || !r || !r->enabled) continue;

            if (!SphereInFrustum(planes, se.worldCenter, se.worldRadius))
                continue;

            RenderItem it;
            it.entity = se.e;
            it.world = t->worldMatrix;
            it.mesh = r->mesh;
            it.shader = r->shader;
            it.worldCenter = se.worldCenter;
            it.worldRadius = se.worldRadius;

            items[count++] = it;
        }

        VisibleSet set;
        set.items = items;
        set.count = count;
        return set;
    }
}
