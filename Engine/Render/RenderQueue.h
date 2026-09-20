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

	struct RenderLight
	{
		Vec3 position = Vec3::Zero();
		float range = 0.0f;
		Vec3 direction = Vec3{ 0.0f, 0.0f, 1.0f };
		float intensity = 0.0f;
		Vec3 color = Vec3::One();
		float innerConeCos = 1.0f;
		float outerConeCos = 1.0f;
		uint32_t type = 0;
	}

	// Design choice (not directly from the book): a tiny editor/debug bridge is
	// carried with the frame submission so the renderer can depth-test selection
	// visualization without coupling MeshPass directly to editor Win32 code.
	struct RenderDebugSelection
	{
		Vec3 localBoundsMin = Vec3::Zero();
		Vec3 localBoundsMax = Vec3::Zero();
		Mat4 world = Mat4::Identity();
		uint32_t enabled = 0;
	};

	// POD render submission for a single frame.
	// Memory for instances is owned by the caller (FrameArena).
	struct RenderQueue
	{
		RenderView view{};
		const RenderInstance* instances = nullptr;
		uint32_t instanceCount = 0;
		uint32_t totalRenderables = 0;
		const RenderLight* lights = nullptr;
		uint32_t lightCount = 0;
		uint32_t totalLights = 0;
		RenderDebugSelection debugSelection{};
	};
}
