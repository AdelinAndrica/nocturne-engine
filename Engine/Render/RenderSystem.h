#pragma once
#include <cstdint>

namespace noc
{
	// Minimal render bootstrap system (DX12 implementation lives in .cpp).
	// Public header stays platform-agnostic: only uses void* for HWND.
	class RenderSystem
	{
	public:
		RenderSystem() = default;

		// Engine lifecycle.
		bool Init(bool enableDebugLayer);
		void Shutdown();

		// Must be called after window creation (needs HWND + client size).
		bool AttachToWindow(void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight);

		// Per-frame.
		void BeginFrame();
		void EndFramePresent(); // clears + presents

	private:
		struct Impl;
		Impl* impl_ = nullptr;
	};
}