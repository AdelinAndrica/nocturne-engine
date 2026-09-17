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
		bool ResizeAttachedWindow(uint32_t clientWidth, uint32_t clientHeight);

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
