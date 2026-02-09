#pragma once
#include "Dx12Common.h"

namespace noc
{
	class Dx12FrameSync
	{
	public:
		bool Init(ID3D12Device* device);
		void Shutdown();

		// Wait for the GPU to finish all work up to the last signaled fence.
		void WaitForGpu(ID3D12CommandQueue* queue);

		// Called at end of frame: signal fence for current frame, advance swapchain index, wait if needed.
		void MoveToNextFrame(ID3D12CommandQueue* queue, IDXGISwapChain3* swapChain, uint32_t& inOutFrameIndex);

		// Phase 10: accessors for deferred release + upload tracking.
		uint64_t FenceValueForFrame(uint32_t frameIndex) const { return fenceValues_[frameIndex]; }
		uint64_t CompletedValue() const { return fence_ ? fence_->GetCompletedValue() : 0; }

	private:
		void WaitForFenceValue_(uint64_t v);

	private:
		dx12::ComPtr<ID3D12Fence> fence_;
		HANDLE fenceEvent_ = nullptr;

		uint64_t fenceValues_[dx12::kFrameCount]{};
	};
}
