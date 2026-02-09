#include "Dx12Device.h"

namespace noc
{
	bool Dx12Device::Init(bool enableDebugLayer)
	{
		debugLayer_ = enableDebugLayer;

		if (!CreateFactory_()) return false;
		if (!PickAdapter_()) return false;
		if (!CreateDevice_()) return false;
		if (!CreateQueue_()) return false;

		return true;
	}

	void Dx12Device::Shutdown()
	{
		queue_.Reset();
		device_.Reset();
		adapter_.Reset();
		factory_.Reset();
	}

	bool Dx12Device::CreateFactory_()
	{
		UINT flags = 0;

		if (debugLayer_)
		{
			dx12::ComPtr<ID3D12Debug> dbg;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dbg))))
			{
				dbg->EnableDebugLayer();
				flags |= DXGI_CREATE_FACTORY_DEBUG;
				NOC_LOG_INFO("Render", "D3D12 debug layer enabled");
			}
			else
			{
				NOC_LOG_WARN("Render", "Failed to enable D3D12 debug layer");
			}
		}

		return dx12::HrOk(CreateDXGIFactory2(flags, IID_PPV_ARGS(&factory_)), "CreateDXGIFactory2");
	}

	bool Dx12Device::PickAdapter_()
	{
		for (UINT i = 0; ; ++i)
		{
			dx12::ComPtr<IDXGIAdapter1> a;
			if (factory_->EnumAdapters1(i, &a) == DXGI_ERROR_NOT_FOUND)
				break;

			DXGI_ADAPTER_DESC1 desc{};
			a->GetDesc1(&desc);

			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
				continue;

			if (SUCCEEDED(D3D12CreateDevice(a.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)))
			{
				adapter_ = a;
				NOC_LOG_INFO("Render", "DX12 adapter selected");
				return true;
			}
		}

		NOC_LOG_ERROR("Render", "No suitable hardware adapter found");
		return false;
	}

	bool Dx12Device::CreateDevice_()
	{
		return dx12::HrOk(D3D12CreateDevice(adapter_.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_)), "D3D12CreateDevice");
	}

	bool Dx12Device::CreateQueue_()
	{
		D3D12_COMMAND_QUEUE_DESC q{};
		q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		q.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		q.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		q.NodeMask = 0;

		return dx12::HrOk(device_->CreateCommandQueue(&q, IID_PPV_ARGS(&queue_)), "CreateCommandQueue");
	}
}
