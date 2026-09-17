#include "Dx12SwapChain.h"

namespace noc
{
	bool Dx12SwapChain::Init(IDXGIFactory6* factory, ID3D12CommandQueue* queue, void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight)
	{
		if (!factory || !queue || !nativeHwnd || clientWidth == 0 || clientHeight == 0)
			return false;

		width_ = clientWidth;
		height_ = clientHeight;

		DXGI_SWAP_CHAIN_DESC1 sc{};
		sc.BufferCount = dx12::kFrameCount;
		sc.Width = clientWidth;
		sc.Height = clientHeight;
		sc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		sc.SampleDesc.Count = 1;

		dx12::ComPtr<IDXGISwapChain1> sc1;
		if (!dx12::HrOk(factory->CreateSwapChainForHwnd(queue, (HWND)nativeHwnd, &sc, nullptr, nullptr, &sc1), "CreateSwapChainForHwnd"))
			return false;

		factory->MakeWindowAssociation((HWND)nativeHwnd, DXGI_MWA_NO_ALT_ENTER);

		if (!dx12::HrOk(sc1.As(&swapChain_), "SwapChain1.As(SwapChain3)"))
			return false;

		frameIndex_ = swapChain_->GetCurrentBackBufferIndex();
		return true;
	}

	void Dx12SwapChain::Shutdown()
	{
		depthStencil_.Reset();
		dsvHeap_.Reset();

		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
			backBuffers_[i].Reset();

		rtvHeap_.Reset();
		swapChain_.Reset();
		width_ = height_ = 0;
		frameIndex_ = 0;
		rtvDescriptorSize_ = 0;
	}

	bool Dx12SwapChain::CreateBackBufferViews_(ID3D12Device* device)
	{
		if (!device || !swapChain_ || !rtvHeap_)
			return false;

		D3D12_CPU_DESCRIPTOR_HANDLE base = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
		{
			if (!dx12::HrOk(swapChain_->GetBuffer(i, IID_PPV_ARGS(&backBuffers_[i])), "SwapChain.GetBuffer"))
				return false;

			D3D12_CPU_DESCRIPTOR_HANDLE h = base;
			h.ptr += (SIZE_T)i * (SIZE_T)rtvDescriptorSize_;
			device->CreateRenderTargetView(backBuffers_[i].Get(), nullptr, h);
			bbState_[i] = D3D12_RESOURCE_STATE_PRESENT;
		}
		return true;
	}

	bool Dx12SwapChain::CreateDepthStencil_(ID3D12Device* device)
	{
		if (!device || width_ == 0 || height_ == 0)
			return false;

		if (!dsvHeap_)
		{
			D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
			heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
			heapDesc.NumDescriptors = 1;
			heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			if (!dx12::HrOk(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&dsvHeap_)), "CreateDescriptorHeap(DSV)"))
				return false;
		}

		depthStencil_.Reset();

		D3D12_HEAP_PROPERTIES heapProps{};
		heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProps.CreationNodeMask = 1;
		heapProps.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Alignment = 0;
		resourceDesc.Width = width_;
		resourceDesc.Height = height_;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = kDepthFormat;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.SampleDesc.Quality = 0;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE clearValue{};
		clearValue.Format = kDepthFormat;
		clearValue.DepthStencil.Depth = 1.0f;
		clearValue.DepthStencil.Stencil = 0;

		if (!dx12::HrOk(device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&clearValue,
			IID_PPV_ARGS(&depthStencil_)), "CreateCommittedResource(DepthStencil)"))
		{
			return false;
		}

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
		dsvDesc.Format = kDepthFormat;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		device->CreateDepthStencilView(depthStencil_.Get(), &dsvDesc, dsvHeap_->GetCPUDescriptorHandleForHeapStart());
		return true;
	}

	bool Dx12SwapChain::CreateRtvHeapAndViews(ID3D12Device* device)
	{
		if (!device || !swapChain_)
			return false;

		D3D12_DESCRIPTOR_HEAP_DESC hd{};
		hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		hd.NumDescriptors = dx12::kFrameCount;
		hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		if (!dx12::HrOk(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&rtvHeap_)), "CreateDescriptorHeap(RTV)"))
			return false;

		rtvDescriptorSize_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		if (!CreateBackBufferViews_(device))
			return false;

		// Luna Chapter 4 grounds the depth-buffer lifecycle alongside the
		// swap-chain/back-buffer size. Exact ownership here is a Nocturne design choice.
		return CreateDepthStencil_(device);
	}

	bool Dx12SwapChain::Resize(ID3D12Device* device, uint32_t clientWidth, uint32_t clientHeight)
	{
		if (!device || !swapChain_)
			return false;
		if (clientWidth == 0 || clientHeight == 0)
			return true;
		if (clientWidth == width_ && clientHeight == height_)
			return true;

		depthStencil_.Reset();
		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
			backBuffers_[i].Reset();

		if (!dx12::HrOk(swapChain_->ResizeBuffers(dx12::kFrameCount, clientWidth, clientHeight,
			DXGI_FORMAT_R8G8B8A8_UNORM, 0), "SwapChain.ResizeBuffers"))
			return false;

		width_ = clientWidth;
		height_ = clientHeight;
		frameIndex_ = swapChain_->GetCurrentBackBufferIndex();

		if (!CreateBackBufferViews_(device))
			return false;
		return CreateDepthStencil_(device);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE Dx12SwapChain::CurrentRtv(uint32_t frameIndex) const
	{
		D3D12_CPU_DESCRIPTOR_HANDLE h{};
		if (!rtvHeap_)
			return h;

		h = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
		h.ptr += (SIZE_T)frameIndex * (SIZE_T)rtvDescriptorSize_;
		return h;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE Dx12SwapChain::DepthStencilView() const
	{
		D3D12_CPU_DESCRIPTOR_HANDLE h{};
		if (!dsvHeap_ || !depthStencil_)
			return h;
		return dsvHeap_->GetCPUDescriptorHandleForHeapStart();
	}

	void Dx12SwapChain::TransitionTo(ID3D12GraphicsCommandList* cmd, uint32_t frameIndex, D3D12_RESOURCE_STATES to)
	{
		if (!cmd || frameIndex >= dx12::kFrameCount || !backBuffers_[frameIndex])
			return;

		if (bbState_[frameIndex] == to)
			return;

		D3D12_RESOURCE_BARRIER b = dx12::TransitionBarrier(backBuffers_[frameIndex].Get(), bbState_[frameIndex], to);
		cmd->ResourceBarrier(1, &b);
		bbState_[frameIndex] = to;
	}

	void Dx12SwapChain::Present()
	{
		if (!swapChain_)
			return;
		swapChain_->Present(1, 0);
	}
}
