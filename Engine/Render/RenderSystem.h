#pragma once
#include <cstdint>

namespace noc
{
	class ResourceManager;

	class RenderSystem
	{
	public:
		RenderSystem() = default;

		bool Init(bool enableDebugLayer);
		void Shutdown();

		bool AttachToWindow(void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight);

		void SetResourceManager(ResourceManager* rm);

		void BeginFrame();
		void EndFramePresent();

	private:
		struct Impl;
		Impl* impl_ = nullptr;
	};
}
