#pragma once
#include "Dx12Common.h"
#include <cstdint>
#include <cstddef>

namespace noc
{
	class Dx12DeferredReleaseQueue;
	class Dx12FrameSync;

	class GpuBuffer
	{
	public:
		enum class Kind : uint8_t { Vertex, Index };

		bool CreateStatic(
			ID3D12Device* device,
			ID3D12GraphicsCommandList* cmd,
			Dx12DeferredReleaseQueue& deferred,
			const Dx12FrameSync& sync,
			uint32_t frameIndex,
			Kind kind,
			const void* srcData,
			size_t numBytes,
			uint32_t strideBytes);

		void ShutdownNow();

		ID3D12Resource* Resource() const { return resource_.Get(); }
		size_t SizeBytes() const { return sizeBytes_; }

		D3D12_VERTEX_BUFFER_VIEW VertexView() const;
		D3D12_INDEX_BUFFER_VIEW IndexView(DXGI_FORMAT fmt) const;

	private:
		dx12::ComPtr<ID3D12Resource> resource_;
		size_t sizeBytes_ = 0;
		uint32_t strideBytes_ = 0;
		Kind kind_ = Kind::Vertex;
	};
}
