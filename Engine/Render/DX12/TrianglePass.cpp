#include "TrianglePass.h"
#include "ShaderCompiler.h"
#include <cstring>

namespace noc
{
	bool TrianglePass::Init(ID3D12Device* device, uint32_t viewportW, uint32_t viewportH)
	{
		if (!device || viewportW == 0 || viewportH == 0)
			return false;

		OnResize(viewportW, viewportH);

		if (!CreateRootSig_(device)) return false;
		if (!CreatePso_(device)) return false;
		if (!CreateVB_(device)) return false;

		NOC_LOG_INFO("Render", "TrianglePass initialized");
		return true;
	}

	void TrianglePass::Shutdown()
	{
		vbUpload_.Reset();
		pso_.Reset();
		rootSig_.Reset();
	}

	void TrianglePass::OnResize(uint32_t viewportW, uint32_t viewportH)
	{
		viewport_.TopLeftX = 0.0f;
		viewport_.TopLeftY = 0.0f;
		viewport_.Width = (float)viewportW;
		viewport_.Height = (float)viewportH;
		viewport_.MinDepth = 0.0f;
		viewport_.MaxDepth = 1.0f;

		scissor_.left = 0;
		scissor_.top = 0;
		scissor_.right = (LONG)viewportW;
		scissor_.bottom = (LONG)viewportH;
	}

	bool TrianglePass::CreateRootSig_(ID3D12Device* device)
	{
		// Empty root signature (no descriptors) but allow IA.
		D3D12_ROOT_SIGNATURE_DESC rs{};
		rs.NumParameters = 0;
		rs.pParameters = nullptr;
		rs.NumStaticSamplers = 0;
		rs.pStaticSamplers = nullptr;
		rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		dx12::ComPtr<ID3DBlob> blob;
		dx12::ComPtr<ID3DBlob> errors;
		HRESULT hr = D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors);
		if (FAILED(hr))
		{
			if (errors)
				NOC_LOG_ERROR("Render", "RootSignature serialize failed: %s", (const char*)errors->GetBufferPointer());
			else
				NOC_LOG_ERROR("Render", "RootSignature serialize failed hr=0x%08X", (unsigned)hr);
			return false;
		}

		return dx12::HrOk(device->CreateRootSignature(
			0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSig_)),
			"CreateRootSignature");
	}

	bool TrianglePass::CreatePso_(ID3D12Device* device)
	{
		static const char* kHlsl = R"(
struct VSInput { float3 pos : POSITION; float4 color : COLOR; };
struct PSInput { float4 pos : SV_POSITION; float4 color : COLOR; };

PSInput VSMain(VSInput v)
{
	PSInput o;
	o.pos = float4(v.pos, 1.0);
	o.color = v.color;
	return o;
}

float4 PSMain(PSInput i) : SV_Target
{
	return i.color;
}
)";

		dx12::ComPtr<ID3DBlob> vs, ps;
		if (!ShaderCompiler::CompileFromString(kHlsl, "VSMain", "vs_5_0", vs)) return false;
		if (!ShaderCompiler::CompileFromString(kHlsl, "PSMain", "ps_5_0", ps)) return false;

		D3D12_INPUT_ELEMENT_DESC input[2]{};
		input[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
		input[1] = { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

		// Build minimal PSO without relying on d3dx12 helpers.
		D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
		pso.pRootSignature = rootSig_.Get();
		pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
		pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
		pso.InputLayout = { input, 2 };
		pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		// Rasterizer default-ish
		D3D12_RASTERIZER_DESC rast{};
		rast.FillMode = D3D12_FILL_MODE_SOLID;
		rast.CullMode = D3D12_CULL_MODE_BACK;
		rast.FrontCounterClockwise = FALSE;
		rast.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		rast.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		rast.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		rast.DepthClipEnable = TRUE;
		rast.MultisampleEnable = FALSE;
		rast.AntialiasedLineEnable = FALSE;
		rast.ForcedSampleCount = 0;
		rast.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
		pso.RasterizerState = rast;

		// Blend default-ish
		D3D12_BLEND_DESC blend{};
		blend.AlphaToCoverageEnable = FALSE;
		blend.IndependentBlendEnable = FALSE;
		for (int i = 0; i < 8; ++i)
		{
			auto& rt = blend.RenderTarget[i];
			rt.BlendEnable = FALSE;
			rt.LogicOpEnable = FALSE;
			rt.SrcBlend = D3D12_BLEND_ONE;
			rt.DestBlend = D3D12_BLEND_ZERO;
			rt.BlendOp = D3D12_BLEND_OP_ADD;
			rt.SrcBlendAlpha = D3D12_BLEND_ONE;
			rt.DestBlendAlpha = D3D12_BLEND_ZERO;
			rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
			rt.LogicOp = D3D12_LOGIC_OP_NOOP;
			rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		}
		pso.BlendState = blend;

		// No depth/stencil for Phase 9.
		D3D12_DEPTH_STENCIL_DESC ds{};
		ds.DepthEnable = FALSE;
		ds.StencilEnable = FALSE;
		pso.DepthStencilState = ds;

		pso.SampleMask = UINT_MAX;
		pso.NumRenderTargets = 1;
		pso.RTVFormats[0] = dx12::kBackBufferFormat;
		pso.SampleDesc.Count = 1;

		return dx12::HrOk(device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&pso_)),
			"CreateGraphicsPipelineState");
	}

	bool TrianglePass::CreateVB_(ID3D12Device* device)
	{
		const Vertex verts[3] = {
			{  0.0f,  0.5f, 0.0f,  1.f, 0.f, 0.f, 1.f },
			{  0.5f, -0.5f, 0.0f,  0.f, 1.f, 0.f, 1.f },
			{ -0.5f, -0.5f, 0.0f,  0.f, 0.f, 1.f, 1.f },
		};

		const UINT vbBytes = (UINT)sizeof(verts);

		auto heap = dx12::HeapProps(D3D12_HEAP_TYPE_UPLOAD);
		auto desc = dx12::BufferDesc(vbBytes);

		if (!dx12::HrOk(device->CreateCommittedResource(
			&heap,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&vbUpload_)),
			"CreateCommittedResource(VB upload)"))
		{
			return false;
		}

		void* mapped = nullptr;
		D3D12_RANGE range{ 0,0 };
		if (!dx12::HrOk(vbUpload_->Map(0, &range, &mapped), "VB.Map"))
			return false;

		std::memcpy(mapped, verts, vbBytes);
		vbUpload_->Unmap(0, nullptr);

		vbView_.BufferLocation = vbUpload_->GetGPUVirtualAddress();
		vbView_.SizeInBytes = vbBytes;
		vbView_.StrideInBytes = sizeof(Vertex);

		return true;
	}

	void TrianglePass::Record(ID3D12GraphicsCommandList* cmd, D3D12_CPU_DESCRIPTOR_HANDLE rtv)
	{
		// Viewport/scissor
		cmd->RSSetViewports(1, &viewport_);
		cmd->RSSetScissorRects(1, &scissor_);

		// Bind RTV + clear
		cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
		const float clear[4] = { 0.02f, 0.02f, 0.05f, 1.0f };
		cmd->ClearRenderTargetView(rtv, clear, 0, nullptr);

		// Pipeline
		cmd->SetGraphicsRootSignature(rootSig_.Get());
		cmd->SetPipelineState(pso_.Get());

		// IA
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmd->IASetVertexBuffers(0, 1, &vbView_);

		// Draw
		cmd->DrawInstanced(3, 1, 0, 0);
	}
}
