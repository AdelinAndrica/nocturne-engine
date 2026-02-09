#pragma once
#include "Dx12Common.h"

namespace noc
{
	class Dx12Device
	{
	public:
		bool Init(bool enableDebugLayer);

		ID3D12Device* Device() const { return device_.Get(); }
		ID3D12CommandQueue* Queue() const { return queue_.Get(); }
		IDXGIFactory6* Factory() const { return factory_.Get(); }

		void Shutdown();

	private:
		bool CreateFactory_();
		bool PickAdapter_();
		bool CreateDevice_();
		bool CreateQueue_();

	private:
		bool debugLayer_ = false;

		dx12::ComPtr<IDXGIFactory6> factory_;
		dx12::ComPtr<IDXGIAdapter1> adapter_;
		dx12::ComPtr<ID3D12Device> device_;
		dx12::ComPtr<ID3D12CommandQueue> queue_;
	};
}
