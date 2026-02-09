#pragma once
#include "Dx12Common.h"

namespace noc
{
	class TrianglePass
	{
	public:
		bool Init(ID3D12Device* device, uint32_t viewportW, uint32_t viewportH);
		void Shutdown();

		void OnResize(uint32_t viewportW, uint32_t viewportH);

		void Record(
			ID3D12GraphicsCommandList* cmd,
			D3D12_CPU_DESCRIPTOR_HANDLE rtv);

	private:
		struct Vertex
		{
			float px, py, pz;
			float r, g, b, a;
		};

		bool CreateRootSig_(ID3D12Device* device);
		bool CreatePso_(ID3D12Device* device);
		bool CreateVB_(ID3D12Device* device);

	private:
		dx12::ComPtr<ID3D12RootSignature> rootSig_;
		dx12::ComPtr<ID3D12PipelineState> pso_;
		dx12::ComPtr<ID3D12Resource> vbUpload_;
		D3D12_VERTEX_BUFFER_VIEW vbView_{};

		D3D12_VIEWPORT viewport_{};
		D3D12_RECT scissor_{};
	};
}
