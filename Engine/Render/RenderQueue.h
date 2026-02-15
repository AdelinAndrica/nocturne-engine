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
