#include "MeshPass.h"

#include "Dx12SwapChain.h"
#include "Dx12FrameSync.h"
#include "ShaderCompiler.h"

#include "Resources/ResourceManager.h"

namespace noc
{
	struct PerFrameConstants
	{
		float time;
		float pad[3];
	};

	static uint64_t HashInputLayoutPC_()
	{
		// Stable constant for Phase 10: position+color layout.
		// (Design choice) Replace with real hashing later.
		return 0xA0C0CA11u;
	}

	bool MeshPass::Init(
		ID3D12Device* device,
		Dx12DescriptorAllocator& cbvSrvUav,
		Dx12DescriptorAllocator& samplers,
		Dx12PsoCache& psoCache)
	{
		(void)samplers;

		if (!device)
			return false;

		cbvSrvUav_ = &cbvSrvUav;
		psoCache_ = &psoCache;

		// Request assets (vpaths are relative to your mounted content root).
		// These return immediately; readiness is polled in Record().
		// NOTE: shader file should live at Data/Shaders/Basic.hlsl, and mesh at Data/Meshes/triangle.nmsh.
		// VFS mount in Phase 3/6 already maps Data/ as root.
		// If your VFS uses different conventions, adjust vpaths accordingly.
		rootReady_ = false;
		psoReady_ = false;
		meshReady_ = false;
		cbReady_ = false;

		// Per-frame constant buffers (upload heap, one per swap buffer).
		if (!perFrameCB_.Init(device, 64 * 1024))
			return false;

		return true;
	}

	void MeshPass::Shutdown(Dx12DeferredReleaseQueue& deferred, uint64_t safeFenceValue)
	{
		// Queue GPU objects for safe release (or release now after GPU idle).
		if (pso_)
		{
			dx12::ComPtr<IUnknown> u;
			pso_.As(&u);
			deferred.Enqueue(safeFenceValue, std::move(u));
			pso_.Reset();
		}
		if (rootSig_)
		{
			dx12::ComPtr<IUnknown> u;
			rootSig_.As(&u);
			deferred.Enqueue(safeFenceValue, std::move(u));
			rootSig_.Reset();
		}

		vb_.ShutdownNow();
		ib_.ShutdownNow();

		perFrameCB_.Shutdown();
	}

	void MeshPass::EnsurePerFrameCbv_(ID3D12Device* device)
	{
		if (cbReady_ || !device || !cbvSrvUav_)
			return;

		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
		{
			perFrameCbv_[i] = cbvSrvUav_->Allocate();

			D3D12_CONSTANT_BUFFER_VIEW_DESC d{};
			d.BufferLocation = perFrameCB_.Resource(i)->GetGPUVirtualAddress();
			// CBV size must be 256-byte aligned.
			d.SizeInBytes = (UINT)((sizeof(PerFrameConstants) + 255u) & ~255u);

			device->CreateConstantBufferView(&d, perFrameCbv_[i].cpu);
		}

		cbReady_ = true;
	}

