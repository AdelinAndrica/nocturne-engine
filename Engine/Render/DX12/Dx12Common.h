#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <dxgi1_6.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include "d3dx12.h"

#include "Core/Log.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace noc::dx12
{
	using Microsoft::WRL::ComPtr;

	inline void SafeCloseHandle(HANDLE& h)
	{
		if (h)
		{
			CloseHandle(h);
			h = nullptr;
		}
	}

	inline bool HrOk(HRESULT hr, const char* what)
	{
		if (SUCCEEDED(hr))
			return true;

		NOC_LOG_ERROR("Render", "%s failed (hr=0x%08X)", what, (unsigned)hr);
		return false;
	}

	inline D3D12_RESOURCE_BARRIER TransitionBarrier(
		ID3D12Resource* res,
		D3D12_RESOURCE_STATES before,
		D3D12_RESOURCE_STATES after)
	{
		D3D12_RESOURCE_BARRIER b{};
		b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		b.Transition.pResource = res;
		b.Transition.StateBefore = before;
		b.Transition.StateAfter = after;
		b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		return b;
	}

	inline D3D12_HEAP_PROPERTIES HeapProps(D3D12_HEAP_TYPE type)
	{
		D3D12_HEAP_PROPERTIES p{};
		p.Type = type;
		p.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		p.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		p.CreationNodeMask = 1;
		p.VisibleNodeMask = 1;
		return p;
	}

	inline D3D12_RESOURCE_DESC BufferDesc(UINT64 bytes)
	{
		D3D12_RESOURCE_DESC d{};
		d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		d.Alignment = 0;
		d.Width = bytes;
		d.Height = 1;
		d.DepthOrArraySize = 1;
		d.MipLevels = 1;
		d.Format = DXGI_FORMAT_UNKNOWN;
		d.SampleDesc.Count = 1;
		d.SampleDesc.Quality = 0;
		d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		d.Flags = D3D12_RESOURCE_FLAG_NONE;
		return d;
	}

	// Common formats for our baseline.
	static constexpr DXGI_FORMAT kBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	static constexpr uint32_t kFrameCount = 2;
}
