#pragma once
#include "Dx12Common.h"

#include "Dx12DescriptorAllocator.h"
#include "Dx12DeferredReleaseQueue.h"
#include "Dx12PsoCache.h"
#include "GpuBuffer.h"
#include "GpuRingConstantBuffer.h"
#include "MeshFormat.h"

#include "Resources/Typed/ResourceHandleT.h"
#include "Resources/Typed/TextResource.h"

namespace noc
{
	class ResourceManager;
	class Dx12SwapChain;
	class Dx12FrameSync;
	struct RenderQueue;

	class MeshPass
	{
	public:
		bool Init(
			ID3D12Device* device,
			Dx12DescriptorAllocator& cbvSrvUav,
			Dx12DescriptorAllocator& samplers,
			Dx12PsoCache& psoCache);

		void Shutdown(Dx12DeferredReleaseQueue& deferred, uint64_t safeFenceValue);

		void Record(
			ID3D12Device* device,
			ID3D12GraphicsCommandList* cmd,
			Dx12SwapChain& swap,
			const Dx12FrameSync& sync,
			uint32_t frameIndex,
			Dx12DeferredReleaseQueue& deferred,
			ResourceManager* rm,
			const RenderQueue* queue);

	private:
		bool EnsureSkyPso_(ID3D12Device* device, ResourceManager* rm);
		bool EnsureRootSigAndPso_(ID3D12Device* device, Dx12PsoCache& cache, ResourceManager* rm);
		bool EnsureGridPso_(ID3D12Device* device, ResourceManager* rm);
		bool EnsureValidationCubeUploaded_(ID3D12Device* device, ID3D12GraphicsCommandList* cmd,
			Dx12DeferredReleaseQueue& deferred, const Dx12FrameSync& sync, uint32_t frameIndex);
		bool EnsureGridUploaded_(ID3D12Device* device, ID3D12GraphicsCommandList* cmd,
			Dx12DeferredReleaseQueue& deferred, const Dx12FrameSync& sync, uint32_t frameIndex);

		void EnsurePerFrameCbv_(ID3D12Device* device);
		void EnsurePerFrameInstanceSrv_(ID3D12Device* device);

	private:
		ResourceHandleT<TextResource> skyHlsl_;
		ResourceHandleT<TextResource> shaderHlsl_;
		ResourceHandleT<TextResource> gridHlsl_;

		// Phase 14 procedural sky pass. Design choice (not directly from the book):
		// a fullscreen gradient provides a stable editor sky without pulling PBR,
		// cubemaps or lighting into this phase.
		dx12::ComPtr<ID3D12RootSignature> skyRootSig_;
		dx12::ComPtr<ID3D12PipelineState> skyPso_;

		// Graphics state shared by the validation mesh pass.
		dx12::ComPtr<ID3D12RootSignature> rootSig_;
		dx12::ComPtr<ID3D12PipelineState> pso_;

		// Design choice (not directly from the book): the editor grid is real GPU
		// line geometry with its own small shader/root signature. It depth-tests
		// against the scene but does not write depth, so objects occlude it correctly.
		dx12::ComPtr<ID3D12RootSignature> gridRootSig_;
		dx12::ComPtr<ID3D12PipelineState> gridPso_;

		// Phase 14 validation geometry. The general multi-mesh render path remains
		// future renderer work; this pass still draws one geometry instanced N times.
		GpuBuffer vb_;
		GpuBuffer ib_;
		uint32_t indexCount_ = 0;

		GpuBuffer gridVb_;
		uint32_t gridVertexCount_ = 0;

		// Per-frame constants
		GpuRingConstantBuffer perFrameCB_;
		Dx12DescriptorAllocator* cbvSrvUav_ = nullptr;
		Dx12DescriptorHandle perFrameCbv_[dx12::kFrameCount]{};

		// Per-frame instance matrices in an upload buffer (mapped once), exposed as SRV t0.
		dx12::ComPtr<ID3D12Resource> instanceBuf_[dx12::kFrameCount];
		uint8_t* instanceMapped_[dx12::kFrameCount]{};
		uint32_t instanceCapacity_ = 0;
		Dx12DescriptorHandle instanceSrv_[dx12::kFrameCount]{};

		Dx12PsoCache* psoCache_ = nullptr;

		bool skyReady_ = false;
		bool rootReady_ = false;
		bool psoReady_ = false;
		bool gridPsoReady_ = false;
		bool meshReady_ = false;
		bool gridReady_ = false;
		bool cbReady_ = false;
		bool instReady_ = false;
	};
}
