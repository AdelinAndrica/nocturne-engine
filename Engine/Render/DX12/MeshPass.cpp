#include "MeshPass.h"

#include "Dx12SwapChain.h"
#include "Dx12FrameSync.h"
#include "ShaderCompiler.h"

#include "Resources/ResourceManager.h"
#include "Render/RenderQueue.h"

namespace noc
{
	struct PerFrameConstants
	{
		Mat4 viewProj;
	};

	static uint64_t HashInputLayoutPC_()
	{
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
		skyReady_ = false;
		rootReady_ = false;
		psoReady_ = false;
		gridPsoReady_ = false;
		meshReady_ = false;
		gridReady_ = false;
		cbReady_ = false;
		instReady_ = false;

		if (!perFrameCB_.Init(device, 64 * 1024))
			return false;

		instanceCapacity_ = 1024;
		return true;
	}

	void MeshPass::Shutdown(Dx12DeferredReleaseQueue& deferred, uint64_t safeFenceValue)
	{
		auto defer = [&](auto& object)
		{
			if (!object)
				return;
			dx12::ComPtr<IUnknown> u;
			object.As(&u);
			deferred.Enqueue(safeFenceValue, std::move(u));
			object.Reset();
		};

		defer(gridPso_);
		defer(gridRootSig_);
		defer(skyPso_);
		defer(skyRootSig_);
		defer(pso_);
		defer(rootSig_);

		gridVb_.ShutdownNow();
		vb_.ShutdownNow();
		ib_.ShutdownNow();

		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
		{
			if (instanceBuf_[i] && instanceMapped_[i])
				instanceBuf_[i]->Unmap(0, nullptr);
			instanceMapped_[i] = nullptr;
			instanceBuf_[i].Reset();

			if (selectionUpload_[i] && selectionMapped_[i])
				selectionUpload_[i]->Unmap(0, nullptr);
			selectionMapped_[i] = nullptr;
			selectionUpload_[i].Reset();
			selectionVbv_[i] = {};
		}

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
			d.SizeInBytes = (UINT)((sizeof(PerFrameConstants) + 255u) & ~255u);
			device->CreateConstantBufferView(&d, perFrameCbv_[i].cpu);
		}
		cbReady_ = true;
	}

	void MeshPass::EnsurePerFrameInstanceSrv_(ID3D12Device* device)
	{
		if (instReady_ || !device || !cbvSrvUav_)
			return;

		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
		{
			instanceSrv_[i] = cbvSrvUav_->Allocate();
			const UINT64 bytes = (UINT64)instanceCapacity_ * sizeof(Mat4);

			D3D12_HEAP_PROPERTIES hp{};
			hp.Type = D3D12_HEAP_TYPE_UPLOAD;
			D3D12_RESOURCE_DESC d = dx12::BufferDesc(bytes);
			if (!dx12::HrOk(device->CreateCommittedResource(
				&hp,
				D3D12_HEAP_FLAG_NONE,
				&d,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&instanceBuf_[i])), "CreateCommittedResource(InstanceUpload)"))
				return;

			void* mapped = nullptr;
			D3D12_RANGE r{ 0, 0 };
			if (!dx12::HrOk(instanceBuf_[i]->Map(0, &r, &mapped), "InstanceUpload.Map"))
				return;
			instanceMapped_[i] = (uint8_t*)mapped;

			D3D12_SHADER_RESOURCE_VIEW_DESC sd{};
			sd.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			sd.Buffer.FirstElement = 0;
			sd.Buffer.NumElements = instanceCapacity_;
			sd.Buffer.StructureByteStride = sizeof(Mat4);
			sd.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			sd.Format = DXGI_FORMAT_UNKNOWN;
			device->CreateShaderResourceView(instanceBuf_[i].Get(), &sd, instanceSrv_[i].cpu);
		}
		instReady_ = true;
	}

	bool MeshPass::EnsureSkyPso_(ID3D12Device* device, ResourceManager* rm)
	{
		if (skyReady_)
			return true;
		if (!device || !rm)
			return false;

		if (!skyHlsl_.IsValid())
			skyHlsl_ = rm->RequestText("Shaders/EditorSky.hlsl");
		const TextResource* src = rm->GetText(skyHlsl_);
		if (!src)
			return false;

		dx12::ComPtr<ID3DBlob> vs;
		dx12::ComPtr<ID3DBlob> ps;
		if (!ShaderCompiler::CompileFromMemory("Shaders/EditorSky.hlsl", src->Str().c_str(), src->Str().size(), "VSMain", "vs_5_1", vs))
			return false;
		if (!ShaderCompiler::CompileFromMemory("Shaders/EditorSky.hlsl", src->Str().c_str(), src->Str().size(), "PSMain", "ps_5_1", ps))
			return false;

		D3D12_ROOT_SIGNATURE_DESC rs{};
		rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
		dx12::ComPtr<ID3DBlob> blob;
		dx12::ComPtr<ID3DBlob> err;
		if (!dx12::HrOk(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err), "SerializeRootSignature(EditorSky)"))
		{
			const char* e = err ? (const char*)err->GetBufferPointer() : "unknown";
			NOC_LOG_ERROR("Render", "Editor sky root signature error: %s", e);
			return false;
		}
		if (!dx12::HrOk(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&skyRootSig_)),
			"CreateRootSignature(EditorSky)"))
			return false;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
		pso.pRootSignature = skyRootSig_.Get();
		pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
		pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
		pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		pso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		pso.DepthStencilState.DepthEnable = FALSE;
		pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		pso.DepthStencilState.StencilEnable = FALSE;
		pso.DSVFormat = Dx12SwapChain::kDepthFormat;
		pso.SampleMask = UINT_MAX;
		pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pso.NumRenderTargets = 1;
		pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		pso.SampleDesc.Count = 1;
		pso.InputLayout = { nullptr, 0 };

		if (!dx12::HrOk(device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&skyPso_)),
			"CreateGraphicsPipelineState(EditorSky)"))
			return false;

		skyReady_ = true;
		NOC_LOG_INFO("Render", "Phase 14 procedural editor sky ready");
		return true;
	}

	bool MeshPass::EnsureRootSigAndPso_(ID3D12Device* device, Dx12PsoCache& cache, ResourceManager* rm)
	{
		if (!device || !rm)
			return false;
		if (psoReady_ && pso_ && rootReady_ && rootSig_)
			return true;
		if (!shaderHlsl_.IsValid())
			shaderHlsl_ = rm->RequestText("Shaders/Basic.hlsl");

		if (!rootReady_)
		{
			D3D12_DESCRIPTOR_RANGE ranges[3]{};
			ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
			ranges[0].NumDescriptors = 1;
			ranges[0].BaseShaderRegister = 0;
			ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			ranges[1].NumDescriptors = 8;
			ranges[1].BaseShaderRegister = 0;
			ranges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
			ranges[2].NumDescriptors = 8;
			ranges[2].BaseShaderRegister = 0;

			D3D12_ROOT_PARAMETER params[3]{};
			for (int i = 0; i < 3; ++i)
			{
				params[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				params[i].DescriptorTable.NumDescriptorRanges = 1;
				params[i].DescriptorTable.pDescriptorRanges = &ranges[i];
				params[i].ShaderVisibility = i == 2 ? D3D12_SHADER_VISIBILITY_PIXEL : D3D12_SHADER_VISIBILITY_ALL;
			}

			D3D12_ROOT_SIGNATURE_DESC rs{};
			rs.NumParameters = 3;
			rs.pParameters = params;
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

		const TextResource* src = rm->GetText(shaderHlsl_);
		if (!src)
			return false;
		dx12::ComPtr<ID3DBlob> vs;
		dx12::ComPtr<ID3DBlob> ps;
		if (!ShaderCompiler::CompileFromMemory("Shaders/Basic.hlsl", src->Str().c_str(), src->Str().size(), "VSMain", "vs_5_1", vs))
			return false;
		if (!ShaderCompiler::CompileFromMemory("Shaders/Basic.hlsl", src->Str().c_str(), src->Str().size(), "PSMain", "ps_5_1", ps))
			return false;

		Dx12PsoKey key{};
		key.vs = vs.Get();
		key.ps = ps.Get();
		key.rootSig = rootSig_.Get();
		key.rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		key.dsvFormat = Dx12SwapChain::kDepthFormat;
		key.inputLayoutHash = HashInputLayoutPC_();
		if (auto* cached = cache.Find(key))
		{
			pso_ = cached;
			psoReady_ = true;
			return true;
		}

		D3D12_INPUT_ELEMENT_DESC layout[2]{};
		layout[0].SemanticName = "POSITION";
		layout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		layout[0].InputSlot = 0;
		layout[1].SemanticName = "COLOR";
		layout[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		layout[1].InputSlot = 0;
		layout[1].AlignedByteOffset = 12;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
		pso.pRootSignature = rootSig_.Get();
		pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
		pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
		pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		pso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		pso.DepthStencilState.DepthEnable = TRUE;
		pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		pso.DepthStencilState.StencilEnable = FALSE;
		pso.DSVFormat = key.dsvFormat;
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

	bool MeshPass::EnsureGridPso_(ID3D12Device* device, ResourceManager* rm)
	{
		if (gridPsoReady_)
			return true;
		if (!device || !rm)
			return false;

		if (!gridHlsl_.IsValid())
			gridHlsl_ = rm->RequestText("Shaders/EditorGrid.hlsl");
		const TextResource* src = rm->GetText(gridHlsl_);
		if (!src)
			return false;

		dx12::ComPtr<ID3DBlob> vs;
		dx12::ComPtr<ID3DBlob> ps;
		if (!ShaderCompiler::CompileFromMemory("Shaders/EditorGrid.hlsl", src->Str().c_str(), src->Str().size(), "VSMain", "vs_5_1", vs))
			return false;
		if (!ShaderCompiler::CompileFromMemory("Shaders/EditorGrid.hlsl", src->Str().c_str(), src->Str().size(), "PSMain", "ps_5_1", ps))
			return false;

		D3D12_DESCRIPTOR_RANGE range{};
		range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		range.NumDescriptors = 1;
		range.BaseShaderRegister = 0;
		D3D12_ROOT_PARAMETER param{};
		param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		param.DescriptorTable.NumDescriptorRanges = 1;
		param.DescriptorTable.pDescriptorRanges = &range;
		param.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		D3D12_ROOT_SIGNATURE_DESC rs{};
		rs.NumParameters = 1;
		rs.pParameters = &param;
		rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		dx12::ComPtr<ID3DBlob> blob;
		dx12::ComPtr<ID3DBlob> err;
		if (!dx12::HrOk(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err), "SerializeRootSignature(EditorGrid)"))
		{
			const char* e = err ? (const char*)err->GetBufferPointer() : "unknown";
			NOC_LOG_ERROR("Render", "Editor grid root signature error: %s", e);
			return false;
		}
		if (!dx12::HrOk(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&gridRootSig_)),
			"CreateRootSignature(EditorGrid)"))
			return false;

		D3D12_INPUT_ELEMENT_DESC layout[2]{};
		layout[0].SemanticName = "POSITION";
		layout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		layout[0].InputSlot = 0;
		layout[1].SemanticName = "COLOR";
		layout[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		layout[1].InputSlot = 0;
		layout[1].AlignedByteOffset = 12;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
		pso.pRootSignature = gridRootSig_.Get();
		pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
		pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
		pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		pso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		pso.DepthStencilState.DepthEnable = TRUE;
		pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		pso.DepthStencilState.StencilEnable = FALSE;
		pso.DSVFormat = Dx12SwapChain::kDepthFormat;
		pso.SampleMask = UINT_MAX;
		pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		pso.NumRenderTargets = 1;
		pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		pso.SampleDesc.Count = 1;
		pso.InputLayout = { layout, 2 };

		if (!dx12::HrOk(device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&gridPso_)),
			"CreateGraphicsPipelineState(EditorGrid)"))
			return false;

		gridPsoReady_ = true;
		NOC_LOG_INFO("Render", "Phase 14 depth-tested GPU editor line pass ready");
		return true;
	}

	bool MeshPass::EnsureValidationCubeUploaded_(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmd,
		Dx12DeferredReleaseQueue& deferred,
		const Dx12FrameSync& sync,
		uint32_t frameIndex)
	{
		if (meshReady_)
			return true;

		CpuMeshPC cpu{};
		auto vertex = [](float x, float y, float z, float r, float g, float b)
		{
			return MeshVertexPC{ x, y, z, r, g, b, 1.0f };
		};
		auto addFace = [&](const MeshVertexPC& a, const MeshVertexPC& b, const MeshVertexPC& c, const MeshVertexPC& d)
		{
			const uint16_t base = static_cast<uint16_t>(cpu.vertices.size());
			cpu.vertices.push_back(a); cpu.vertices.push_back(b); cpu.vertices.push_back(c); cpu.vertices.push_back(d);
			cpu.indices.push_back(base + 0); cpu.indices.push_back(base + 1); cpu.indices.push_back(base + 2);
			cpu.indices.push_back(base + 0); cpu.indices.push_back(base + 2); cpu.indices.push_back(base + 3);
		};

		addFace(vertex(-1,-1,-1, 0.18f,0.45f,0.95f), vertex(-1, 1,-1, 0.18f,0.45f,0.95f), vertex( 1, 1,-1, 0.18f,0.45f,0.95f), vertex( 1,-1,-1, 0.18f,0.45f,0.95f));
		addFace(vertex( 1,-1, 1, 0.13f,0.30f,0.72f), vertex( 1, 1, 1, 0.13f,0.30f,0.72f), vertex(-1, 1, 1, 0.13f,0.30f,0.72f), vertex(-1,-1, 1, 0.13f,0.30f,0.72f));
		addFace(vertex(-1,-1, 1, 0.80f,0.22f,0.28f), vertex(-1, 1, 1, 0.80f,0.22f,0.28f), vertex(-1, 1,-1, 0.80f,0.22f,0.28f), vertex(-1,-1,-1, 0.80f,0.22f,0.28f));
		addFace(vertex( 1,-1,-1, 0.20f,0.72f,0.38f), vertex( 1, 1,-1, 0.20f,0.72f,0.38f), vertex( 1, 1, 1, 0.20f,0.72f,0.38f), vertex( 1,-1, 1, 0.20f,0.72f,0.38f));
		addFace(vertex(-1, 1,-1, 0.92f,0.67f,0.22f), vertex(-1, 1, 1, 0.92f,0.67f,0.22f), vertex( 1, 1, 1, 0.92f,0.67f,0.22f), vertex( 1, 1,-1, 0.92f,0.67f,0.22f));
		addFace(vertex(-1,-1, 1, 0.26f,0.29f,0.34f), vertex(-1,-1,-1, 0.26f,0.29f,0.34f), vertex( 1,-1,-1, 0.26f,0.29f,0.34f), vertex( 1,-1, 1, 0.26f,0.29f,0.34f));

		indexCount_ = static_cast<uint32_t>(cpu.indices.size());
		if (!vb_.CreateStatic(device, cmd, deferred, sync, frameIndex, GpuBuffer::Kind::Vertex,
			cpu.vertices.data(), cpu.vertices.size() * sizeof(MeshVertexPC), sizeof(MeshVertexPC)))
			return false;
		if (!ib_.CreateStatic(device, cmd, deferred, sync, frameIndex, GpuBuffer::Kind::Index,
			cpu.indices.data(), cpu.indices.size() * sizeof(uint16_t), 0))
			return false;

		meshReady_ = true;
		return true;
	}

	bool MeshPass::EnsureGridUploaded_(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmd,
		Dx12DeferredReleaseQueue& deferred,
		const Dx12FrameSync& sync,
		uint32_t frameIndex)
	{
		if (gridReady_)
			return true;

		std::vector<MeshVertexPC> vertices;
		vertices.reserve(128);
		constexpr float gridY = -1.16f;
		constexpr float axisY = -1.14f;
		constexpr int minX = -10;
		constexpr int maxX = 10;
		constexpr int minZ = -4;
		constexpr int maxZ = 20;

		auto addLine = [&](float ax, float ay, float az, float bx, float by, float bz,
			float r, float g, float b)
		{
			vertices.push_back({ ax, ay, az, r, g, b, 1.0f });
			vertices.push_back({ bx, by, bz, r, g, b, 1.0f });
		};

		for (int x = minX; x <= maxX; ++x)
		{
			const bool major = (x % 5) == 0;
			const float c = major ? 0.34f : 0.20f;
			addLine((float)x, gridY, (float)minZ, (float)x, gridY, (float)maxZ, c, c + 0.02f, c + 0.06f);
		}
		for (int z = minZ; z <= maxZ; ++z)
		{
			const bool major = (z % 5) == 0;
			const float c = major ? 0.34f : 0.20f;
			addLine((float)minX, gridY, (float)z, (float)maxX, gridY, (float)z, c, c + 0.02f, c + 0.06f);
		}

		addLine(0.0f, axisY, 0.0f, 2.0f, axisY, 0.0f, 0.90f, 0.18f, 0.20f);
		addLine(0.0f, axisY, 0.0f, 0.0f, axisY + 2.0f, 0.0f, 0.22f, 0.86f, 0.38f);
		addLine(0.0f, axisY, 0.0f, 0.0f, axisY, 2.0f, 0.22f, 0.48f, 0.96f);

		gridVertexCount_ = static_cast<uint32_t>(vertices.size());
		if (!gridVb_.CreateStatic(device, cmd, deferred, sync, frameIndex, GpuBuffer::Kind::Vertex,
			vertices.data(), vertices.size() * sizeof(MeshVertexPC), sizeof(MeshVertexPC)))
			return false;

		gridReady_ = true;
		return true;
	}

	bool MeshPass::EnsureSelectionUpload_(ID3D12Device* device)
	{
		if (!device)
			return false;
		if (selectionUpload_[0])
			return true;

		constexpr UINT64 bytes = sizeof(MeshVertexPC) * 24u;
		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
		{
			D3D12_HEAP_PROPERTIES hp{};
			hp.Type = D3D12_HEAP_TYPE_UPLOAD;
			D3D12_RESOURCE_DESC desc = dx12::BufferDesc(bytes);
			if (!dx12::HrOk(device->CreateCommittedResource(
				&hp, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr, IID_PPV_ARGS(&selectionUpload_[i])), "CreateCommittedResource(EditorSelectionUpload)"))
				return false;

			void* mapped = nullptr;
			D3D12_RANGE noRead{ 0, 0 };
			if (!dx12::HrOk(selectionUpload_[i]->Map(0, &noRead, &mapped), "EditorSelectionUpload.Map"))
				return false;
			selectionMapped_[i] = static_cast<uint8_t*>(mapped);
			selectionVbv_[i].BufferLocation = selectionUpload_[i]->GetGPUVirtualAddress();
			selectionVbv_[i].SizeInBytes = static_cast<UINT>(bytes);
			selectionVbv_[i].StrideInBytes = sizeof(MeshVertexPC);
		}
		return true;
	}

	void MeshPass::Record(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmd,
		Dx12SwapChain& swap,
		const Dx12FrameSync& sync,
		uint32_t frameIndex,
		Dx12DeferredReleaseQueue& deferred,
		ResourceManager* rm,
		const RenderQueue* queue)
	{
		if (!device || !cmd)
			return;

		EnsurePerFrameCbv_(device);
		EnsurePerFrameInstanceSrv_(device);

		auto rtv = swap.CurrentRtv(frameIndex);
		auto dsv = swap.DepthStencilView();
		if (rtv.ptr == 0 || dsv.ptr == 0)
			return;

		cmd->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
		const float clearColor[4] = { 0.018f, 0.025f, 0.040f, 1.0f };
		cmd->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
		cmd->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		D3D12_VIEWPORT vp{};
		vp.Width = (float)swap.Width();
		vp.Height = (float)swap.Height();
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		D3D12_RECT sc{ 0, 0, (LONG)swap.Width(), (LONG)swap.Height() };
		cmd->RSSetViewports(1, &vp);
		cmd->RSSetScissorRects(1, &sc);

		if (rm && EnsureSkyPso_(device, rm))
		{
			cmd->SetGraphicsRootSignature(skyRootSig_.Get());
			cmd->SetPipelineState(skyPso_.Get());
			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			cmd->IASetVertexBuffers(0, 0, nullptr);
			cmd->IASetIndexBuffer(nullptr);
			cmd->DrawInstanced(3, 1, 0, 0);
		}

		if (!rm || !queue)
			return;

		perFrameCB_.BeginFrame(frameIndex);
		D3D12_GPU_VIRTUAL_ADDRESS gpu = 0;
		void* cpu = nullptr;
		if (!perFrameCB_.Allocate(sizeof(PerFrameConstants), gpu, cpu))
			return;
		reinterpret_cast<PerFrameConstants*>(cpu)->viewProj = queue->view.viewProj;

		ID3D12DescriptorHeap* heaps[] = { cbvSrvUav_->Heap() };
		cmd->SetDescriptorHeaps(1, heaps);

		if (queue->instanceCount > 0 &&
			EnsureRootSigAndPso_(device, *psoCache_, rm) &&
			EnsureValidationCubeUploaded_(device, cmd, deferred, sync, frameIndex))
		{
			const uint32_t count = (queue->instanceCount > instanceCapacity_) ? instanceCapacity_ : queue->instanceCount;
			Mat4* dst = reinterpret_cast<Mat4*>(instanceMapped_[frameIndex]);
			for (uint32_t i = 0; i < count; ++i)
				dst[i] = queue->instances[i].world;

			cmd->SetGraphicsRootSignature(rootSig_.Get());
			cmd->SetPipelineState(pso_.Get());
			cmd->SetGraphicsRootDescriptorTable(0, perFrameCbv_[frameIndex].gpu);
			cmd->SetGraphicsRootDescriptorTable(1, instanceSrv_[frameIndex].gpu);
			auto vbv = vb_.VertexView();
			auto ibv = ib_.IndexView(DXGI_FORMAT_R16_UINT);
			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			cmd->IASetVertexBuffers(0, 1, &vbv);
			cmd->IASetIndexBuffer(&ibv);
			cmd->DrawIndexedInstanced(indexCount_, count, 0, 0, 0);
		}

		const bool linePassReady = EnsureGridPso_(device, rm);

		if (linePassReady && EnsureGridUploaded_(device, cmd, deferred, sync, frameIndex))
		{
			cmd->SetGraphicsRootSignature(gridRootSig_.Get());
			cmd->SetPipelineState(gridPso_.Get());
			cmd->SetGraphicsRootDescriptorTable(0, perFrameCbv_[frameIndex].gpu);
			auto gridVbv = gridVb_.VertexView();
			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
			cmd->IASetVertexBuffers(0, 1, &gridVbv);
			cmd->IASetIndexBuffer(nullptr);
			cmd->DrawInstanced(gridVertexCount_, 1, 0, 0);
		}

		// Selection geometry is built from the object's local bounds and then
		// transformed by the selected object's exact world matrix. The outline uses
		// a small constant world-space clearance rather than percentage expansion,
		// so very large/thin objects (such as Ground_Plane) do not look inflated.
		if (linePassReady && queue->debugSelection.enabled && EnsureSelectionUpload_(device))
		{
			const Vec3 localCenter =
				(queue->debugSelection.localBoundsMin + queue->debugSelection.localBoundsMax) * 0.5f;
			Vec3 localHalf =
				(queue->debugSelection.localBoundsMax - queue->debugSelection.localBoundsMin) * 0.5f;

			// Design choice (not directly from the book): keep roughly 6 mm of
			// clearance in world space on each oriented local axis. Convert that
			// fixed world offset back to local units using the scale encoded by the
			// world-matrix columns. This avoids size-dependent outline inflation.
			constexpr float kSelectionWorldOffset = 0.006f;
			const Mat4& selectionWorld = queue->debugSelection.world;
			const float scaleX = Length(Vec3{
				M(selectionWorld, 0, 0), M(selectionWorld, 1, 0), M(selectionWorld, 2, 0) });
			const float scaleY = Length(Vec3{
				M(selectionWorld, 0, 1), M(selectionWorld, 1, 1), M(selectionWorld, 2, 1) });
			const float scaleZ = Length(Vec3{
				M(selectionWorld, 0, 2), M(selectionWorld, 1, 2), M(selectionWorld, 2, 2) });
			const Vec3 localOffset{
				scaleX > 1e-6f ? kSelectionWorldOffset / scaleX : 0.0f,
				scaleY > 1e-6f ? kSelectionWorldOffset / scaleY : 0.0f,
				scaleZ > 1e-6f ? kSelectionWorldOffset / scaleZ : 0.0f
			};
			localHalf = localHalf + localOffset;

			const Vec3 mn = localCenter - localHalf;
			const Vec3 mx = localCenter + localHalf;

			const Vec3 localCorners[8] = {
				{mn.x,mn.y,mn.z},{mx.x,mn.y,mn.z},{mn.x,mx.y,mn.z},{mx.x,mx.y,mn.z},
				{mn.x,mn.y,mx.z},{mx.x,mn.y,mx.z},{mn.x,mx.y,mx.z},{mx.x,mx.y,mx.z}
			};
			Vec3 worldCorners[8]{};
			for (int i = 0; i < 8; ++i)
				worldCorners[i] = TransformPoint(queue->debugSelection.world, localCorners[i]);

			const int edges[12][2] = {
				{0,1},{0,2},{1,3},{2,3},{4,5},{4,6},{5,7},{6,7},{0,4},{1,5},{2,6},{3,7}
			};

			MeshVertexPC* out = reinterpret_cast<MeshVertexPC*>(selectionMapped_[frameIndex]);
			for (int e = 0; e < 12; ++e)
			{
				const Vec3 a = worldCorners[edges[e][0]];
				const Vec3 b = worldCorners[edges[e][1]];
				out[e * 2 + 0] = { a.x, a.y, a.z, 1.00f, 0.72f, 0.16f, 1.0f };
				out[e * 2 + 1] = { b.x, b.y, b.z, 1.00f, 0.72f, 0.16f, 1.0f };
			}

			cmd->SetGraphicsRootSignature(gridRootSig_.Get());
			cmd->SetPipelineState(gridPso_.Get());
			cmd->SetGraphicsRootDescriptorTable(0, perFrameCbv_[frameIndex].gpu);
			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
			cmd->IASetVertexBuffers(0, 1, &selectionVbv_[frameIndex]);
			cmd->IASetIndexBuffer(nullptr);
			cmd->DrawInstanced(24, 1, 0, 0);
		}
	}
}
