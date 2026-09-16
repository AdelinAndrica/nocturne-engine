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
