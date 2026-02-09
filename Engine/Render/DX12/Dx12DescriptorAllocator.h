#pragma once
#include "Dx12Common.h"
#include <cstdint>

namespace noc
{
	struct Dx12DescriptorHandle
	{
		D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
		D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
		uint32_t index = 0;
		bool shaderVisible = false;
	};

	class Dx12DescriptorAllocator
	{
	public:
		bool Init(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t capacity, bool shaderVisible);
		void Shutdown();

		Dx12DescriptorHandle Allocate(); // linear for Phase 10 (stable, no frees)
		ID3D12DescriptorHeap* Heap() const { return heap_.Get(); }
		uint32_t DescriptorSize() const { return descriptorSize_; }
		bool ShaderVisible() const { return shaderVisible_; }

	private:
		dx12::ComPtr<ID3D12DescriptorHeap> heap_;
		D3D12_DESCRIPTOR_HEAP_TYPE type_ = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		uint32_t descriptorSize_ = 0;
		uint32_t capacity_ = 0;
		uint32_t cursor_ = 0;
		bool shaderVisible_ = false;
	};
}
