#include "Dx12DescriptorAllocator.h"

namespace noc
{
	bool Dx12DescriptorAllocator::Init(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t capacity, bool shaderVisible)
	{
		if (!device || capacity == 0)
			return false;

		type_ = type;
		capacity_ = capacity;
		cursor_ = 0;
		shaderVisible_ = shaderVisible;

		D3D12_DESCRIPTOR_HEAP_DESC hd{};
		hd.Type = type;
		hd.NumDescriptors = capacity;
		hd.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		if (!dx12::HrOk(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap_)), "CreateDescriptorHeap"))
			return false;

		descriptorSize_ = device->GetDescriptorHandleIncrementSize(type);
		return true;
	}

	void Dx12DescriptorAllocator::Shutdown()
	{
		heap_.Reset();
		capacity_ = 0;
		cursor_ = 0;
		descriptorSize_ = 0;
		shaderVisible_ = false;
	}

	Dx12DescriptorHandle Dx12DescriptorAllocator::Allocate()
	{
		Dx12DescriptorHandle h{};
		if (!heap_ || cursor_ >= capacity_)
		{
			NOC_LOG_ERROR("Render", "DescriptorAllocator out of space (type=%u cap=%u)", (unsigned)type_, capacity_);
			return h;
		}

		const uint32_t idx = cursor_++;
		D3D12_CPU_DESCRIPTOR_HANDLE cpu = heap_->GetCPUDescriptorHandleForHeapStart();
		cpu.ptr += (SIZE_T)idx * (SIZE_T)descriptorSize_;

		h.cpu = cpu;
		h.index = idx;
		h.shaderVisible = shaderVisible_;

		if (shaderVisible_)
		{
			D3D12_GPU_DESCRIPTOR_HANDLE gpu = heap_->GetGPUDescriptorHandleForHeapStart();
			gpu.ptr += (UINT64)idx * (UINT64)descriptorSize_;
			h.gpu = gpu;
		}

		return h;
	}
}
