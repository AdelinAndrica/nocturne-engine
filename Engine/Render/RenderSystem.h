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
		bool ResizeAttachedWindow(uint32_t clientWidth, uint32_t clientHeight);

		void SetResourceManager(ResourceManager* rm);

		// Frame lifecycle (unchanged semantics)
		void BeginFrame();
		void EndFramePresent();

		// Runtime -> Render handoff for this frame (no ownership taken).
		void SetFrameRenderQueue(const RenderQueue* q);

	private:
		struct Impl;
		Impl* impl_ = nullptr;
	};
}
