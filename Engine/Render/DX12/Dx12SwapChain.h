#pragma once
#include "Dx12Common.h"
#include <cstdint>

namespace noc
{
	class Dx12SwapChain
	{
	public:
		bool Init(IDXGIFactory6* factory, ID3D12CommandQueue* queue, void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight);
		void Shutdown();

		bool CreateRtvHeapAndViews(ID3D12Device* device);
		bool Resize(ID3D12Device* device, uint32_t clientWidth, uint32_t clientHeight);

		void Present();
		uint32_t FrameIndex() const { return frameIndex_; }
		void UpdateFrameIndex(uint32_t i) { frameIndex_ = i; }

		IDXGISwapChain3* SwapChain() const { return swapChain_.Get(); }

		uint32_t Width() const { return width_; }
		uint32_t Height() const { return height_; }

		D3D12_CPU_DESCRIPTOR_HANDLE CurrentRtv(uint32_t frameIndex) const;

		void TransitionTo(ID3D12GraphicsCommandList* cmd, uint32_t frameIndex, D3D12_RESOURCE_STATES to);

	private:
		bool CreateBackBufferViews_(ID3D12Device* device);

	private:
		uint32_t width_ = 0, height_ = 0;
		uint32_t frameIndex_ = 0;

		dx12::ComPtr<IDXGISwapChain3> swapChain_;

		dx12::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
		uint32_t rtvDescriptorSize_ = 0;

		dx12::ComPtr<ID3D12Resource> backBuffers_[dx12::kFrameCount];
		D3D12_RESOURCE_STATES bbState_[dx12::kFrameCount]{};
	};
}
