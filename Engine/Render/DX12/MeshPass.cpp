#include "MeshPass.h"

#include "Dx12SwapChain.h"
#include "Dx12FrameSync.h"
#include "ShaderCompiler.h"

#include "Resources/ResourceManager.h"
#include "Resources/Typed/MeshResource.h"
#include "Render/RenderQueue.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <new>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace noc
{
	namespace
	{
		constexpr uint32_t kMaxShaderLights = 32;

		struct GpuLightConstants
		{
			Vec4 positionType{};
			Vec4 directionRange{};
			Vec4 colorIntensity{};
			Vec4 spot{};
		};

		struct PerFrameConstants
		{
			Mat4 viewProj;
			GpuLightConstants lights[kMaxShaderLights]{};
			uint32_t lightCount = 0;
			float padding[3]{};
		};

		struct GpuInstanceData
		{
			Mat4 world;
			Mat4 normalWorld;
		};

		struct MeshVertexPN
		{
			float px = 0.0f;
			float py = 0.0f;
			float pz = 0.0f;
			float nx = 0.0f;
			float ny = 1.0f;
			float nz = 0.0f;
		};

		[[nodiscard]] uint64_t MeshKey_(ResourceHandle handle) noexcept
		{
			return (static_cast<uint64_t>(handle.generation) << 32u)
				| static_cast<uint64_t>(handle.index);
		}

		[[nodiscard]] Vec3 NormalizeSafe_(
			const Vec3& value,
			const Vec3& fallback = Vec3{ 0.0f, 1.0f, 0.0f }) noexcept
		{
			const float length = Length(value);
			return length > 1.0e-8f
				? value * (1.0f / length)
				: fallback;
		}

		[[nodiscard]] Mat4 NormalMatrix_(const Mat4& world) noexcept
		{
			Mat4 inverse{};
			if (!TryInverseAffine(world, inverse))
				return Mat4::Identity();

			Mat4 result = Mat4::Identity();
			for (uint32_t row = 0; row < 3; ++row)
			{
				for (uint32_t column = 0; column < 3; ++column)
					M(result, row, column) = M(inverse, column, row);
			}
			return result;
		}

		static uint64_t HashInputLayoutPN_()
		{
			return 0xA0C0CA12u;
		}
	}

	struct MeshPass::GpuMesh
	{
		GpuBuffer vertexBuffer;
		GpuBuffer indexBuffer;
		uint32_t indexCount = 0;
		bool ready = false;
		bool failed = false;
	};

	struct MeshPass::Impl
	{
		std::unordered_map<uint64_t, std::unique_ptr<GpuMesh>> meshes;
		std::unordered_set<uint64_t> resourceFailureLogged;
		bool lightOverflowWarned = false;
	};

	MeshPass::~MeshPass()
	{
		delete impl_;
		impl_ = nullptr;
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
		gridReady_ = false;
		cbReady_ = false;

		if (!perFrameCB_.Init(device, 64 * 1024))
			return false;

		if (!impl_)
		{
			impl_ = new (std::nothrow) Impl();
			if (!impl_)
			{
				perFrameCB_.Shutdown();
				return false;
			}
		}

		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
			instanceCapacity_[i] = 0;
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

		if (impl_)
		{
			for (auto& entry : impl_->meshes)
			{
				if (entry.second)
				{
					entry.second->vertexBuffer.ShutdownNow();
					entry.second->indexBuffer.ShutdownNow();
				}
			}
			impl_->meshes.clear();
			impl_->resourceFailureLogged.clear();
			impl_->lightOverflowWarned = false;
		}

		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
		{
			if (instanceBuf_[i] && instanceMapped_[i])
				instanceBuf_[i]->Unmap(0, nullptr);
			instanceMapped_[i] = nullptr;
			instanceBuf_[i].Reset();
			instanceCapacity_[i] = 0;

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

	bool MeshPass::EnsureInstanceBuffer_(
		ID3D12Device* device,
		uint32_t frameIndex,
		uint32_t requiredCapacity)
	{
		if (!device
			|| !cbvSrvUav_
			|| frameIndex >= dx12::kFrameCount)
		{
			return false;
		}

		if (requiredCapacity == 0)
			return true;

		if (instanceMapped_[frameIndex]
			&& instanceCapacity_[frameIndex] >= requiredCapacity)
		{
			return true;
		}

		uint32_t newCapacity = 64;
		while (newCapacity < requiredCapacity)
		{
			if (newCapacity > 0x40000000u)
				return false;
			newCapacity *= 2u;
		}

		if (instanceBuf_[frameIndex] && instanceMapped_[frameIndex])
			instanceBuf_[frameIndex]->Unmap(0, nullptr);
		instanceMapped_[frameIndex] = nullptr;
		instanceBuf_[frameIndex].Reset();
		instanceCapacity_[frameIndex] = 0;

		if (instanceSrv_[frameIndex].cpu.ptr == 0)
			instanceSrv_[frameIndex] = cbvSrvUav_->Allocate();
		if (instanceSrv_[frameIndex].cpu.ptr == 0)
			return false;

		const UINT64 bytes =
			static_cast<UINT64>(newCapacity)
			* sizeof(GpuInstanceData);
		D3D12_HEAP_PROPERTIES heap{};
		heap.Type = D3D12_HEAP_TYPE_UPLOAD;
		D3D12_RESOURCE_DESC desc = dx12::BufferDesc(bytes);
		if (!dx12::HrOk(
				device->CreateCommittedResource(
					&heap,
					D3D12_HEAP_FLAG_NONE,
					&desc,
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(&instanceBuf_[frameIndex])),
				"CreateCommittedResource(InstanceUpload)"))
		{
			return false;
		}

		void* mapped = nullptr;
		D3D12_RANGE noRead{ 0, 0 };
		if (!dx12::HrOk(
				instanceBuf_[frameIndex]->Map(
					0,
					&noRead,
					&mapped),
				"InstanceUpload.Map"))
		{
			instanceBuf_[frameIndex].Reset();
			return false;
		}

		instanceMapped_[frameIndex] =
			static_cast<uint8_t*>(mapped);
		instanceCapacity_[frameIndex] = newCapacity;

		D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
		srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srv.Shader4ComponentMapping =
			D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv.Buffer.FirstElement = 0;
		srv.Buffer.NumElements = newCapacity;
		srv.Buffer.StructureByteStride = sizeof(GpuInstanceData);
		srv.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		srv.Format = DXGI_FORMAT_UNKNOWN;
		device->CreateShaderResourceView(
			instanceBuf_[frameIndex].Get(),
			&srv,
			instanceSrv_[frameIndex].cpu);
		return true;
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
			D3D12_DESCRIPTOR_RANGE ranges[2]{};
			ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
			ranges[0].NumDescriptors = 1;
			ranges[0].BaseShaderRegister = 0;
			ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			ranges[1].NumDescriptors = 1;
			ranges[1].BaseShaderRegister = 0;

			D3D12_ROOT_PARAMETER params[3]{};
			params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			params[0].DescriptorTable.NumDescriptorRanges = 1;
			params[0].DescriptorTable.pDescriptorRanges = &ranges[0];
			params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

			params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			params[1].DescriptorTable.NumDescriptorRanges = 1;
			params[1].DescriptorTable.pDescriptorRanges = &ranges[1];
			params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

			// Design choice (not directly from the book): scene meshes are submitted
			// as independent draw calls in Phase 16. A single root constant selects
			// the corresponding transform inside the frame instance StructuredBuffer.
			params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
			params[2].Constants.ShaderRegister = 1;
			params[2].Constants.RegisterSpace = 0;
			params[2].Constants.Num32BitValues = 1;
			params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

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
		key.inputLayoutHash = HashInputLayoutPN_();
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
		layout[1].SemanticName = "NORMAL";
		layout[1].Format = DXGI_FORMAT_R32G32B32_FLOAT;
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

	MeshPass::GpuMesh* MeshPass::EnsureMeshUploaded_(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmd,
		Dx12DeferredReleaseQueue& deferred,
		const Dx12FrameSync& sync,
		uint32_t frameIndex,
		ResourceManager& resources,
		ResourceHandle handle)
	{
		if (!impl_
			|| !device
			|| !cmd
			|| !handle.IsValid())
		{
			return nullptr;
		}

		const uint64_t key = MeshKey_(handle);
		const auto cached = impl_->meshes.find(key);
		if (cached != impl_->meshes.end())
		{
			return cached->second && cached->second->ready
				? cached->second.get()
				: nullptr;
		}

		const ResourceHandleT<MeshResource> typed{ handle };
		const MeshResource* resource = resources.GetMesh(typed);
		if (!resource)
		{
			if (resources.HasFailed(handle)
				&& impl_->resourceFailureLogged.insert(key).second)
			{
				NOC_LOG_ERROR(
					"Render",
					"Mesh resource failed to load (handle=%u:%u): %s",
					handle.index,
					handle.generation,
					resources.GetError(handle));
			}
			return nullptr;
		}

		const IntermediateMesh& cpu = resource->CpuMesh();
		const uint32_t vertexCount =
			static_cast<uint32_t>(cpu.positions.size() / 3u);
		if (vertexCount == 0
			|| cpu.positions.size() != static_cast<size_t>(vertexCount) * 3u
			|| cpu.indices.empty()
			|| (cpu.indices.size() % 3u) != 0u)
		{
			NOC_LOG_ERROR(
				"Render",
				"Mesh resource has invalid triangle geometry (handle=%u:%u)",
				handle.index,
				handle.generation);
			return nullptr;
		}

		std::vector<Vec3> normals(vertexCount, Vec3::Zero());
		const bool hasNormals =
			cpu.normals.size()
				== static_cast<size_t>(vertexCount) * 3u;

		if (hasNormals)
		{
			for (uint32_t i = 0; i < vertexCount; ++i)
			{
				normals[i] = NormalizeSafe_(Vec3{
					cpu.normals[i * 3u + 0u],
					cpu.normals[i * 3u + 1u],
					cpu.normals[i * 3u + 2u]
				});
			}
		}
		else
		{
			for (size_t i = 0; i < cpu.indices.size(); i += 3u)
			{
				const uint32_t ia = cpu.indices[i + 0u];
				const uint32_t ib = cpu.indices[i + 1u];
				const uint32_t ic = cpu.indices[i + 2u];
				if (ia >= vertexCount || ib >= vertexCount || ic >= vertexCount)
				{
					NOC_LOG_ERROR(
						"Render",
						"Mesh resource contains out-of-range index (handle=%u:%u)",
						handle.index,
						handle.generation);
					return nullptr;
				}

				const Vec3 a{
					cpu.positions[ia * 3u + 0u],
					cpu.positions[ia * 3u + 1u],
					cpu.positions[ia * 3u + 2u]
				};
				const Vec3 b{
					cpu.positions[ib * 3u + 0u],
					cpu.positions[ib * 3u + 1u],
					cpu.positions[ib * 3u + 2u]
				};
				const Vec3 c{
					cpu.positions[ic * 3u + 0u],
					cpu.positions[ic * 3u + 1u],
					cpu.positions[ic * 3u + 2u]
				};
				const Vec3 face = Cross(b - a, c - a);
				normals[ia] = normals[ia] + face;
				normals[ib] = normals[ib] + face;
				normals[ic] = normals[ic] + face;
			}

			for (Vec3& normal : normals)
				normal = NormalizeSafe_(normal);
		}

		for (const uint32_t index : cpu.indices)
		{
			if (index >= vertexCount)
			{
				NOC_LOG_ERROR(
					"Render",
					"Mesh resource contains out-of-range index (handle=%u:%u)",
					handle.index,
					handle.generation);
				return nullptr;
			}
		}

		std::vector<MeshVertexPN> vertices(vertexCount);
		for (uint32_t i = 0; i < vertexCount; ++i)
		{
			vertices[i] = MeshVertexPN{
				cpu.positions[i * 3u + 0u],
				cpu.positions[i * 3u + 1u],
				cpu.positions[i * 3u + 2u],
				normals[i].x,
				normals[i].y,
				normals[i].z
			};
		}

		auto owned = std::make_unique<GpuMesh>();
		GpuMesh* gpuMesh = owned.get();
		impl_->meshes.emplace(key, std::move(owned));

		if (!gpuMesh->vertexBuffer.CreateStatic(
				device,
				cmd,
				deferred,
				sync,
				frameIndex,
				GpuBuffer::Kind::Vertex,
				vertices.data(),
				vertices.size() * sizeof(MeshVertexPN),
				sizeof(MeshVertexPN))
			|| !gpuMesh->indexBuffer.CreateStatic(
				device,
				cmd,
				deferred,
				sync,
				frameIndex,
				GpuBuffer::Kind::Index,
				cpu.indices.data(),
				cpu.indices.size() * sizeof(uint32_t),
				0))
		{
			gpuMesh->failed = true;
			NOC_LOG_ERROR(
				"Render",
				"GPU mesh upload failed (handle=%u:%u)",
				handle.index,
				handle.generation);
			return nullptr;
		}

		gpuMesh->indexCount =
			static_cast<uint32_t>(cpu.indices.size());
		gpuMesh->ready = true;
		return gpuMesh;
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
		auto* frameConstants =
			reinterpret_cast<PerFrameConstants*>(cpu);
		*frameConstants = {};
		frameConstants->viewProj = queue->view.viewProj;

		const uint32_t lightCount =
			std::min(queue->lightCount, kMaxShaderLights);
		frameConstants->lightCount = lightCount;
		for (uint32_t i = 0; i < lightCount; ++i)
		{
			const RenderLight& light = queue->lights[i];
			GpuLightConstants& out = frameConstants->lights[i];
			out.positionType = Vec4{
				light.position.x,
				light.position.y,
				light.position.z,
				static_cast<float>(light.type)
			};
			out.directionRange = Vec4{
				light.direction.x,
				light.direction.y,
				light.direction.z,
				light.range
			};
			out.colorIntensity = Vec4{
				light.color.x,
				light.color.y,
				light.color.z,
				light.intensity
			};
			out.spot = Vec4{
				light.innerConeCos,
				light.outerConeCos,
				0.0f,
				0.0f
			};
		}

		if (queue->lightCount > kMaxShaderLights
			&& impl_
			&& !impl_->lightOverflowWarned)
		{
			// Design choice (not directly from the book): the Phase 16 forward
			// shader has a bounded analytic-light array. World extraction remains
			// unbounded; later clustered/deferred lighting may replace this cap.
			NOC_LOG_WARN(
				"Render",
				"Forward light budget exceeded: rendering first %u of %u lights",
				kMaxShaderLights,
				queue->lightCount);
			impl_->lightOverflowWarned = true;
		}

		ID3D12DescriptorHeap* heaps[] = { cbvSrvUav_->Heap() };
		cmd->SetDescriptorHeaps(1, heaps);

		if (queue->instanceCount > 0
			&& EnsureRootSigAndPso_(device, *psoCache_, rm)
			&& EnsureInstanceBuffer_(
				device,
				frameIndex,
				queue->instanceCount))
		{
			auto* dst = reinterpret_cast<GpuInstanceData*>(
				instanceMapped_[frameIndex]);
			for (uint32_t i = 0; i < queue->instanceCount; ++i)
			{
				dst[i].world = queue->instances[i].world;
				dst[i].normalWorld =
					NormalMatrix_(queue->instances[i].world);
			}

			cmd->SetGraphicsRootSignature(rootSig_.Get());
			cmd->SetPipelineState(pso_.Get());
			cmd->SetGraphicsRootDescriptorTable(
				0,
				perFrameCbv_[frameIndex].gpu);
			cmd->SetGraphicsRootDescriptorTable(
				1,
				instanceSrv_[frameIndex].gpu);
			cmd->IASetPrimitiveTopology(
				D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			for (uint32_t i = 0; i < queue->instanceCount; ++i)
			{
				const RenderInstance& instance = queue->instances[i];
				GpuMesh* mesh = EnsureMeshUploaded_(
					device,
					cmd,
					deferred,
					sync,
					frameIndex,
					*rm,
					instance.mesh);
				if (!mesh)
					continue;

				auto vbv = mesh->vertexBuffer.VertexView();
				auto ibv = mesh->indexBuffer.IndexView(
					DXGI_FORMAT_R32_UINT);
				cmd->IASetVertexBuffers(0, 1, &vbv);
				cmd->IASetIndexBuffer(&ibv);

				// SV_InstanceID is local to this draw. StartInstanceLocation only
				// offsets IA per-instance streams, while our transforms live in a
				// StructuredBuffer. Select the correct frame instance explicitly.
				cmd->SetGraphicsRoot32BitConstant(2, i, 0);
				cmd->DrawIndexedInstanced(
					mesh->indexCount,
					1,
					0,
					0,
					0);
			}
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
