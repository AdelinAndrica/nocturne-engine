#include "GpuRingConstantBuffer.h"

namespace noc
{
	bool GpuRingConstantBuffer::Init(ID3D12Device* device, size_t bytesPerFrame)
	{
		if (!device || bytesPerFrame == 0)
			return false;

		capacity_ = Align256_(bytesPerFrame);

		D3D12_HEAP_PROPERTIES hp{};
		hp.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC d{};
		d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		d.Width = (UINT64)capacity_;
		d.Height = 1;
		d.DepthOrArraySize = 1;
		d.MipLevels = 1;
		d.SampleDesc.Count = 1;
		d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
		{
			if (!dx12::HrOk(device->CreateCommittedResource(
				&hp,
				D3D12_HEAP_FLAG_NONE,
				&d,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&frames_[i].resource)), "CreateCommittedResource(FrameCB)"))
			{
				return false;
			}

			void* mapped = nullptr;
			D3D12_RANGE r{ 0, 0 };
			if (!dx12::HrOk(frames_[i].resource->Map(0, &r, &mapped), "FrameCB.Map"))
				return false;

			frames_[i].mapped = (uint8_t*)mapped;
		}

		return true;
	}

	void GpuRingConstantBuffer::Shutdown()
	{
		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
		{
			if (frames_[i].resource && frames_[i].mapped)
				frames_[i].resource->Unmap(0, nullptr);

			frames_[i].mapped = nullptr;
			frames_[i].resource.Reset();
		}
		capacity_ = 0;
		cursor_ = 0;
		curFrame_ = 0;
	}

	void GpuRingConstantBuffer::BeginFrame(uint32_t frameIndex)
	{
		curFrame_ = frameIndex;
		cursor_ = 0;
	}

	bool GpuRingConstantBuffer::Allocate(size_t bytes, D3D12_GPU_VIRTUAL_ADDRESS& outGpu, void*& outCpu)
	{
		const size_t aligned = Align256_(bytes);
		if (cursor_ + aligned > capacity_)
			return false;

		auto* res = frames_[curFrame_].resource.Get();
		if (!res || !frames_[curFrame_].mapped)
			return false;

		outGpu = res->GetGPUVirtualAddress() + (UINT64)cursor_;
		outCpu = frames_[curFrame_].mapped + cursor_;
		cursor_ += aligned;
		return true;
	}
}
