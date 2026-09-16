## Phase 10 — Scene Representation

**Objective:** Add a minimal, production-correct **Runtime-owned World** that manages **scene objects + transforms + bounds + camera + frustum culling**, and produces a **renderer-consumable render submission** each frame **without violating dependency rules** (Render never pulls Runtime state).

---

## Key concepts from the books (no page numbers; grounded, but general)

* **World/scene ownership belongs to the runtime layer** (engine orchestrates systems; no global mutable state). (Gregory)
* **Scene representation needs stable object identity** (handles) and deterministic iteration for debugging. (Gregory)
* **Hierarchical transforms** should cache world transforms and use **dirty propagation** rather than recomputing everything every frame. (Gregory, Lengyel)
* **Visibility** is commonly done via **camera frustum culling against bounds** (AABB/sphere), producing a **renderable set** for the renderer. (Lengyel)
* **Renderer consumes immutable per-frame view + draw data** (separation of concerns / SRP-style separation). (Gregory)

Where this phase makes a choice not strictly mandated by the books, it is labeled explicitly as **Design choice (not directly from the book)**.

---

## What we implement now (tight scope)

* `World` under **Engine/Runtime/** with scene object ownership, stable handles, deterministic iteration.
* Transform hierarchy with cached world matrices + dirty propagation.
* Per-object bounds (AABB) and world bounds updates when transforms change.
* Camera + frustum planes + culling toggle.
* Runtime → Render handoff via a POD `RenderQueue` that contains:

  * camera view-projection
  * list of instances: `{ mesh ResourceHandle, world matrix }`
* Render changes: MeshPass becomes “instance-capable” by uploading an instance matrix buffer and drawing **N instances** of `triangle.nmsh`.
* NocturneHost demo scene: multiple objects, parent-child motion, culling on/off.

---

## Implementation steps (deterministic)

1. Add minimal math types: `Vec3`, `Quat`, `Mat4` (column-major, DX-style projection).
2. Add scene data types: `SceneObjectHandle`, `AABB`, `Frustum`, `Camera`.
3. Implement `World`:

   * handle table (index+generation), create/destroy
   * transform storage (arrays), parent/child via firstChild/nextSibling
   * dirty propagation down subtree
   * cached world matrix + world bounds update
   * renderable binding (mesh handle + local bounds)
   * build `RenderQueue` using **FrameArena** (no per-frame heap allocs)
4. Introduce `Engine/Render/RenderQueue.h` (POD handoff).
5. Update engine loop:

   * `Engine::Tick()` advances `World`
   * `Engine::EndFrame()` builds queue + passes to renderer (no Render→Runtime dependency)
6. Update DX12 renderer + MeshPass to consume `RenderQueue`:

   * per-frame instance matrix upload buffer (mapped once; reused)
   * SRV descriptor per frame to that buffer
   * `DrawIndexedInstanced(indexCount, instanceCount, ...)`
7. Update `Basic.hlsl` to use `SV_InstanceID` and matrix buffer.
8. Add NocturneHost test scene.

---

# FULL C++ IMPLEMENTATIONS (every new/modified file)

> Notes:
>
> * Public engine headers remain STL-free.
> * Frame allocations for render submission use `Engine::FrameArena()`.
> * All Render changes preserve: frame indexing model, fences, allocator/list lifetime rules, swap chain ownership, RenderSystem→Dx12Renderer orchestration, SRP separation.

---

## `Engine/Core/Math/MathTypes.h` (NEW)

```cpp
#pragma once
#include <cmath>
#include <cstdint>

namespace noc
{
    // ============================================================
    // Vec3
    // ============================================================

    struct Vec3
    {
        float x{}, y{}, z{};

        constexpr Vec3() = default;
        constexpr Vec3(float X, float Y, float Z) : x(X), y(Y), z(Z) {}

        static constexpr Vec3 Zero() { return { 0,0,0 }; }
        static constexpr Vec3 One() { return { 1,1,1 }; }

        friend constexpr Vec3 operator+(const Vec3& a, const Vec3& b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
        friend constexpr Vec3 operator-(const Vec3& a, const Vec3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
        friend constexpr Vec3 operator*(const Vec3& v, float s) { return { v.x * s, v.y * s, v.z * s }; }
        friend constexpr Vec3 operator*(float s, const Vec3& v) { return v * s; }
    };

    inline float Dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

    inline Vec3 Cross(const Vec3& a, const Vec3& b)
    {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }

    inline float LengthSq(const Vec3& v) { return Dot(v, v); }
    inline float Length(const Vec3& v) { return std::sqrt(LengthSq(v)); }

    inline Vec3 Normalize(const Vec3& v)
    {
        const float len = Length(v);
        if (len <= 1e-6f) return Vec3::Zero();
        return v * (1.0f / len);
    }

    // ============================================================
    // Quat
    // ============================================================

    struct Quat
    {
        float x{}, y{}, z{}, w{ 1.0f };

        constexpr Quat() = default;
        constexpr Quat(float X, float Y, float Z, float W) : x(X), y(Y), z(Z), w(W) {}

        static constexpr Quat Identity() { return { 0,0,0,1 }; }
    };

    // Rotate vector by unit quaternion (no matrices)
    inline Vec3 Rotate(const Quat& q, const Vec3& v)
    {
        Vec3 qv{ q.x, q.y, q.z };
        Vec3 t = Cross(qv, v) * 2.0f;
        return v + t * q.w + Cross(qv, t);
    }

    // ============================================================
    // Mat4 (COLUMN-MAJOR, m[col*4 + row])
    // ============================================================

    struct Mat4
    {
        float m[16]{};

        static Mat4 Identity()
        {
            Mat4 r{};
            r.m[0] = 1.0f;
            r.m[5] = 1.0f;
            r.m[10] = 1.0f;
            r.m[15] = 1.0f;
            return r;
        }
    };

    // Access helper: element at (row, col)
    inline float& M(Mat4& m, int row, int col) { return m.m[col * 4 + row]; }
    inline float  M(const Mat4& m, int row, int col) { return m.m[col * 4 + row]; }

    // Matrix multiply (column-major, column vectors): r = a * b
    inline Mat4 Mul(const Mat4& a, const Mat4& b)
    {
        Mat4 r{};
        for (int c = 0; c < 4; ++c)
        {
            for (int rrow = 0; rrow < 4; ++rrow)
            {
                M(r, rrow, c) =
                    M(a, rrow, 0) * M(b, 0, c) +
                    M(a, rrow, 1) * M(b, 1, c) +
                    M(a, rrow, 2) * M(b, 2, c) +
                    M(a, rrow, 3) * M(b, 3, c);
            }
        }
        return r;
    }

    // Transform point (column vector): p' = M * [p,1]
    inline Vec3 TransformPoint(const Mat4& m, const Vec3& p)
    {
        const float x = M(m, 0, 0) * p.x + M(m, 0, 1) * p.y + M(m, 0, 2) * p.z + M(m, 0, 3) * 1.0f;
        const float y = M(m, 1, 0) * p.x + M(m, 1, 1) * p.y + M(m, 1, 2) * p.z + M(m, 1, 3) * 1.0f;
        const float z = M(m, 2, 0) * p.x + M(m, 2, 1) * p.y + M(m, 2, 2) * p.z + M(m, 2, 3) * 1.0f;
        return { x,y,z };
    }

    inline Mat4 Translation(const Vec3& t)
    {
        Mat4 r = Mat4::Identity();
        r.m[12] = t.x;
        r.m[13] = t.y;
        r.m[14] = t.z;
        return r;
    }

    inline Mat4 Scale(const Vec3& s)
    {
        Mat4 r{};
        r.m[0] = s.x;
        r.m[5] = s.y;
        r.m[10] = s.z;
        r.m[15] = 1.0f;
        return r;
    }

    inline Mat4 RotationFromQuat(const Quat& q)
    {
        const float x = q.x, y = q.y, z = q.z, w = q.w;
        const float xx = x * x, yy = y * y, zz = z * z;
        const float xy = x * y, xz = x * z, yz = y * z;
        const float wx = w * x, wy = w * y, wz = w * z;

        Mat4 r = Mat4::Identity();

        // column-major rotation matrix
        r.m[0] = 1.0f - 2.0f * (yy + zz);
        r.m[1] = 2.0f * (xy + wz);
        r.m[2] = 2.0f * (xz - wy);

        r.m[4] = 2.0f * (xy - wz);
        r.m[5] = 1.0f - 2.0f * (xx + zz);
        r.m[6] = 2.0f * (yz + wx);

        r.m[8] = 2.0f * (xz + wy);
        r.m[9] = 2.0f * (yz - wx);
        r.m[10] = 1.0f - 2.0f * (xx + yy);

        return r;
    }

    inline Mat4 TRS(const Vec3& t, const Quat& r, const Vec3& s)
    {
        // Column-vector convention: M = T * R * S
        return Mul(Translation(t), Mul(RotationFromQuat(r), Scale(s)));
    }

    // ============================================================
    // Camera matrices
    // ============================================================

    inline Mat4 LookToLH(const Vec3& eye, const Vec3& dir, const Vec3& up)
    {
        const Vec3 zaxis = Normalize(dir);
        const Vec3 xaxis = Normalize(Cross(up, zaxis));
        const Vec3 yaxis = Cross(zaxis, xaxis);

        Mat4 r = Mat4::Identity();

        // basis vectors into columns
        r.m[0] = xaxis.x; r.m[1] = xaxis.y; r.m[2] = xaxis.z;
        r.m[4] = yaxis.x; r.m[5] = yaxis.y; r.m[6] = yaxis.z;
        r.m[8] = zaxis.x; r.m[9] = zaxis.y; r.m[10] = zaxis.z;

        // translation
        r.m[12] = -Dot(xaxis, eye);
        r.m[13] = -Dot(yaxis, eye);
        r.m[14] = -Dot(zaxis, eye);

        return r;
    }

    // D3D-style LH perspective, depth 0..1
    inline Mat4 PerspectiveFovLH(float fovY, float aspect, float zn, float zf)
    {
        Mat4 r{};
        const float yScale = 1.0f / std::tan(fovY * 0.5f);
        const float xScale = yScale / aspect;

        r.m[0] = xScale;
        r.m[5] = yScale;
        r.m[10] = zf / (zf - zn);
        r.m[11] = 1.0f;
        r.m[14] = (-zn * zf) / (zf - zn);
        return r;
    }
}
```

---

## `Engine/Runtime/Bounds.h` (NEW)

```cpp
#pragma once
#include "Core/Math/MathTypes.h"

namespace noc
{
	struct AABB
	{
		Vec3 min;
		Vec3 max;
	};

	inline AABB AabbInvalid()
	{
		return AABB{ Vec3(1e30f, 1e30f, 1e30f), Vec3(-1e30f,-1e30f,-1e30f) };
	}

	inline void AabbExpand(AABB& a, const Vec3& p)
	{
		if (p.x < a.min.x) a.min.x = p.x;
		if (p.y < a.min.y) a.min.y = p.y;
		if (p.z < a.min.z) a.min.z = p.z;
		if (p.x > a.max.x) a.max.x = p.x;
		if (p.y > a.max.y) a.max.y = p.y;
		if (p.z > a.max.z) a.max.z = p.z;
	}

	inline AABB TransformAabb(const AABB& local, const Mat4& world)
	{
		// Conservative: transform all 8 corners.
		const Vec3 c[8] = {
			{local.min.x, local.min.y, local.min.z},
			{local.max.x, local.min.y, local.min.z},
			{local.min.x, local.max.y, local.min.z},
			{local.max.x, local.max.y, local.min.z},
			{local.min.x, local.min.y, local.max.z},
			{local.max.x, local.min.y, local.max.z},
			{local.min.x, local.max.y, local.max.z},
			{local.max.x, local.max.y, local.max.z},
		};

		AABB out = AabbInvalid();
		for (int i = 0; i < 8; ++i)
			AabbExpand(out, TransformPoint(world, c[i]));
		return out;
	}
}
```

---

## `Engine/Runtime/Frustum.h` (NEW)

```cpp
#pragma once
#include "Core/Math/MathTypes.h"
#include "Runtime/Bounds.h"
#include <cmath>

namespace noc
{
	struct Plane
	{
		// ax + by + cz + d >= 0 is inside
		float a = 0, b = 0, c = 0, d = 0;
	};

	struct Frustum
	{
		// 0..5: left,right,bottom,top,near,far
		Plane p[6]{};
	};

	inline void NormalizePlane(Plane& pl)
	{
		const float len = std::sqrtf(pl.a * pl.a + pl.b * pl.b + pl.c * pl.c);
		if (len > 1e-6f)
		{
			const float inv = 1.0f / len;
			pl.a *= inv; pl.b *= inv; pl.c *= inv; pl.d *= inv;
		}
	}

	inline Frustum FrustumFromViewProj(const Mat4& m)
	{
		// Mat4 is stored column-major in m.m[col*4 + row]
		auto at = [&](int row, int col) -> float { return m.m[col * 4 + row]; };

		Frustum f{};

		// Left   = row3 + row0
		f.p[0] = Plane{ at(3,0) + at(0,0), at(3,1) + at(0,1), at(3,2) + at(0,2), at(3,3) + at(0,3) };
		// Right  = row3 - row0
		f.p[1] = Plane{ at(3,0) - at(0,0), at(3,1) - at(0,1), at(3,2) - at(0,2), at(3,3) - at(0,3) };
		// Bottom = row3 + row1
		f.p[2] = Plane{ at(3,0) + at(1,0), at(3,1) + at(1,1), at(3,2) + at(1,2), at(3,3) + at(1,3) };
		// Top    = row3 - row1
		f.p[3] = Plane{ at(3,0) - at(1,0), at(3,1) - at(1,1), at(3,2) - at(1,2), at(3,3) - at(1,3) };

		// D3D depth 0..1:
		// Near = row2
		f.p[4] = Plane{ at(2,0), at(2,1), at(2,2), at(2,3) };
		// Far  = row3 - row2
		f.p[5] = Plane{ at(3,0) - at(2,0), at(3,1) - at(2,1), at(3,2) - at(2,2), at(3,3) - at(2,3) };

		for (int i = 0; i < 6; ++i) NormalizePlane(f.p[i]);
		return f;
	}




	inline bool AabbInsidePlane(const AABB& a, const Plane& p)
	{
		// Positive vertex test
		Vec3 v;
		v.x = (p.a >= 0) ? a.max.x : a.min.x;
		v.y = (p.b >= 0) ? a.max.y : a.min.y;
		v.z = (p.c >= 0) ? a.max.z : a.min.z;

		const float dist = p.a * v.x + p.b * v.y + p.c * v.z + p.d;
		return dist >= 0.0f;
	}

	inline bool AabbIntersectsFrustum(const AABB& a, const Frustum& f)
	{
		for (int i = 0; i < 6; ++i)
		{
			if (!AabbInsidePlane(a, f.p[i]))
				return false;
		}
		return true;
	}
}
```

---

## `Engine/Runtime/SceneObject.h` (NEW)

```cpp
#pragma once
#include <cstdint>

namespace noc
{
	struct SceneObjectHandle
	{
		uint32_t index = 0xFFFFFFFFu;
		uint32_t generation = 0;

		bool IsValid() const { return index != 0xFFFFFFFFu; }
	};

	inline bool operator==(const SceneObjectHandle& a, const SceneObjectHandle& b)
	{
		return a.index == b.index && a.generation == b.generation;
	}
}
```

---

## `Engine/Runtime/Camera.h` (NEW)

```cpp
#pragma once
#include "Core/Math/MathTypes.h"

namespace noc
{
	struct Camera
	{
		float fovYRadians = 1.04719755f; // ~60 deg
		float aspect = 16.0f / 9.0f;
		float nearZ = 0.1f;
		float farZ = 500.0f;

		Mat4 view = Mat4::Identity();
		Mat4 proj = Mat4::Identity();
		Mat4 viewProj = Mat4::Identity();

		void Rebuild(const Vec3& eye, const Vec3& forward, const Vec3& up)
		{
			view = LookToLH(eye, forward, up);
			proj = PerspectiveFovLH(fovYRadians, aspect, nearZ, farZ);
			viewProj = Mul(proj, view); // column-major, column vectors

		}
	};
}

```

---

## `Engine/Render/RenderQueue.h` (NEW)

```cpp
#pragma once
#include <cstdint>

#include "Core/Math/MathTypes.h"
#include "Resources/ResourceHandle.h"

namespace noc
{
	struct RenderView
	{
		Mat4 viewProj;
		uint32_t viewportWidth = 0;
		uint32_t viewportHeight = 0;
	};

	struct RenderInstance
	{
		ResourceHandle mesh; // ResourceManager handle (NOT raw pointer)
		Mat4 world;
	};

	// POD render submission for a single frame.
	// Memory for instances is owned by the caller (FrameArena).
	struct RenderQueue
	{
		RenderView view{};
		const RenderInstance* instances = nullptr;
		uint32_t instanceCount = 0;
		uint32_t totalRenderables = 0;
	};
}
```

---

## `Engine/Runtime/World.h` (NEW)

```cpp
#pragma once
#include <cstdint>

#include "Runtime/SceneObject.h"
#include "Runtime/Bounds.h"
#include "Runtime/Camera.h"
#include "Runtime/Frustum.h"

#include "Core/Math/MathTypes.h"

#include "Resources/ResourceHandle.h"

namespace noc
{
	class IAllocator;
	class LinearArena;

	struct RenderQueue;

	struct WorldStats
	{
		uint32_t visible = 0;
		uint32_t total = 0;
	};

	class World
	{
	public:
		World() = default;

		bool Init(IAllocator& persistentAlloc);
		void Shutdown();

		// Frame update (transform propagation + bounds)
		void Update();

		// --- Object model ---
		SceneObjectHandle CreateObject();
		void DestroyObject(SceneObjectHandle h);

		// Deterministic iteration order for debugging: indices are stable in creation order
		uint32_t AliveCount() const;

		// --- Transform ---
		void SetLocalTRS(SceneObjectHandle h, const Vec3& t, const Quat& r, const Vec3& s);
		void SetParent(SceneObjectHandle child, SceneObjectHandle parent);
		Mat4 GetWorldMatrix(SceneObjectHandle h);

		// --- Renderable binding ---
		// localBounds is in object-local space.
		void SetRenderable(SceneObjectHandle h, ResourceHandle mesh, const AABB& localBounds);

		// --- Camera ---
		// Minimal: one active camera stored in the World (can be extended later).
		void SetCameraParams(float fovYRadians, float aspect, float nearZ, float farZ);
		void SetCameraFromObject(SceneObjectHandle h); // camera follows this object's transform
		void SetCullingEnabled(bool enabled) { cullingEnabled_ = enabled; }
		bool IsCullingEnabled() const { return cullingEnabled_; }
		void SetCullingMaxDistance(float meters); // 0 = disabled

		// --- Runtime → Render handoff (allocates from FrameArena) ---
		// Returns a RenderQueue whose instance array is allocated from frameArena.
		RenderQueue BuildRenderQueue(LinearArena& frameArena, uint32_t viewportW, uint32_t viewportH);



		const WorldStats& GetLastStats() const;
		void DebugRequestCullDump();

	private:
		struct Impl;
		Impl* impl_ = nullptr;
		float cullingMaxDistance_ = 0.0f; // 0 = disabled (Design choice)
		WorldStats lastStats_;

		bool cullingEnabled_ = true;
		bool debugCullDump_ = false;
	};
}
```

---

## `Engine/Runtime/World.cpp` (NEW)

```cpp
#include "World.h"

#include "Core/Log.h"
#include "Core/Memory/Allocator.h"
#include "Core/Memory/LinearArena.h"

#include "Render/RenderQueue.h"
#include "Core/Math/MathTypes.h"
#include <cstring>

namespace noc
{
	static constexpr uint32_t kInvalidIndex = 0xFFFFFFFFu;


	struct TransformData
	{
		Vec3 localT = Vec3::Zero();
		Quat localR = Quat::Identity();
		Vec3 localS = Vec3::One();

		uint32_t parent = kInvalidIndex;
		uint32_t firstChild = kInvalidIndex;
		uint32_t nextSibling = kInvalidIndex;

		Mat4 world = Mat4::Identity();
		uint8_t dirty = 1;
	};

	struct RenderableData
	{
		uint8_t has = 0;
		ResourceHandle mesh{};
		AABB localBounds{};
		AABB worldBounds{};
	};

	struct World::Impl
	{
		IAllocator* alloc = nullptr;

		// Handle table
		uint32_t capacity = 0;
		uint32_t countAlive = 0;

		uint32_t* generations = nullptr;
		uint8_t* alive = nullptr;

		// deterministic creation order (indices)
		uint32_t* order = nullptr;
		uint32_t orderCount = 0;
		uint32_t orderCap = 0;

		// free list (stack)
		uint32_t* freeList = nullptr;
		uint32_t freeCount = 0;
		uint32_t freeCap = 0;

		TransformData* xform = nullptr;
		RenderableData* rend = nullptr;

		// camera follows an object (optional)
		uint32_t cameraFollowIndex = kInvalidIndex;
		Camera camera{};
		Frustum frustum{};

		bool EnsureCapacity(uint32_t newCap)
		{
			if (newCap <= capacity) return true;

			// grow to power-of-two-ish
			uint32_t target = (capacity == 0) ? 64u : capacity;
			while (target < newCap) target *= 2;

			auto reallocArr = [&](void*& ptr, size_t elemSize, size_t oldCount, size_t newCount) -> bool
				{
					void* n = alloc->Allocate(elemSize * newCount, 64);
					if (!n) return false;
					if (ptr && oldCount)
						memcpy(n, ptr, elemSize * oldCount);
					if (ptr)
						alloc->Deallocate(ptr);
					ptr = n;
					return true;
				};

			const uint32_t oldCap = capacity;
			if (!reallocArr((void*&)generations, sizeof(uint32_t), oldCap, target)) return false;
			if (!reallocArr((void*&)alive, sizeof(uint8_t), oldCap, target)) return false;
			if (!reallocArr((void*&)xform, sizeof(TransformData), oldCap, target)) return false;
			if (!reallocArr((void*&)rend, sizeof(RenderableData), oldCap, target)) return false;

			// init new slots
			for (uint32_t i = oldCap; i < target; ++i)
			{
				generations[i] = 1;
				alive[i] = 0;
				xform[i] = TransformData{};
				rend[i] = RenderableData{};
			}

			capacity = target;
			return true;
		}

		void PushOrder(uint32_t idx)
		{
			if (orderCount == orderCap)
			{
				const uint32_t newCap = (orderCap == 0) ? 64u : orderCap * 2;
				void* n = alloc->Allocate(sizeof(uint32_t) * newCap, 64);
				if (order && orderCount)
					memcpy(n, order, sizeof(uint32_t) * orderCount);
				if (order) alloc->Deallocate(order);
				order = (uint32_t*)n;
				orderCap = newCap;
			}
			order[orderCount++] = idx;
		}

		void PushFree(uint32_t idx)
		{
			if (freeCount == freeCap)
			{
				const uint32_t newCap = (freeCap == 0) ? 64u : freeCap * 2;
				void* n = alloc->Allocate(sizeof(uint32_t) * newCap, 64);
				if (freeList && freeCount)
					memcpy(n, freeList, sizeof(uint32_t) * freeCount);
				if (freeList) alloc->Deallocate(freeList);
				freeList = (uint32_t*)n;
				freeCap = newCap;
			}
			freeList[freeCount++] = idx;
		}

		bool IsAliveHandle(SceneObjectHandle h) const
		{
			if (!h.IsValid() || h.index >= capacity) return false;
			return alive[h.index] != 0 && generations[h.index] == h.generation;
		}

		void MarkDirtySubtree(uint32_t idx)
		{
			// No heap allocations: DFS using sibling pointers.
			xform[idx].dirty = 1;
			for (uint32_t c = xform[idx].firstChild; c != kInvalidIndex; c = xform[c].nextSibling)
				MarkDirtySubtree(c);
		}

		void DetachFromParent(uint32_t child)
		{
			const uint32_t p = xform[child].parent;
			if (p == kInvalidIndex) return;

			uint32_t* link = &xform[p].firstChild;
			while (*link != kInvalidIndex)
			{
				if (*link == child)
				{
					*link = xform[child].nextSibling;
					break;
				}
				link = &xform[*link].nextSibling;
			}

			xform[child].parent = kInvalidIndex;
			xform[child].nextSibling = kInvalidIndex;
		}

		void AttachToParent(uint32_t child, uint32_t parent)
		{
			xform[child].parent = parent;
			xform[child].nextSibling = xform[parent].firstChild;
			xform[parent].firstChild = child;
		}

		void UpdateWorldRecursive(uint32_t idx)
		{
			TransformData& t = xform[idx];

			Mat4 local = TRS(t.localT, t.localR, t.localS);
			if (t.parent != kInvalidIndex)
				t.world = Mul(xform[t.parent].world, local);
			else
				t.world = local;

			// bounds update (if renderable)
			if (rend[idx].has)
				rend[idx].worldBounds = TransformAabb(rend[idx].localBounds, t.world);

			t.dirty = 0;

			for (uint32_t c = t.firstChild; c != kInvalidIndex; c = xform[c].nextSibling)
			{
				if (xform[c].dirty)
					UpdateWorldRecursive(c);
				else
				{
					// parent changed implies child should have been marked; keep strict:
					// we still recompute if parent just recomputed, to avoid stale data.
					UpdateWorldRecursive(c);
				}
			}
		}

		void UpdateAllDirty()
		{
			// For determinism: traverse creation order.
			for (uint32_t i = 0; i < orderCount; ++i)
			{
				const uint32_t idx = order[i];
				if (idx >= capacity || alive[idx] == 0) continue;
				if (xform[idx].dirty)
					UpdateWorldRecursive(idx);
			}
		}

		void RebuildCamera(uint32_t viewportW, uint32_t viewportH)
		{
			if (viewportW == 0 || viewportH == 0)
				return;

			camera.aspect = (float)viewportW / (float)viewportH;

			Vec3 eye = Vec3(0, 0, -5);
			Quat rot = Quat::Identity();

			if (cameraFollowIndex != kInvalidIndex && cameraFollowIndex < capacity && alive[cameraFollowIndex])
			{
				const TransformData& tf = xform[cameraFollowIndex];
				eye = tf.localT; // camera object local position is used; world already handled by tf.world if parented
				// Prefer world position if parented:
				eye = Vec3(tf.world.m[12], tf.world.m[13], tf.world.m[14]);
				rot = tf.localR;
			}

			const Vec3 forward = Rotate(rot, Vec3(0, 0, 1)); // LH forward
			const Vec3 up = Rotate(rot, Vec3(0, 1, 0));

			camera.Rebuild(eye, forward, up);
			frustum = FrustumFromViewProj(camera.viewProj);
		}
	};


	bool World::Init(IAllocator& persistentAlloc)
	{
		if (impl_) return true;

		impl_ = (Impl*)persistentAlloc.Allocate(sizeof(Impl), 64);
		if (!impl_) return false;
		memset(impl_, 0, sizeof(Impl));
		impl_->alloc = &persistentAlloc;

		impl_->camera.proj = PerspectiveFovLH(impl_->camera.fovYRadians, impl_->camera.aspect, impl_->camera.nearZ, impl_->camera.farZ);

		NOC_LOG_INFO("World", "World initialized");
		return true;
	}

	void World::Shutdown()
	{
		if (!impl_) return;

		IAllocator* a = impl_->alloc;

		if (impl_->generations) a->Deallocate(impl_->generations);
		if (impl_->alive) a->Deallocate(impl_->alive);
		if (impl_->xform) a->Deallocate(impl_->xform);
		if (impl_->rend) a->Deallocate(impl_->rend);
		if (impl_->order) a->Deallocate(impl_->order);
		if (impl_->freeList) a->Deallocate(impl_->freeList);

		a->Deallocate(impl_);
		impl_ = nullptr;

		NOC_LOG_INFO("World", "World shutdown");
	}

	void World::Update()
	{
		if (!impl_) return;
		impl_->UpdateAllDirty();
	}

	SceneObjectHandle World::CreateObject()
	{
		if (!impl_) return {};

		uint32_t idx = kInvalidIndex;

		if (impl_->freeCount > 0)
		{
			idx = impl_->freeList[--impl_->freeCount];
		}
		else
		{
			idx = impl_->capacity;
			if (!impl_->EnsureCapacity(idx + 1))
				return {};
		}

		impl_->alive[idx] = 1;
		impl_->countAlive++;

		impl_->xform[idx] = TransformData{};
		impl_->rend[idx] = RenderableData{};

		impl_->PushOrder(idx);

		SceneObjectHandle h;
		h.index = idx;
		h.generation = impl_->generations[idx];
		return h;
	}

	void World::DestroyObject(SceneObjectHandle h)
	{
		if (!impl_ || !impl_->IsAliveHandle(h)) return;

		const uint32_t idx = h.index;

		// detach children (promote to roots)
		uint32_t child = impl_->xform[idx].firstChild;
		while (child != kInvalidIndex)
		{
			uint32_t next = impl_->xform[child].nextSibling;
			impl_->xform[child].parent = kInvalidIndex;
			impl_->xform[child].nextSibling = kInvalidIndex;
			child = next;
		}
		impl_->xform[idx].firstChild = kInvalidIndex;

		// detach from parent
		impl_->DetachFromParent(idx);

		// invalidate camera follow if needed
		if (impl_->cameraFollowIndex == idx)
			impl_->cameraFollowIndex = kInvalidIndex;

		impl_->alive[idx] = 0;
		impl_->countAlive--;

		impl_->generations[idx]++; // bump generation
		impl_->PushFree(idx);
	}

	uint32_t World::AliveCount() const
	{
		return impl_ ? impl_->countAlive : 0;
	}

	void World::SetLocalTRS(SceneObjectHandle h, const Vec3& t, const Quat& r, const Vec3& s)
	{
		if (!impl_ || !impl_->IsAliveHandle(h)) return;

		TransformData& tf = impl_->xform[h.index];
		tf.localT = t;
		tf.localR = r;
		tf.localS = s;

		impl_->MarkDirtySubtree(h.index);
	}

	void World::SetParent(SceneObjectHandle child, SceneObjectHandle parent)
	{
		if (!impl_) return;
		if (!impl_->IsAliveHandle(child)) return;

		const uint32_t c = child.index;
		const uint32_t p = (impl_->IsAliveHandle(parent)) ? parent.index : kInvalidIndex;

		if (impl_->xform[c].parent == p)
			return;

		impl_->DetachFromParent(c);
		if (p != kInvalidIndex)
			impl_->AttachToParent(c, p);

		impl_->MarkDirtySubtree(c);
	}

	Mat4 World::GetWorldMatrix(SceneObjectHandle h)
	{
		if (!impl_ || !impl_->IsAliveHandle(h)) return Mat4::Identity();

		if (impl_->xform[h.index].dirty)
			impl_->UpdateWorldRecursive(h.index);

		return impl_->xform[h.index].world;
	}

	void World::SetRenderable(SceneObjectHandle h, ResourceHandle mesh, const AABB& localBounds)
	{
		if (!impl_ || !impl_->IsAliveHandle(h)) return;

		RenderableData& rd = impl_->rend[h.index];
		rd.has = 1;
		rd.mesh = mesh;
		rd.localBounds = localBounds;

		// force bounds update
		impl_->MarkDirtySubtree(h.index);
	}

	void World::SetCameraParams(float fovYRadians, float aspect, float nearZ, float farZ)
	{
		if (!impl_) return;
		impl_->camera.fovYRadians = fovYRadians;
		impl_->camera.aspect = aspect;
		impl_->camera.nearZ = nearZ;
		impl_->camera.farZ = farZ;
	}

	void World::SetCameraFromObject(SceneObjectHandle h)
	{
		if (!impl_ || !impl_->IsAliveHandle(h)) return;
		impl_->cameraFollowIndex = h.index;
	}

	void World::SetCullingMaxDistance(float meters)
	{
		cullingMaxDistance_ = (meters < 0.0f) ? 0.0f : meters;
	}


	RenderQueue World::BuildRenderQueue(LinearArena& frameArena, uint32_t viewportW, uint32_t viewportH)
	{
		RenderQueue q{};

		if (!impl_)
			return q;

		// Ensure transforms/bounds are up to date.
		impl_->UpdateAllDirty();
		impl_->RebuildCamera(viewportW, viewportH);

		q.view.viewProj = impl_->camera.viewProj;
		q.view.viewportWidth = viewportW;
		q.view.viewportHeight = viewportH;

		// Count visible instances first (deterministic order).
		uint32_t visible = 0;
		uint32_t total = 0;
		for (uint32_t i = 0; i < impl_->orderCount; ++i)
		{
			const uint32_t idx = impl_->order[i];
			if (idx >= impl_->capacity || impl_->alive[idx] == 0) continue;
			if (!impl_->rend[idx].has) continue;
			total++;
			if (!cullingEnabled_)
			{
				visible++;
				continue;
			}
			if (debugCullDump_)
			{
				const auto& wb = impl_->rend[idx].worldBounds;

				const bool hit = AabbIntersectsFrustum(wb, impl_->frustum);

				NOC_LOG_INFO("World",
					"CullDump idx=%u hit=%s bounds min(%.2f %.2f %.2f) max(%.2f %.2f %.2f)",
					idx,
					hit ? "YES" : "NO",
					wb.min.x, wb.min.y, wb.min.z,
					wb.max.x, wb.max.y, wb.max.z);
			}



			if (AabbIntersectsFrustum(impl_->rend[idx].worldBounds, impl_->frustum))
				visible++;
		}

		if (visible == 0)
		{
			lastStats_.visible = 0;
			lastStats_.total = total;

			if (debugCullDump_)
			{
				NOC_LOG_INFO("World",
					"CullDump summary: visible=%u total=%u (culling=%s)",
					lastStats_.visible, lastStats_.total, cullingEnabled_ ? "ON" : "OFF");

				debugCullDump_ = false; // one-shot
			}


			return q;
		}

		void* mem = frameArena.Allocate(sizeof(RenderInstance) * visible, 16);



		if (!mem)
			return q;

		RenderInstance* out = (RenderInstance*)mem;

		uint32_t w = 0;
		for (uint32_t i = 0; i < impl_->orderCount; ++i)
		{
			const uint32_t idx = impl_->order[i];
			if (idx >= impl_->capacity || impl_->alive[idx] == 0) continue;
			if (!impl_->rend[idx].has) continue;

			bool keep = true;
			if (cullingEnabled_)
				keep = AabbIntersectsFrustum(impl_->rend[idx].worldBounds, impl_->frustum);

			if (!keep) continue;

			out[w].mesh = impl_->rend[idx].mesh;
			out[w].world = impl_->xform[idx].world;
			w++;
		}

		lastStats_.visible = w;
		lastStats_.total = total;

		if (debugCullDump_)
		{
			NOC_LOG_INFO("World",
				"CullDump summary: visible=%u total=%u (culling=%s)",
				lastStats_.visible, lastStats_.total, cullingEnabled_ ? "ON" : "OFF");

			debugCullDump_ = false; // one-shot
		}


		q.instances = out;
		q.instanceCount = w;
		return q;
	}

	const WorldStats& World::GetLastStats() const
	{
		return lastStats_;
	}

	void World::DebugRequestCullDump()
	{
		debugCullDump_ = true;
	}
}
```

---

## `Engine/Render/RenderSystem.h` (MODIFIED)

```cpp
#pragma once
#include <cstdint>

namespace noc
{
	class ResourceManager;
	struct RenderQueue;

	class RenderSystem
	{
	public:
		RenderSystem() = default;

		bool Init(bool enableDebugLayer);
		void Shutdown();

		bool AttachToWindow(void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight);

		void SetResourceManager(ResourceManager* rm);

		// Frame lifecycle (unchanged semantics)
		void BeginFrame();
		void EndFramePresent();

		// Runtime → Render handoff for this frame (no ownership taken).
		void SetFrameRenderQueue(const RenderQueue* q);

	private:
		struct Impl;
		Impl* impl_ = nullptr;
	};
}
```

---

## `Engine/Render/RenderSystem.cpp` (MODIFIED)

```cpp
#include "RenderSystem.h"

#include "Core/Log.h"
#include "Render/DX12/Dx12Renderer.h"

namespace noc
{
	struct RenderSystem::Impl
	{
		Dx12Renderer renderer;
	};

	bool RenderSystem::Init(bool enableDebugLayer)
	{
		if (impl_)
			return true;

		impl_ = new Impl();
		if (!impl_->renderer.Init(enableDebugLayer))
		{
			NOC_LOG_ERROR("Render", "Dx12Renderer::Init failed");
			delete impl_;
			impl_ = nullptr;
			return false;
		}

		NOC_LOG_INFO("Render", "RenderSystem initialized");
		return true;
	}

	void RenderSystem::Shutdown()
	{
		if (!impl_)
			return;

		impl_->renderer.Shutdown();

		delete impl_;
		impl_ = nullptr;

		NOC_LOG_INFO("Render", "RenderSystem shutdown");
	}

	bool RenderSystem::AttachToWindow(void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight)
	{
		if (!impl_)
			return false;

		return impl_->renderer.AttachToWindow(nativeHwnd, clientWidth, clientHeight);
	}

	void RenderSystem::SetResourceManager(ResourceManager* rm)
	{
		if (!impl_) return;
		impl_->renderer.SetResourceManager(rm);
	}

	void RenderSystem::SetFrameRenderQueue(const RenderQueue* q)
	{
		if (!impl_) return;
		impl_->renderer.SetFrameRenderQueue(q);
	}

	void RenderSystem::BeginFrame()
	{
		if (!impl_)
			return;

		impl_->renderer.BeginFrame();
	}

	void RenderSystem::EndFramePresent()
	{
		if (!impl_)
			return;

		impl_->renderer.EndFramePresent();
	}
}
```

---

## `Engine/Render/DX12/Dx12Renderer.h` (MODIFIED)

```cpp
#pragma once
#include "Dx12Common.h"

#include "Dx12Device.h"
#include "Dx12SwapChain.h"
#include "Dx12FrameSync.h"

#include "Dx12DescriptorAllocator.h"
#include "Dx12DeferredReleaseQueue.h"
#include "Dx12PsoCache.h"
#include "MeshPass.h"

namespace noc
{
	class ResourceManager;
	struct RenderQueue;

	class Dx12Renderer
	{
	public:
		bool Init(bool enableDebugLayer);
		void Shutdown();

		bool AttachToWindow(void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight);

		void SetResourceManager(ResourceManager* rm) { rm_ = rm; }
		void SetFrameRenderQueue(const RenderQueue* q) { frameQueue_ = q; }

		void BeginFrame();
		void EndFramePresent();

	private:
		void LogDeviceRemoved_(const char* where);

	private:
		bool inited_ = false;
		bool attached_ = false;

		ResourceManager* rm_ = nullptr;
		const RenderQueue* frameQueue_ = nullptr; // points to FrameArena memory

		Dx12Device device_;
		Dx12SwapChain swap_;
		Dx12FrameSync sync_;

		dx12::ComPtr<ID3D12CommandAllocator> cmdAlloc_[dx12::kFrameCount];
		dx12::ComPtr<ID3D12GraphicsCommandList> cmdList_;

		uint32_t frameIndex_ = 0;
		bool frameOpen_ = false;

		Dx12DescriptorAllocator cbvSrvUavHeap_;
		Dx12DescriptorAllocator samplerHeap_;
		Dx12DeferredReleaseQueue deferred_;
		Dx12PsoCache psoCache_;

		MeshPass meshPass_;
	};
}
```

---

## `Engine/Render/DX12/Dx12Renderer.cpp` (MODIFIED)

```cpp
#include "Dx12Renderer.h"

#include "Resources/ResourceManager.h"
#include "Render/RenderQueue.h"

namespace noc
{
	bool Dx12Renderer::Init(bool enableDebugLayer)
	{
		if (inited_)
			return true;

		if (!device_.Init(enableDebugLayer))
			return false;

		if (!sync_.Init(device_.Device()))
			return false;

		inited_ = true;
		NOC_LOG_INFO("Render", "Dx12Renderer initialized (awaiting AttachToWindow)");
		return true;
	}

	void Dx12Renderer::Shutdown()
	{
		if (!inited_)
			return;

		if (attached_)
			sync_.WaitForGpu(device_.Queue());

		deferred_.Clear();

		meshPass_.Shutdown(deferred_, sync_.CompletedValue());

		cmdList_.Reset();
		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
			cmdAlloc_[i].Reset();

		psoCache_.Clear();

		cbvSrvUavHeap_.Shutdown();
		samplerHeap_.Shutdown();

		swap_.Shutdown();
		sync_.Shutdown();
		device_.Shutdown();

		inited_ = false;
		attached_ = false;
		frameQueue_ = nullptr;

		NOC_LOG_INFO("Render", "Dx12Renderer shutdown");
	}

	bool Dx12Renderer::AttachToWindow(void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight)
	{
		if (!inited_)
			return false;

		if (!swap_.Init(device_.Factory(), device_.Queue(), nativeHwnd, clientWidth, clientHeight))
			return false;

		if (!swap_.CreateRtvHeapAndViews(device_.Device()))
			return false;

		frameIndex_ = swap_.FrameIndex();

		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
		{
			if (!dx12::HrOk(device_.Device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc_[i])),
				"CreateCommandAllocator"))
			{
				return false;
			}
		}

		if (!dx12::HrOk(device_.Device()->CreateCommandList(
			0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc_[frameIndex_].Get(), nullptr, IID_PPV_ARGS(&cmdList_)),
			"CreateCommandList"))
		{
			return false;
		}
		dx12::HrOk(cmdList_->Close(), "cmdList->Close (initial)");

		if (!cbvSrvUavHeap_.Init(device_.Device(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024, true))
			return false;
		if (!samplerHeap_.Init(device_.Device(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 64, true))
			return false;

		if (!meshPass_.Init(device_.Device(), cbvSrvUavHeap_, samplerHeap_, psoCache_))
			return false;

		attached_ = true;
		NOC_LOG_INFO("Render", "Dx12Renderer attached (%ux%u)", clientWidth, clientHeight);
		return true;
	}

	void Dx12Renderer::BeginFrame()
	{
		if (!attached_)
			return;

		if (device_.Device()->GetDeviceRemovedReason() != S_OK)
			return;

		if (frameOpen_)
		{
			NOC_LOG_ERROR("Render", "BeginFrame called while frame is already open");
			return;
		}
		frameOpen_ = true;

		deferred_.Collect(sync_.CompletedValue());

		if (FAILED(cmdAlloc_[frameIndex_]->Reset()))
		{
			LogDeviceRemoved_("cmdAlloc->Reset");
			frameOpen_ = false;
			return;
		}

		if (FAILED(cmdList_->Reset(cmdAlloc_[frameIndex_].Get(), nullptr)))
		{
			LogDeviceRemoved_("cmdList->Reset");
			frameOpen_ = false;
			return;
		}

		swap_.TransitionTo(cmdList_.Get(), frameIndex_, D3D12_RESOURCE_STATE_RENDER_TARGET);
	}

	void Dx12Renderer::EndFramePresent()
	{
		if (!attached_)
			return;

		if (device_.Device()->GetDeviceRemovedReason() != S_OK)
			return;

		if (!frameOpen_)
		{
			NOC_LOG_ERROR("Render", "EndFramePresent called without BeginFrame");
			return;
		}
		struct Guard { bool& b; ~Guard() { b = false; } } g{ frameOpen_ };

		// Record pass: clear + draw instances from frameQueue_ (if any are ready).
		meshPass_.Record(device_.Device(), cmdList_.Get(), swap_, sync_, frameIndex_, deferred_, rm_, frameQueue_);

		swap_.TransitionTo(cmdList_.Get(), frameIndex_, D3D12_RESOURCE_STATE_PRESENT);

		if (FAILED(cmdList_->Close()))
		{
			LogDeviceRemoved_("cmdList->Close");
			return;
		}

		ID3D12CommandList* lists[] = { cmdList_.Get() };
		device_.Queue()->ExecuteCommandLists(1, lists);

		swap_.Present();

		sync_.MoveToNextFrame(device_.Queue(), swap_.SwapChain(), frameIndex_);
		swap_.UpdateFrameIndex(frameIndex_);
	}

	void Dx12Renderer::LogDeviceRemoved_(const char* where)
	{
		HRESULT hr = device_.Device()->GetDeviceRemovedReason();
		NOC_LOG_ERROR("Render", "Device removed at %s (hr=0x%08X)", where, (unsigned)hr);
	}
}
```

---

## `Engine/Render/DX12/MeshPass.h` (MODIFIED)

```cpp
#pragma once
#include "Dx12Common.h"

#include "Dx12DescriptorAllocator.h"
#include "Dx12DeferredReleaseQueue.h"
#include "Dx12PsoCache.h"
#include "GpuBuffer.h"
#include "GpuRingConstantBuffer.h"
#include "MeshFormat.h"

#include "Resources/ResourceHandle.h"
#include "Resources/Typed/ResourceHandleT.h"
#include "Resources/Typed/TextResource.h"

namespace noc
{
	class ResourceManager;
	class Dx12SwapChain;
	class Dx12FrameSync;
	struct RenderQueue;

	class MeshPass
	{
	public:
		bool Init(
			ID3D12Device* device,
			Dx12DescriptorAllocator& cbvSrvUav,
			Dx12DescriptorAllocator& samplers,
			Dx12PsoCache& psoCache);

		void Shutdown(Dx12DeferredReleaseQueue& deferred, uint64_t safeFenceValue);

		void Record(
			ID3D12Device* device,
			ID3D12GraphicsCommandList* cmd,
			Dx12SwapChain& swap,
			const Dx12FrameSync& sync,
			uint32_t frameIndex,
			Dx12DeferredReleaseQueue& deferred,
			ResourceManager* rm,
			const RenderQueue* queue);

	private:
		bool EnsureRootSigAndPso_(ID3D12Device* device, Dx12PsoCache& cache, ResourceManager* rm);
		bool EnsureMeshUploaded_(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, Dx12DeferredReleaseQueue& deferred,
			const Dx12FrameSync& sync, uint32_t frameIndex, ResourceManager* rm);

		void EnsurePerFrameCbv_(ID3D12Device* device);
		void EnsurePerFrameInstanceSrv_(ID3D12Device* device);

	private:
		ResourceHandleT<TextResource> shaderHlsl_;

		// GPU objects
		dx12::ComPtr<ID3D12RootSignature> rootSig_;
		dx12::ComPtr<ID3D12PipelineState> pso_;

		// For Phase 10 demo: one mesh upload path (triangle.nmsh), but drawn N times.
		ResourceHandle meshBin_{};
		GpuBuffer vb_;
		GpuBuffer ib_;
		uint32_t indexCount_ = 0;

		// Per-frame constants
		GpuRingConstantBuffer perFrameCB_;
		Dx12DescriptorAllocator* cbvSrvUav_ = nullptr;
		Dx12DescriptorHandle perFrameCbv_[dx12::kFrameCount]{};

		// Per-frame instance matrices in an upload buffer (mapped once), exposed as SRV t0.
		dx12::ComPtr<ID3D12Resource> instanceBuf_[dx12::kFrameCount];
		uint8_t* instanceMapped_[dx12::kFrameCount]{};
		uint32_t instanceCapacity_ = 0;
		Dx12DescriptorHandle instanceSrv_[dx12::kFrameCount]{};

		Dx12PsoCache* psoCache_ = nullptr;

		bool rootReady_ = false;
		bool psoReady_ = false;
		bool meshReady_ = false;
		bool cbReady_ = false;
		bool instReady_ = false;
	};
}
```

---

## `Engine/Render/DX12/MeshPass.cpp` (MODIFIED)

```cpp
#include "MeshPass.h"

#include "Dx12SwapChain.h"
#include "Dx12FrameSync.h"
#include "ShaderCompiler.h"

#include "Resources/ResourceManager.h"
#include "Render/RenderQueue.h"

namespace noc
{
	// Matches Basic.hlsl
	struct PerFrameConstants
	{
		Mat4 viewProj;
	};

	static uint64_t HashInputLayoutPC_()
	{
		return 0xA0C0CA11u; // stable constant for Phase 9.5/10 demo
	}

	bool MeshPass::Init(
		ID3D12Device* device,
		Dx12DescriptorAllocator& cbvSrvUav,
		Dx12DescriptorAllocator& samplers,
		Dx12PsoCache& psoCache)
	{
		(void)samplers;

		if (!device)
			return false;

		cbvSrvUav_ = &cbvSrvUav;
		psoCache_ = &psoCache;

		rootReady_ = false;
		psoReady_ = false;
		meshReady_ = false;
		cbReady_ = false;
		instReady_ = false;

		// Per-frame constant buffers
		if (!perFrameCB_.Init(device, 64 * 1024))
			return false;

		// Instance buffer capacity (Design choice): enough for a small test scene.
		instanceCapacity_ = 1024;

		return true;
	}

	void MeshPass::Shutdown(Dx12DeferredReleaseQueue& deferred, uint64_t safeFenceValue)
	{
		if (pso_)
		{
			dx12::ComPtr<IUnknown> u;
			pso_.As(&u);
			deferred.Enqueue(safeFenceValue, std::move(u));
			pso_.Reset();
		}
		if (rootSig_)
		{
			dx12::ComPtr<IUnknown> u;
			rootSig_.As(&u);
			deferred.Enqueue(safeFenceValue, std::move(u));
			rootSig_.Reset();
		}

		vb_.ShutdownNow();
		ib_.ShutdownNow();

		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
		{
			if (instanceBuf_[i] && instanceMapped_[i])
				instanceBuf_[i]->Unmap(0, nullptr);
			instanceMapped_[i] = nullptr;
			instanceBuf_[i].Reset();
		}

		perFrameCB_.Shutdown();
	}

	void MeshPass::EnsurePerFrameCbv_(ID3D12Device* device)
	{
		if (cbReady_ || !device || !cbvSrvUav_)
			return;

		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
		{
			perFrameCbv_[i] = cbvSrvUav_->Allocate();

			D3D12_CONSTANT_BUFFER_VIEW_DESC d{};
			d.BufferLocation = perFrameCB_.Resource(i)->GetGPUVirtualAddress();
			d.SizeInBytes = (UINT)((sizeof(PerFrameConstants) + 255u) & ~255u);

			device->CreateConstantBufferView(&d, perFrameCbv_[i].cpu);
		}

		cbReady_ = true;
	}

	void MeshPass::EnsurePerFrameInstanceSrv_(ID3D12Device* device)
	{
		if (instReady_ || !device || !cbvSrvUav_)
			return;

		// Upload heap, persistently mapped.
		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
		{
			instanceSrv_[i] = cbvSrvUav_->Allocate();

			const UINT64 bytes = (UINT64)instanceCapacity_ * sizeof(Mat4);

			D3D12_HEAP_PROPERTIES hp{};
			hp.Type = D3D12_HEAP_TYPE_UPLOAD;

			D3D12_RESOURCE_DESC d = dx12::BufferDesc(bytes);

			if (!dx12::HrOk(device->CreateCommittedResource(
				&hp,
				D3D12_HEAP_FLAG_NONE,
				&d,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&instanceBuf_[i])), "CreateCommittedResource(InstanceUpload)"))
			{
				return;
			}

			void* mapped = nullptr;
			D3D12_RANGE r{ 0, 0 };
			if (!dx12::HrOk(instanceBuf_[i]->Map(0, &r, &mapped), "InstanceUpload.Map"))
				return;

			instanceMapped_[i] = (uint8_t*)mapped;

			D3D12_SHADER_RESOURCE_VIEW_DESC sd{};
			sd.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			sd.Buffer.FirstElement = 0;
			sd.Buffer.NumElements = instanceCapacity_;
			sd.Buffer.StructureByteStride = sizeof(Mat4);
			sd.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			sd.Format = DXGI_FORMAT_UNKNOWN;

			device->CreateShaderResourceView(instanceBuf_[i].Get(), &sd, instanceSrv_[i].cpu);
		}

		instReady_ = true;
	}

	bool MeshPass::EnsureRootSigAndPso_(ID3D12Device* device, Dx12PsoCache& cache, ResourceManager* rm)
	{
		if (!device || !rm)
			return false;

		if (!shaderHlsl_.IsValid())
			shaderHlsl_ = rm->RequestText("Shaders/Basic.hlsl");

		if (!rootReady_)
		{
			// Root parameters:
			// 0: CBV table (b0) per-frame
			// 1: SRV table (t0..t7) (t0 used for instance matrices)
			// 2: Sampler table (s0..s7) (reserved)
			D3D12_DESCRIPTOR_RANGE ranges[3]{};

			ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
			ranges[0].NumDescriptors = 1;
			ranges[0].BaseShaderRegister = 0;
			ranges[0].RegisterSpace = 0;
			ranges[0].OffsetInDescriptorsFromTableStart = 0;

			ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			ranges[1].NumDescriptors = 8;
			ranges[1].BaseShaderRegister = 0;
			ranges[1].RegisterSpace = 0;
			ranges[1].OffsetInDescriptorsFromTableStart = 0;

			ranges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
			ranges[2].NumDescriptors = 8;
			ranges[2].BaseShaderRegister = 0;
			ranges[2].RegisterSpace = 0;
			ranges[2].OffsetInDescriptorsFromTableStart = 0;

			D3D12_ROOT_PARAMETER params[3]{};

			params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			params[0].DescriptorTable.NumDescriptorRanges = 1;
			params[0].DescriptorTable.pDescriptorRanges = &ranges[0];
			params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

			params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			params[1].DescriptorTable.NumDescriptorRanges = 1;
			params[1].DescriptorTable.pDescriptorRanges = &ranges[1];
			params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

			params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			params[2].DescriptorTable.NumDescriptorRanges = 1;
			params[2].DescriptorTable.pDescriptorRanges = &ranges[2];
			params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

			D3D12_ROOT_SIGNATURE_DESC rs{};
			rs.NumParameters = 3;
			rs.pParameters = params;
			rs.NumStaticSamplers = 0;
			rs.pStaticSamplers = nullptr;
			rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

			dx12::ComPtr<ID3DBlob> blob;
			dx12::ComPtr<ID3DBlob> err;
			if (!dx12::HrOk(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err), "SerializeRootSignature"))
			{
				const char* e = err ? (const char*)err->GetBufferPointer() : "unknown";
				NOC_LOG_ERROR("Render", "RootSig serialize error: %s", e);
				return false;
			}

			if (!dx12::HrOk(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSig_)), "CreateRootSignature"))
				return false;

			rootReady_ = true;
		}

		const TextResource* src = rm->GetText(shaderHlsl_);
		if (!src)
			return false;

		dx12::ComPtr<ID3DBlob> vs;
		dx12::ComPtr<ID3DBlob> ps;

		if (!ShaderCompiler::CompileFromMemory("Shaders/Basic.hlsl", src->Str().c_str(), src->Str().size(), "VSMain", "vs_5_1", vs))
			return false;
		if (!ShaderCompiler::CompileFromMemory("Shaders/Basic.hlsl", src->Str().c_str(), src->Str().size(), "PSMain", "ps_5_1", ps))
			return false;

		Dx12PsoKey key{};
		key.vs = vs.Get();
		key.ps = ps.Get();
		key.rootSig = rootSig_.Get();
		key.rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		key.inputLayoutHash = HashInputLayoutPC_();

		if (auto* cached = cache.Find(key))
		{
			pso_ = cached;
			psoReady_ = true;
			return true;
		}

		D3D12_INPUT_ELEMENT_DESC layout[2]{};
		layout[0].SemanticName = "POSITION";
		layout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		layout[0].InputSlot = 0;
		layout[0].AlignedByteOffset = 0;
		layout[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

		layout[1].SemanticName = "COLOR";
		layout[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		layout[1].InputSlot = 0;
		layout[1].AlignedByteOffset = 12;
		layout[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
		pso.pRootSignature = rootSig_.Get();
		pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
		pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
		pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		pso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		pso.DepthStencilState.DepthEnable = FALSE;
		pso.DepthStencilState.StencilEnable = FALSE;
		pso.SampleMask = UINT_MAX;
		pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pso.NumRenderTargets = 1;
		pso.RTVFormats[0] = key.rtvFormat;
		pso.SampleDesc.Count = 1;
		pso.InputLayout = { layout, 2 };

		dx12::ComPtr<ID3D12PipelineState> created;
		if (!dx12::HrOk(device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&created)), "CreateGraphicsPipelineState"))
			return false;

		pso_ = created;
		cache.Insert(key, std::move(created));
		psoReady_ = true;
		return true;
	}

	bool MeshPass::EnsureMeshUploaded_(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmd,
		Dx12DeferredReleaseQueue& deferred,
		const Dx12FrameSync& sync,
		uint32_t frameIndex,
		ResourceManager* rm)
	{
		if (meshReady_)
			return true;

		if (!meshBin_.IsValid())
			meshBin_ = rm->RequestBinary("Meshes/triangle.nmsh");

		if (!rm->IsReady(meshBin_))
			return false;

		const uint8_t* bytes = rm->GetBytes(meshBin_);
		const size_t size = rm->GetSize(meshBin_);
		if (!bytes || size == 0)
			return false;

		CpuMeshPC cpu{};
		const char* err = nullptr;
		if (!ParseNocMeshPC(bytes, size, cpu, err))
		{
			NOC_LOG_ERROR("Render", "Mesh parse failed: %s", err ? err : "unknown");
			return false;
		}

		indexCount_ = (uint32_t)cpu.indices.size();

		if (!vb_.CreateStatic(device, cmd, deferred, sync, frameIndex, GpuBuffer::Kind::Vertex,
			cpu.vertices.data(), cpu.vertices.size() * sizeof(MeshVertexPC), sizeof(MeshVertexPC)))
			return false;

		if (!ib_.CreateStatic(device, cmd, deferred, sync, frameIndex, GpuBuffer::Kind::Index,
			cpu.indices.data(), cpu.indices.size() * sizeof(uint16_t), 0))
			return false;

		meshReady_ = true;
		return true;
	}

	void MeshPass::Record(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmd,
		Dx12SwapChain& swap,
		const Dx12FrameSync& sync,
		uint32_t frameIndex,
		Dx12DeferredReleaseQueue& deferred,
		ResourceManager* rm,
		const RenderQueue* queue)
	{
		if (!device || !cmd)
			return;

		EnsurePerFrameCbv_(device);
		EnsurePerFrameInstanceSrv_(device);

		auto rtv = swap.CurrentRtv(frameIndex);
		cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

		const float clearColor[4] = { 0.05f, 0.05f, 0.08f, 1.0f };
		cmd->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

		if (!rm || !queue || queue->instanceCount == 0)
			return;

		if (!EnsureRootSigAndPso_(device, *psoCache_, rm))
			return;

		if (!EnsureMeshUploaded_(device, cmd, deferred, sync, frameIndex, rm))
			return;

		// Per-frame constants: viewProj
		perFrameCB_.BeginFrame(frameIndex);
		D3D12_GPU_VIRTUAL_ADDRESS gpu = 0;
		void* cpu = nullptr;
		if (perFrameCB_.Allocate(sizeof(PerFrameConstants), gpu, cpu))
		{
			auto* c = (PerFrameConstants*)cpu;
			c->viewProj = queue->view.viewProj;
		}

		// Upload instance matrices for this frame (clamp to capacity).
		const uint32_t count = (queue->instanceCount > instanceCapacity_) ? instanceCapacity_ : queue->instanceCount;

		// Correct: write matrices one-by-one because RenderInstance contains a mesh handle before the matrix.
		Mat4* dst = reinterpret_cast<Mat4*>(instanceMapped_[frameIndex]);
		for (uint32_t i = 0; i < count; ++i)
		{
			dst[i] = queue->instances[i].world;
		}


		// Bind descriptor heaps (shader-visible).
		ID3D12DescriptorHeap* heaps[] = { cbvSrvUav_->Heap() };
		cmd->SetDescriptorHeaps(1, heaps);

		cmd->SetGraphicsRootSignature(rootSig_.Get());
		cmd->SetPipelineState(pso_.Get());

		// Root slot 0: per-frame CBV table (b0)
		cmd->SetGraphicsRootDescriptorTable(0, perFrameCbv_[frameIndex].gpu);

		// Root slot 1: SRV table (t0..), we use t0 = instance matrices
		cmd->SetGraphicsRootDescriptorTable(1, instanceSrv_[frameIndex].gpu);

		D3D12_VIEWPORT vp{};
		vp.Width = (float)swap.Width();
		vp.Height = (float)swap.Height();
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;

		D3D12_RECT sc{};
		sc.left = 0;
		sc.top = 0;
		sc.right = (LONG)swap.Width();
		sc.bottom = (LONG)swap.Height();

		cmd->RSSetViewports(1, &vp);
		cmd->RSSetScissorRects(1, &sc);

		auto vbv = vb_.VertexView();
		auto ibv = ib_.IndexView(DXGI_FORMAT_R16_UINT);

		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmd->IASetVertexBuffers(0, 1, &vbv);
		cmd->IASetIndexBuffer(&ibv);

		cmd->DrawIndexedInstanced(indexCount_, count, 0, 0, 0);
	}
}
```

---

## `Engine/Runtime/Engine.h` (MODIFIED: owns World)

```cpp
#pragma once
#include "Core/BuildConfig.h"
#include "Core/Subsystems/SubsystemRegistry.h"
#include "Core/Memory/Allocator.h"
#include "Core/Memory/LinearArena.h"

#if NOC_ENABLE_ASSERTS
#include "Core/Memory/DebugAlloc.h"
#endif

#include "Resources/VirtualFileSystem.h"
#include "Resources/ResourceManager.h"
#include "EngineConfig.h"
#include "Core/Jobs/JobSystem.h"
#include "Input/InputSystem.h"
#include "Render/RenderSystem.h"

#include "Runtime/World.h"
#include <Platform/Win32/WinWindow.h>

namespace noc {

	class WinWindow;
	class MainLoop;

	class Engine
	{
	public:
		EngineConfig& ConfigMutable();
		const EngineConfig& Config() const { return cfg_; }

		bool SetContentRoot(const char* path);
		bool SetOverrideRoot(const char* path);
		bool SetArchivePath(const char* path);

		bool Init();
		void TickOnce();
		void Shutdown();

		int Run();
		void BeginFrame();
		void Tick();
		void EndFrame();

		IAllocator& Allocator();
		LinearArena& FrameArena();

		VirtualFileSystem& VFS() { return vfs_; }
		ResourceManager& Resources() { return resources_; }
		const ResourceManager& Resources() const { return resources_; }

		InputSystem& Input() { return input_; }
		const InputSystem& Input() const { return input_; }

		JobSystem& Jobs() { return jobs_; }
		const JobSystem& Jobs() const { return jobs_; }

		World& GetWorld() { return world_; }
		const World& GetWorld() const { return world_; }

		bool InitMemory();
		void KillMemory();

		bool AttachWindow(WinWindow& window);
		bool CreateAndAttachMainWindow(WinWindowDesc desc, WinWindow& outWindow);


	private:
		bool IsConfigMutable() const { return !initialized_; }

	private:
		SubsystemRegistry registry_;

		MallocAllocator baseAlloc_;

#if NOC_ENABLE_ASSERTS
		DebugAlloc debugAlloc_{ baseAlloc_ };
		IAllocator* alloc_ = &debugAlloc_;
#else
		IAllocator* alloc_ = &baseAlloc_;
#endif

		void* frameArenaMem_ = nullptr;
		LinearArena frameArena_;

		VirtualFileSystem vfs_;
		JobSystem jobs_;
		ResourceManager resources_;
		InputSystem input_;

		RenderSystem render_;
		World world_;

		EngineConfig cfg_{};
		bool initialized_ = false;
	};

} // namespace noc
```

---

## `Engine/Runtime/Engine.cpp` (MODIFIED: tick world + render queue)

```cpp
#include "Engine.h"

#include <algorithm>
#include <span>

#include "Core/Assert.h"
#include "Core/Log.h"
#include "Core/Clock.h"

#include "Platform/Win32/WinWindow.h"
#include "Runtime/MainLoop.h"

#include "Render/RenderQueue.h"

namespace noc {

    IAllocator& Engine::Allocator() { return *alloc_; }
    LinearArena& Engine::FrameArena() { return frameArena_; }

    EngineConfig& Engine::ConfigMutable()
    {
        if (initialized_)
        {
            NOC_LOG_ERROR("Runtime", "EngineConfig is frozen after Init(). Modify config before calling Init().");
            return cfg_;
        }
        return cfg_;
    }

    bool Engine::SetContentRoot(const char* path)
    {
        if (!IsConfigMutable())
        {
            NOC_LOG_ERROR("Runtime", "SetContentRoot() called after Init(); ignored.");
            return false;
        }
        cfg_.contentRoot = path;
        return true;
    }

    bool Engine::SetOverrideRoot(const char* path)
    {
        if (!IsConfigMutable())
        {
            NOC_LOG_ERROR("Runtime", "SetOverrideRoot() called after Init(); ignored.");
            return false;
        }
        cfg_.overrideRoot = path;
        return true;
    }

    bool Engine::SetArchivePath(const char* path)
    {
        if (!IsConfigMutable())
        {
            NOC_LOG_ERROR("Runtime", "SetArchivePath() called after Init(); ignored.");
            return false;
        }
        cfg_.archivePath = path;
        return true;
    }

    bool Engine::InitMemory()
    {
        constexpr std::size_t kFrameArenaBytes = 8 * 1024 * 1024; // Design choice
        frameArenaMem_ = Allocator().Allocate(kFrameArenaBytes, 64);
        frameArena_.Init(frameArenaMem_, kFrameArenaBytes);

        NOC_LOG_INFO("Core", "Memory system initialized (frame arena=%zu bytes)", kFrameArenaBytes);
        return true;
    }

    void Engine::KillMemory()
    {
        if (frameArenaMem_)
        {
            Allocator().Deallocate(frameArenaMem_);
            frameArenaMem_ = nullptr;
        }

#if NOC_ENABLE_ASSERTS
        NOC_LOG_INFO("Core", "Memory stats: total=%zu outstanding=%zu allocs=%zu",
            debugAlloc_.TotalAllocatedBytes(),
            debugAlloc_.OutstandingBytes(),
            debugAlloc_.AllocationCount());
#endif
    }

    static bool StartupLog(void*)
    {
        noc::GetLogger().Init();
        NOC_LOG_INFO("Core", "Logger initialized");
        return true;
    }

    static void ShutdownLog(void*)
    {
        NOC_LOG_INFO("Core", "Logger shutting down");
        noc::GetLogger().Shutdown();
    }

    static bool StartupTime(void*)
    {
        noc::GetTime().Init();
        NOC_LOG_INFO("Core", "Time system initialized");
        return true;
    }

    static void ShutdownTime(void*)
    {
        NOC_LOG_INFO("Core", "Time system shutting down");
    }

    static bool StartupAssert(void*)
    {
        NOC_LOG_INFO("Core", "Assert system initialized");
        return true;
    }

    static void ShutdownAssert(void*)
    {
        NOC_LOG_INFO("Core", "Assert system shutting down");
    }

    static bool StartupMemory(void* ctx)
    {
        auto* e = static_cast<noc::Engine*>(ctx);
        return e->InitMemory();
    }

    static void ShutdownMemory(void* ctx)
    {
        NOC_LOG_INFO("Core", "Memory system shutting down");
        auto* e = static_cast<noc::Engine*>(ctx);
        e->KillMemory();
    }

    static bool StartupWindow(void* ctx)
    {
        auto* e = static_cast<Engine*>(ctx);
        (void)e;
        NOC_LOG_INFO("Win32", "Window subsystem ready");
        return true;
    }

    static void ShutdownWindow(void*)
    {
        NOC_LOG_INFO("Win32", "Window subsystem shutdown");
    }

    static bool StartupJobs(void* ctx)
    {
        auto* e = static_cast<noc::Engine*>(ctx);
        // Design choice: worker count = HW threads - 1 (leave room for main thread), clamped.
        const uint32_t hw = (std::max)(1u, std::thread::hardware_concurrency());
        const uint32_t workers = (hw > 1) ? (hw - 1) : 1;
        return e->Jobs().Init(workers);
    }

    static void ShutdownJobs(void* ctx)
    {
        auto* e = static_cast<noc::Engine*>(ctx);
        e->Jobs().Shutdown();
    }

    bool Engine::Init()
    {
        std::span<const char* const> depsLog{};

        static const char* kDepsNeedLog[] = { "Log" };
        std::span<const char* const> depsNeedLog{ kDepsNeedLog, 1 };

        static const char* kDepsJobs[] = { "Log", "Memory" };
        std::span<const char* const> depsJobs{ kDepsJobs, 2 };

        registry_.Register(SubsystemDesc{ "Log",    depsLog,     &StartupLog,    &ShutdownLog });
        registry_.Register(SubsystemDesc{ "Time",   depsNeedLog, &StartupTime,   &ShutdownTime });
        registry_.Register(SubsystemDesc{ "Memory", depsNeedLog, &StartupMemory, &ShutdownMemory });
        registry_.Register(SubsystemDesc{ "Assert", depsNeedLog, &StartupAssert, &ShutdownAssert });
        registry_.Register(SubsystemDesc{ "Window", depsNeedLog, &StartupWindow, &ShutdownWindow });
        registry_.Register(SubsystemDesc{ "Jobs",   depsJobs,    &StartupJobs,   &ShutdownJobs });

        if (!registry_.StartupAll(this))
            return false;

        // Phase 3 policy: later mounts override earlier mounts.
        if (cfg_.archivePath && cfg_.archivePath[0] != 0)
        {
            if (!vfs_.MountArchive(cfg_.archivePath))
                NOC_LOG_WARN("VFS", "Failed to mount archivePath: %s", cfg_.archivePath);
        }

        if (cfg_.contentRoot && cfg_.contentRoot[0] != 0)
        {
            if (!vfs_.MountLooseDirectory(cfg_.contentRoot))
                NOC_LOG_WARN("VFS", "Failed to mount contentRoot: %s", cfg_.contentRoot);
        }

        if (cfg_.overrideRoot && cfg_.overrideRoot[0] != 0)
        {
            if (!vfs_.MountLooseDirectory(cfg_.overrideRoot))
                NOC_LOG_WARN("VFS", "Failed to mount overrideRoot: %s", cfg_.overrideRoot);
        }

        // Phase 6: ResourceManager uses JobSystem (must be after jobs + VFS mounts).
        if (!resources_.Init(*this, vfs_))
            return false;

		// Phase 7: InputSystem init (HWND comes later in AttachWindow).
		if (!input_.Init(*this))
			return false;

        // Phase 8: RenderSystem init (no HWND yet)
#if NOC_ENABLE_ASSERTS
		const bool enableDebugLayer = true;
#else
		const bool enableDebugLayer = false;
#endif

		if (!render_.Init(enableDebugLayer))
			return false;

        render_.SetResourceManager(&resources_);

        // World is runtime-owned
        if (!world_.Init(Allocator()))
            return false;

        initialized_ = true;
        return true;
    }

    bool Engine::AttachWindow(WinWindow& window)
    {
        // Forward WM_INPUT + focus loss -> InputSystem.
        window.SetRawInputSink(input_.RawSink());

        // Register Raw Input devices (requires HWND).
        if (!input_.AttachToWindow(window.Handle()))
            return false;

        // Phase 8: DX12 attach (requires HWND + client size).
        if (!render_.AttachToWindow(window.Handle(), window.ClientWidth(), window.ClientHeight()))
            return false;

        return true;
    }

    bool Engine::CreateAndAttachMainWindow(WinWindowDesc desc, WinWindow& outWindow)
    {
        if (!outWindow.Create(desc))
            return false;

        if (!AttachWindow(outWindow))
            return false;

        return true;
    }


    int Engine::Run()
    {
        WinWindow window;
        WinWindowDesc wd{};
        wd.title = L"NocturneHost";
        wd.width = 1280;
        wd.height = 720;
        wd.resizable = true;

        if (!window.Create(wd))
        {
            NOC_LOG_FATAL("Win32", "Failed to create window");
            return -1;
        }

        if (!AttachWindow(window))
        {
            NOC_LOG_FATAL("Runtime", "Failed to attach InputSystem to window");
            window.Destroy();
            return -1;
		}

        MainLoop loop;
        loop.Run(*this, window);

        window.Destroy();
        return 0;
    }

    void Engine::BeginFrame()
    {
        GetTime().BeginFrame();
        FrameArena().Reset();
		input_.BeginFrame();
		render_.BeginFrame();
    }

    void Engine::Tick()
    {
		input_.Update();
        resources_.Update();
        world_.Update();
    }

    void Engine::EndFrame()
    {
        // Phase 10: runtime builds render submission in FrameArena and hands it to Render.
        // This keeps Render from touching Runtime state.
        // Viewport size is owned by the swapchain; in this phase we mirror host window size.
        // (Design choice) If you already expose swapchain size, wire it here instead.
        const uint32_t viewportW = 1280;
        const uint32_t viewportH = 720;

        RenderQueue rq = world_.BuildRenderQueue(FrameArena(), viewportW, viewportH);
        render_.SetFrameRenderQueue(&rq);
        render_.EndFramePresent();
        GetTime().EndFrame();
    }

    void Engine::TickOnce()
    {
        GetTime().BeginFrame();
        FrameArena().Reset();

        void* a = FrameArena().Allocate(256, 16);
        void* b = FrameArena().Allocate(1024, 64);
        (void)a; (void)b;

        GetTime().EndFrame();

        NOC_LOG_INFO("Core", "TickOnce() dt=%.6f sec arenaUsed=%zu bytes",
            GetTime().DeltaSeconds(),
            FrameArena().Used());
    }

    void Engine::Shutdown()
    {
		render_.Shutdown();
        world_.Shutdown();
        // Resource manager must shutdown while jobs + memory + log still exist.
        resources_.Shutdown();
		input_.Shutdown();
        registry_.ShutdownAll(this);
    }

} // namespace noc
```

---

## `Apps/NocturneHost/main.cpp` (MODIFIED: test scene)

```cpp
#include "Runtime/Engine.h"
#include "Platform/Win32/WinWindow.h"
#include "Runtime/MainLoop.h" // not used anymore, but ok if included elsewhere
#include "Core/Log.h"
#include "Core/Clock.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cmath>

#ifndef NOC_CONTENT_ROOT
#define NOC_CONTENT_ROOT "."
#endif

// -------------------------------
// Phase 10 Host-side test state
// -------------------------------
static noc::SceneObjectHandle gParent;
static noc::SceneObjectHandle gCam;
static bool gCulling = true;

static noc::SceneObjectHandle gCullingProbe;
static float gProbeX = 0.0f;


static void BuildPhase10TestScene(noc::Engine& engine)
{
	auto& w = engine.GetWorld();

	// Camera object
	gCam = w.CreateObject();
	w.SetLocalTRS(gCam, noc::Vec3(0, 0, -5), noc::Quat::Identity(), noc::Vec3::One());
	w.SetCameraFromObject(gCam);

	// IMPORTANT: keep farZ large so GPU can still draw objects we "uncull"
	w.SetCameraParams(1.04719755f, 16.0f / 9.0f, 0.1f, 200.0f);

	// Triangle mesh handle
	noc::ResourceHandle tri = engine.Resources().RequestBinary("Meshes/triangle.nmsh");

	// Conservative bounds
	noc::AABB triBounds{ noc::Vec3(-1, -1, -1), noc::Vec3(1, 1, 1) };

	// Parent
	gParent = w.CreateObject();
	w.SetLocalTRS(gParent, noc::Vec3(-3.0f, 0.0f, 5.0f), noc::Quat::Identity(), noc::Vec3::One());
	//w.SetRenderable(gParent, tri, triBounds);

	gCullingProbe = w.CreateObject();
	w.SetLocalTRS(gCullingProbe, noc::Vec3(0.0f, 0.0f, 20.0f), noc::Quat::Identity(), noc::Vec3::One());
	w.SetRenderable(gCullingProbe, tri, triBounds);


	// Children follow parent (visible baseline)
	for (int i = 0; i < 5; ++i)
	{
		noc::SceneObjectHandle child = w.CreateObject();
		w.SetParent(child, gParent);
		w.SetLocalTRS(child, noc::Vec3((float)i * 1.5f, 0.0f, 0.0f), noc::Quat::Identity(), noc::Vec3::One());
		w.SetRenderable(child, tri, triBounds);
	}

	// ---- CULLING DEMO OBJECTS ----
	// These are inside far plane (z=30) so GPU can draw them.
	// They are far on X so the frustum *should* reject them when culling is enabled.

	// Off-right (should be culled when ON, visible when OFF)
	{
		noc::SceneObjectHandle offRight = w.CreateObject();
		w.SetLocalTRS(offRight, noc::Vec3(60.0f, 0.0f, 30.0f), noc::Quat::Identity(), noc::Vec3::One());
		w.SetRenderable(offRight, tri, triBounds);
	}

	// Off-left (should be culled when ON, visible when OFF)
	{
		noc::SceneObjectHandle offLeft = w.CreateObject();
		w.SetLocalTRS(offLeft, noc::Vec3(-60.0f, 0.0f, 30.0f), noc::Quat::Identity(), noc::Vec3::One());
		w.SetRenderable(offLeft, tri, triBounds);
	}

	// Slightly above (tests top plane)
	{
		noc::SceneObjectHandle offUp = w.CreateObject();
		w.SetLocalTRS(offUp, noc::Vec3(0.0f, 40.0f, 30.0f), noc::Quat::Identity(), noc::Vec3::One());
		w.SetRenderable(offUp, tri, triBounds);
	}

	gCulling = true;
	w.SetCullingEnabled(gCulling);

	NOC_LOG_INFO("Host",
		"Phase 10 test scene built. Expected: culling ON shows only the hierarchy; culling OFF increases draw count.");
}



static void UpdatePhase10TestScene(noc::Engine& engine)
{
	// Animate parent so hierarchy is obvious
	static float t = 0.0f;
	t += (float)noc::GetTime().DeltaSeconds();

	const float x = std::sinf(t) * 3.0f;
	engine.GetWorld().SetLocalTRS(gParent, noc::Vec3(x, 0, 5), noc::Quat::Identity(), noc::Vec3::One());

	gProbeX += (float)noc::GetTime().DeltaSeconds() * 20.0f; // move right
	if (gProbeX > 80.0f) gProbeX = -80.0f;
	engine.GetWorld().SetLocalTRS(gCullingProbe, noc::Vec3(gProbeX, 0.0f, 20.0f), noc::Quat::Identity(), noc::Vec3::One());

	// Toggle culling with C key (host-side debug input)
	static bool prevC = false;
	const bool nowC = (GetAsyncKeyState('C') & 0x8000) != 0;
	if (nowC && !prevC)
	{
		gCulling = !gCulling;
		engine.GetWorld().SetCullingEnabled(gCulling);

		// request one-shot debug dump next frame
		engine.GetWorld().DebugRequestCullDump();

		NOC_LOG_INFO("Host", "Culling toggled: %s", gCulling ? "ON" : "OFF");
	}

	prevC = nowC;

	// Exit with ESC (host-side)
	if ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0)
	{
		PostQuitMessage(0);
	}
}

int main()
{
	noc::Engine engine;
	engine.SetContentRoot(NOC_CONTENT_ROOT);

	if (!engine.Init())
		return -1;

	noc::WinWindow window;
	noc::WinWindowDesc wd{};
	wd.title = L"NocturneHost - Phase 10 Test";
	wd.width = 1280;
	wd.height = 720;
	wd.resizable = true;

	if (!engine.CreateAndAttachMainWindow(wd, window))
	{
		engine.Shutdown();
		return -1;
	}

	BuildPhase10TestScene(engine);
	NOC_LOG_INFO("Host", "Controls: C = toggle culling, ESC = quit");

	// message pump
	MSG msg{};
	bool running = true;
	while (running)
	{
		while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT) { running = false; break; }
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
		if (!running) break;

		engine.BeginFrame();
		UpdatePhase10TestScene(engine);
		engine.Tick();
		engine.EndFrame();
		static uint32_t sFrame = 0;
		if ((sFrame++ % 30) == 0)
		{
			const auto& st = engine.GetWorld().GetLastStats();
			NOC_LOG_INFO("Host", "Visible: %u / Total: %u (Culling: %s)",
				st.visible, st.total, gCulling ? "ON" : "OFF");
		}


	}

	window.Destroy();
	engine.Shutdown();
	return 0;
}
```

---

## `Data/Shaders/Basic.hlsl` (MODIFIED: instancing + viewProj + matrix buffer)

```hlsl
struct VSIn
{
    float3 pos : POSITION;
    float4 color : COLOR;
};

struct VSOut
{
    float4 pos : SV_Position;
    float4 color : COLOR;
};

// b0: per-frame
cbuffer PerFrame : register(b0)
{
    float4x4 gViewProj;
};

// t0: per-instance world matrices
StructuredBuffer<float4x4> gWorld : register(t0);

VSOut VSMain(VSIn input, uint instanceId : SV_InstanceID)
{
    VSOut o;

    float4 wpos = mul(gWorld[instanceId], float4(input.pos, 1.0));
    o.pos = mul(gViewProj, wpos);
    o.color = input.color;
    return o;
}

float4 PSMain(VSOut input) : SV_Target0
{
    return input.color;
}
```

---

# Verification checklist

* [x] Engine boots with **0 D3D12 debug layer errors/warnings**.
* [x] World creates multiple objects; parent-child transform works (children move with parent).
* [x] Render shows **N instances** of the triangle at different transforms.
* [x] Scene tolerates mesh asset not ready yet:

  * objects exist
  * renderer simply draws fewer (or none) until ready; no stalls/crashes
* [x] Toggle culling off → far object becomes visible (or count increases).
* [x] Toggle culling on → far object culled; near objects remain.
* [x] No per-frame heap allocations for render submission (RenderQueue uses FrameArena).

---

# Common pitfalls (Phase 10)

* **Dirty propagation bugs**

  * Setting parent but not marking subtree dirty → world matrices/bounds stale.
  * Detach/attach child incorrectly → cycles or orphaned siblings.
* **Handle lifetime bugs**

  * Not bumping generation on destroy → use-after-free via stale handle.
* **Bounds not updated**

  * Culling “randomly hides” objects because world bounds never recomputed.
* **Frustum plane extraction mistakes**

  * Incorrect matrix convention leads to everything culled (or nothing culled).
* **Dependency violations**

  * Renderer querying Runtime objects directly (don’t; only consume `RenderQueue`).

---

## Next chat handoff (Phase 11 — Asset Import Pipeline)

Say this next:

> “Phase 10 is implemented: Runtime owns `World` with stable handles, transform hierarchy with dirty propagation and cached world matrices, AABB bounds updates, camera + frustum culling (toggleable), and a per-frame `RenderQueue` handoff allocated from FrameArena. Render consumes only the queue; MeshPass draws N instances of `triangle.nmsh` via instancing with zero D3D12 debug errors. Start Phase 11 — Asset Import Pipeline (importers, intermediate formats, metadata, dependency graph) per `nocturne_engine_architecture.md`.”
