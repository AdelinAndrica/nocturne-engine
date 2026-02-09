#include "GpuBuffer.h"
#include "Dx12DeferredReleaseQueue.h"
#include "Dx12FrameSync.h"

namespace noc
{
	static D3D12_RESOURCE_DESC BufferDesc_(UINT64 bytes)
	{
		D3D12_RESOURCE_DESC d{};
		d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		d.Alignment = 0;
		d.Width = bytes;
		d.Height = 1;
		d.DepthOrArraySize = 1;
		d.MipLevels = 1;
		d.Format = DXGI_FORMAT_UNKNOWN;
		d.SampleDesc.Count = 1;
		d.SampleDesc.Quality = 0;
		d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		d.Flags = D3D12_RESOURCE_FLAG_NONE;
		return d;
	}

	bool GpuBuffer::CreateStatic(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmd,
		Dx12DeferredReleaseQueue& deferred,
		const Dx12FrameSync& sync,
		uint32_t frameIndex,
		Kind kind,
		const void* srcData,
		size_t numBytes,
		uint32_t strideBytes)
	{
		if (!device || !cmd || !srcData || numBytes == 0)
			return false;

		ShutdownNow();

		kind_ = kind;
		sizeBytes_ = numBytes;
		strideBytes_ = strideBytes;

		// Default heap (GPU-only)
		D3D12_HEAP_PROPERTIES hpDefault{};
		hpDefault.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC desc = BufferDesc_((UINT64)numBytes);

		if (!dx12::HrOk(device->CreateCommittedResource(
			&hpDefault,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&resource_)), "CreateCommittedResource(DefaultBuffer)"))
		{
			return false;
		}

		// Upload heap (CPU-visible staging)
		D3D12_HEAP_PROPERTIES hpUpload{};
		hpUpload.Type = D3D12_HEAP_TYPE_UPLOAD;

		dx12::ComPtr<ID3D12Resource> upload;
		if (!dx12::HrOk(device->CreateCommittedResource(
			&hpUpload,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&upload)), "CreateCommittedResource(UploadBuffer)"))
		{
			resource_.Reset();
			return false;
		}

		// Copy bytes into upload.
		void* mapped = nullptr;
		D3D12_RANGE r{ 0, 0 };
		if (!dx12::HrOk(upload->Map(0, &r, &mapped), "UploadBuffer.Map"))
		{
			resource_.Reset();
			upload.Reset();
			return false;
		}
		memcpy(mapped, srcData, numBytes);
		upload->Unmap(0, nullptr);

		// Record copy into default buffer.
		cmd->CopyBufferRegion(resource_.Get(), 0, upload.Get(), 0, (UINT64)numBytes);

		// Transition to final state for binding.
		D3D12_RESOURCE_STATES finalState =
			(kind == Kind::Vertex) ? D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
			: D3D12_RESOURCE_STATE_INDEX_BUFFER;

		D3D12_RESOURCE_BARRIER b = dx12::TransitionBarrier(resource_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, finalState);
		cmd->ResourceBarrier(1, &b);

		// Keep upload alive until the fence value for this frame has completed.
		// We tag with the value that *will be signaled* for this frame index.
		{
			dx12::ComPtr<IUnknown> asUnknown;
			upload.As(&asUnknown);
			deferred.Enqueue(sync.FenceValueForFrame(frameIndex), std::move(asUnknown));
		}

		return true;
	}

	void GpuBuffer::ShutdownNow()
	{
		resource_.Reset();
		sizeBytes_ = 0;
		strideBytes_ = 0;
		kind_ = Kind::Vertex;
	}

	D3D12_VERTEX_BUFFER_VIEW GpuBuffer::VertexView() const
	{
		D3D12_VERTEX_BUFFER_VIEW v{};
		if (!resource_)
			return v;

		v.BufferLocation = resource_->GetGPUVirtualAddress();
		v.SizeInBytes = (UINT)sizeBytes_;
		v.StrideInBytes = strideBytes_;
		return v;
	}

	D3D12_INDEX_BUFFER_VIEW GpuBuffer::IndexView(DXGI_FORMAT fmt) const
	{
		D3D12_INDEX_BUFFER_VIEW v{};
		if (!resource_)
			return v;

		v.BufferLocation = resource_->GetGPUVirtualAddress();
		v.SizeInBytes = (UINT)sizeBytes_;
		v.Format = fmt;
		return v;
	}
}
