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
