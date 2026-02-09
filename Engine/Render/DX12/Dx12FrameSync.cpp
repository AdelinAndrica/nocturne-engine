#include "Dx12FrameSync.h"

namespace noc
{
	bool Dx12FrameSync::Init(ID3D12Device* device)
	{
		if (!device)
			return false;

		if (!dx12::HrOk(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)), "CreateFence"))
			return false;

		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
			fenceValues_[i] = 0;

		fenceEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
		if (!fenceEvent_)
		{
			NOC_LOG_ERROR("Render", "CreateEvent failed (err=%lu)", GetLastError());
			return false;
		}

		// Match Phase 8/9 behavior: bump once so first MoveToNextFrame is sane.
		fenceValues_[0] = 1;
		fenceValues_[1] = 1;
		return true;
	}

	void Dx12FrameSync::Shutdown()
	{
		dx12::SafeCloseHandle(fenceEvent_);
		fence_.Reset();
	}

	void Dx12FrameSync::WaitForFenceValue_(uint64_t v)
	{
		if (!fence_ || !fenceEvent_)
			return;

		if (fence_->GetCompletedValue() >= v)
			return;

		fence_->SetEventOnCompletion(v, fenceEvent_);
		WaitForSingleObjectEx(fenceEvent_, INFINITE, FALSE);
	}

	void Dx12FrameSync::WaitForGpu(ID3D12CommandQueue* queue)
	{
		if (!queue || !fence_)
			return;

		// Use frame 0 fence slot as a “flush” lane.
		const uint64_t v = fenceValues_[0];
		queue->Signal(fence_.Get(), v);
		WaitForFenceValue_(v);
		fenceValues_[0] = v + 1;
	}

	void Dx12FrameSync::MoveToNextFrame(ID3D12CommandQueue* queue, IDXGISwapChain3* swapChain, uint32_t& inOutFrameIndex)
	{
		if (!queue || !swapChain || !fence_)
			return;

		const uint32_t frameIndex = inOutFrameIndex;

		// Signal for the frame we just submitted.
		const uint64_t currentFenceValue = fenceValues_[frameIndex];
		HRESULT hr = queue->Signal(fence_.Get(), currentFenceValue);
		if (FAILED(hr))
		{
			NOC_LOG_ERROR("Render", "MoveToNextFrame: Signal failed (hr=0x%08X)", (unsigned)hr);
			return;
		}

		// Update to the next back buffer.
		inOutFrameIndex = swapChain->GetCurrentBackBufferIndex();

		// Wait until that back buffer is ready (avoid allocator reuse hazards).
		const uint64_t nextFenceValue = fenceValues_[inOutFrameIndex];
		WaitForFenceValue_(nextFenceValue);

		// Set fence value for next time we signal on this frame.
		fenceValues_[inOutFrameIndex] = currentFenceValue + 1;
	}
}
