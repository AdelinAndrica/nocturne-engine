#pragma once
#include "Dx12Common.h"
#include <cstdint>
#include <cstddef>

namespace noc
{
	// Per-frame upload-heap constant buffer (one resource per frame).
	// Constants are allocated linearly; reset once per frame.
	class GpuRingConstantBuffer
	{
	public:
		bool Init(ID3D12Device* device, size_t bytesPerFrame);
		void Shutdown();

		void BeginFrame(uint32_t frameIndex);

		// Allocates aligned (256B) constant data in the *current* frame buffer.
		// Returns GPU virtual address and CPU pointer to write into.
		bool Allocate(size_t bytes, D3D12_GPU_VIRTUAL_ADDRESS& outGpu, void*& outCpu);

		ID3D12Resource* Resource(uint32_t frameIndex) const { return frames_[frameIndex].resource.Get(); }
		uint8_t* Mapped(uint32_t frameIndex) const { return frames_[frameIndex].mapped; }
		size_t CapacityBytes() const { return capacity_; }

	private:
		static size_t Align256_(size_t x) { return (x + 255u) & ~255u; }

		struct Frame
		{
			dx12::ComPtr<ID3D12Resource> resource;
			uint8_t* mapped = nullptr;
		};

		Frame frames_[dx12::kFrameCount]{};
		size_t capacity_ = 0;
		size_t cursor_ = 0;
		uint32_t curFrame_ = 0;
	};
}