	bool MeshPass::EnsureRootSigAndPso_(ID3D12Device* device, Dx12PsoCache& cache, ResourceManager* rm)
	{
		if (!device || !rm)
			return false;

		// Request handles once.
		if (!shaderHlsl_.IsValid())
			shaderHlsl_ = rm->RequestText("Shaders/Basic.hlsl");

		// Root signature: build once (no dependency on asset readiness).
		if (!rootReady_)
		{
			// Root parameters:
			// 0: CBV table (b0) per-frame
			// 1: SRV table (t0..t7) per-draw/material (future)
			// 2: Sampler table (s0..s7) (future)
			D3D12_DESCRIPTOR_RANGE ranges[3]{};

			ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
			ranges[0].NumDescriptors = 1;
			ranges[0].BaseShaderRegister = 0;
			ranges[0].RegisterSpace = 0;
			ranges[0].OffsetInDescriptorsFromTableStart = 0;

			ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			ranges[1].NumDescriptors = 8;
			ranges[1].BaseShaderRegister = 0;
			ranges[1].RegisterSpace = 0;
			ranges[1].OffsetInDescriptorsFromTableStart = 0;

			ranges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
			ranges[2].NumDescriptors = 8;
			ranges[2].BaseShaderRegister = 0;
			ranges[2].RegisterSpace = 0;
			ranges[2].OffsetInDescriptorsFromTableStart = 0;

			D3D12_ROOT_PARAMETER params[3]{};

			params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			params[0].DescriptorTable.NumDescriptorRanges = 1;
			params[0].DescriptorTable.pDescriptorRanges = &ranges[0];
			params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

			params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			params[1].DescriptorTable.NumDescriptorRanges = 1;
			params[1].DescriptorTable.pDescriptorRanges = &ranges[1];
			params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

			params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			params[2].DescriptorTable.NumDescriptorRanges = 1;
			params[2].DescriptorTable.pDescriptorRanges = &ranges[2];
			params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

			D3D12_ROOT_SIGNATURE_DESC rs{};
			rs.NumParameters = 3;
			rs.pParameters = params;
			rs.NumStaticSamplers = 0;
			rs.pStaticSamplers = nullptr;
			rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

			dx12::ComPtr<ID3DBlob> blob;
			dx12::ComPtr<ID3DBlob> err;
			if (!dx12::HrOk(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err), "SerializeRootSignature"))
			{
				const char* e = err ? (const char*)err->GetBufferPointer() : "unknown";
				NOC_LOG_ERROR("Render", "RootSig serialize error: %s", e);
				return false;
			}

			if (!dx12::HrOk(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSig_)), "CreateRootSignature"))
				return false;

			rootReady_ = true;
		}

		// Shader compilation requires the TextResource to be ready.
		const TextResource* src = rm->GetText(shaderHlsl_);
		if (!src)
			return false; // not ready yet

		dx12::ComPtr<ID3DBlob> vs;
		dx12::ComPtr<ID3DBlob> ps;

		if (!ShaderCompiler::CompileFromMemory("Shaders/Basic.hlsl", src->Str().c_str(), src->Str().size(), "VSMain", "vs_5_1", vs))
			return false;
		if (!ShaderCompiler::CompileFromMemory("Shaders/Basic.hlsl", src->Str().c_str(), src->Str().size(), "PSMain", "ps_5_1", ps))
			return false;

		// PSO cache lookup.
		Dx12PsoKey key{};
		key.vs = vs.Get();
		key.ps = ps.Get();
		key.rootSig = rootSig_.Get();
		key.rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		key.inputLayoutHash = HashInputLayoutPC_();

		if (auto* cached = cache.Find(key))
		{
			pso_ = cached;
			psoReady_ = true;
			return true;
		}

		// Create PSO
		D3D12_INPUT_ELEMENT_DESC layout[2]{};
		layout[0].SemanticName = "POSITION";
		layout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		layout[0].InputSlot = 0;
		layout[0].AlignedByteOffset = 0;
		layout[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

		layout[1].SemanticName = "COLOR";
		layout[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		layout[1].InputSlot = 0;
		layout[1].AlignedByteOffset = 12;
		layout[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
		pso.pRootSignature = rootSig_.Get();
		pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
		pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
		pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		pso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		pso.DepthStencilState.DepthEnable = FALSE;
		pso.DepthStencilState.StencilEnable = FALSE;
		pso.SampleMask = UINT_MAX;
		pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pso.NumRenderTargets = 1;
		pso.RTVFormats[0] = key.rtvFormat;
		pso.SampleDesc.Count = 1;
		pso.InputLayout = { layout, 2 };

		dx12::ComPtr<ID3D12PipelineState> created;
		if (!dx12::HrOk(device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&created)), "CreateGraphicsPipelineState"))
			return false;

		pso_ = created;
		cache.Insert(key, std::move(created));
		psoReady_ = true;
		return true;
	}

	bool MeshPass::EnsureMeshUploaded_(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmd,
		Dx12DeferredReleaseQueue& deferred,
		const Dx12FrameSync& sync,
		uint32_t frameIndex,
		ResourceManager* rm)
	{
		if (meshReady_)
			return true;

		if (!meshBin_.IsValid())
			meshBin_ = rm->RequestBinary("Meshes/triangle.nmsh");

		if (!rm->IsReady(meshBin_))
			return false;

		const uint8_t* bytes = rm->GetBytes(meshBin_);
		const size_t size = rm->GetSize(meshBin_);
		if (!bytes || size == 0)
			return false;

		CpuMeshPC cpu{};
		const char* err = nullptr;
		if (!ParseNocMeshPC(bytes, size, cpu, err))
		{
			NOC_LOG_ERROR("Render", "Mesh parse failed: %s", err ? err : "unknown");
			return false;
		}

		indexCount_ = (uint32_t)cpu.indices.size();

		if (!vb_.CreateStatic(device, cmd, deferred, sync, frameIndex, GpuBuffer::Kind::Vertex,
			cpu.vertices.data(), cpu.vertices.size() * sizeof(MeshVertexPC), sizeof(MeshVertexPC)))
			return false;

		if (!ib_.CreateStatic(device, cmd, deferred, sync, frameIndex, GpuBuffer::Kind::Index,
			cpu.indices.data(), cpu.indices.size() * sizeof(uint16_t), 0))
			return false;

		meshReady_ = true;
		return true;
	}

	void MeshPass::Record(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmd,
		Dx12SwapChain& swap,
		const Dx12FrameSync& sync,
		uint32_t frameIndex,
		Dx12DeferredReleaseQueue& deferred,
		ResourceManager* rm)
	{
		if (!device || !cmd)
			return;

		// Always ensure per-frame CBV descriptors exist.
		EnsurePerFrameCbv_(device);

		// Clear RT (swap already provides RTV).
		auto rtv = swap.CurrentRtv(frameIndex);
		cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

		const float clearColor[4] = { 0.05f, 0.05f, 0.08f, 1.0f };
		cmd->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

		if (!rm)
			return;

		// Build root/PSO when shader becomes ready.
		if (!EnsureRootSigAndPso_(device, *psoCache_, rm))
			return;

		// Upload mesh when bytes become ready.
		if (!EnsureMeshUploaded_(device, cmd, deferred, sync, frameIndex, rm))
			return;

		// Write per-frame constants.
		perFrameCB_.BeginFrame(frameIndex);
		D3D12_GPU_VIRTUAL_ADDRESS gpu = 0;
		void* cpu = nullptr;
		if (perFrameCB_.Allocate(sizeof(PerFrameConstants), gpu, cpu))
		{
			auto* c = (PerFrameConstants*)cpu;
			c->time = 0.0f; // (Design choice) wire real time later
		}

		// Bind descriptor heaps (shader-visible).
		ID3D12DescriptorHeap* heaps[] = { cbvSrvUav_->Heap() };
		cmd->SetDescriptorHeaps(1, heaps);

		cmd->SetGraphicsRootSignature(rootSig_.Get());
		cmd->SetPipelineState(pso_.Get());

		// Root slot 0: per-frame CBV table (b0)
		cmd->SetGraphicsRootDescriptorTable(0, perFrameCbv_[frameIndex].gpu);

		// Viewport/scissor from swapchain size.
		D3D12_VIEWPORT vp{};
		vp.Width = (float)swap.Width();
		vp.Height = (float)swap.Height();
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;

		D3D12_RECT sc{};
		sc.left = 0;
		sc.top = 0;
		sc.right = (LONG)swap.Width();
		sc.bottom = (LONG)swap.Height();

		cmd->RSSetViewports(1, &vp);
		cmd->RSSetScissorRects(1, &sc);

		// IA bind
		auto vbv = vb_.VertexView();
		auto ibv = ib_.IndexView(DXGI_FORMAT_R16_UINT);

		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmd->IASetVertexBuffers(0, 1, &vbv);
		cmd->IASetIndexBuffer(&ibv);

		cmd->DrawIndexedInstanced(indexCount_, 1, 0, 0, 0);
	}
}
