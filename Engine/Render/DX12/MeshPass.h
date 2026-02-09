#pragma once
#include "Dx12Common.h"

#include "Dx12DescriptorAllocator.h"
#include "Dx12DeferredReleaseQueue.h"
#include "Dx12PsoCache.h"
#include "GpuBuffer.h"
#include "GpuRingConstantBuffer.h"
#include "MeshFormat.h"

#include "Resources/ResourceHandle.h"
#include "Resources/Typed/ResourceHandleT.h"
#include "Resources/Typed/TextResource.h"

namespace noc
{
	class ResourceManager;
	class Dx12SwapChain;
	class Dx12FrameSync;

	// Phase 10: a minimal “real” draw pass:
	// - shader source from VFS (TextResource)
	// - mesh bytes from VFS (Binary)
	// - default-heap VB/IB
	// - descriptor table for per-frame CBV
	class MeshPass
	{
	public:
		bool Init(
			ID3D12Device* device,
			Dx12DescriptorAllocator& cbvSrvUav,
			Dx12DescriptorAllocator& samplers,
			Dx12PsoCache& psoCache);

		void Shutdown(Dx12DeferredReleaseQueue& deferred, uint64_t safeFenceValue);

		// Called every frame after cmd list is reset and RT is in RT state.
		void Record(
			ID3D12Device* device,
			ID3D12GraphicsCommandList* cmd,
			Dx12SwapChain& swap,
			const Dx12FrameSync& sync,
			uint32_t frameIndex,
			Dx12DeferredReleaseQueue& deferred,
			ResourceManager* rm);

	private:
		bool EnsureRootSigAndPso_(
			ID3D12Device* device,
			Dx12PsoCache& cache,
			ResourceManager* rm);

		bool EnsureMeshUploaded_(
			ID3D12Device* device,
			ID3D12GraphicsCommandList* cmd,
			Dx12DeferredReleaseQueue& deferred,
			const Dx12FrameSync& sync,
			uint32_t frameIndex,
			ResourceManager* rm);

		void EnsurePerFrameCbv_(ID3D12Device* device);

	private:
		// --- Assets (CPU) ---
		ResourceHandle meshBin_{};                 // RequestBinary("Meshes/triangle.nmsh")
		ResourceHandleT<TextResource> shaderHlsl_; // RequestText("Shaders/Basic.hlsl")

		// --- GPU objects ---
		dx12::ComPtr<ID3D12RootSignature> rootSig_;
		dx12::ComPtr<ID3D12PipelineState> pso_;

		GpuBuffer vb_;
		GpuBuffer ib_;
		uint32_t indexCount_ = 0;

		GpuRingConstantBuffer perFrameCB_;
		Dx12DescriptorAllocator* cbvSrvUav_ = nullptr;
		Dx12DescriptorHandle perFrameCbv_[dx12::kFrameCount]{};

		Dx12PsoCache* psoCache_ = nullptr;

		// state flags
		bool rootReady_ = false;
		bool psoReady_ = false;
		bool meshReady_ = false;
		bool cbReady_ = false;
	};
}
