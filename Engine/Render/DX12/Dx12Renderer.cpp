#include "Dx12Renderer.h"

#include "Resources/ResourceManager.h"
#include "Render/RenderQueue.h"

namespace noc
{
	bool Dx12Renderer::Init(bool enableDebugLayer)
	{
		if (inited_)
			return true;

		if (!device_.Init(enableDebugLayer))
			return false;

		if (!sync_.Init(device_.Device()))
			return false;

		inited_ = true;
		NOC_LOG_INFO("Render", "Dx12Renderer initialized (awaiting AttachToWindow)");
		return true;
	}

	void Dx12Renderer::Shutdown()
	{
		if (!inited_)
			return;

		if (attached_)
			sync_.WaitForGpu(device_.Queue());

		deferred_.Clear();

		meshPass_.Shutdown(deferred_, sync_.CompletedValue());

		cmdList_.Reset();
		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
			cmdAlloc_[i].Reset();

		psoCache_.Clear();

		cbvSrvUavHeap_.Shutdown();
		samplerHeap_.Shutdown();

		swap_.Shutdown();
		sync_.Shutdown();
		device_.Shutdown();

		inited_ = false;
		attached_ = false;
		frameQueue_ = nullptr;

		NOC_LOG_INFO("Render", "Dx12Renderer shutdown");
	}

	bool Dx12Renderer::AttachToWindow(void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight)
	{
		if (!inited_ || attached_ || clientWidth == 0 || clientHeight == 0)
			return false;

		if (!swap_.Init(device_.Factory(), device_.Queue(), nativeHwnd, clientWidth, clientHeight))
			return false;

		if (!swap_.CreateRtvHeapAndViews(device_.Device()))
			return false;

		frameIndex_ = swap_.FrameIndex();

		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
		{
			if (!dx12::HrOk(device_.Device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc_[i])),
				"CreateCommandAllocator"))
			{
				return false;
			}
		}

		if (!dx12::HrOk(device_.Device()->CreateCommandList(
			0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc_[frameIndex_].Get(), nullptr, IID_PPV_ARGS(&cmdList_)),
			"CreateCommandList"))
		{
			return false;
		}
		dx12::HrOk(cmdList_->Close(), "cmdList->Close (initial)");

		if (!cbvSrvUavHeap_.Init(device_.Device(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024, true))
			return false;
		if (!samplerHeap_.Init(device_.Device(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 64, true))
			return false;

		if (!meshPass_.Init(device_.Device(), cbvSrvUavHeap_, samplerHeap_, psoCache_))
			return false;

		attached_ = true;
		NOC_LOG_INFO("Render", "Dx12Renderer attached (%ux%u)", clientWidth, clientHeight);
		return true;
	}

	bool Dx12Renderer::ResizeAttachedWindow(uint32_t clientWidth, uint32_t clientHeight)
	{
		if (!attached_)
			return false;
		if (clientWidth == 0 || clientHeight == 0)
			return true;
		if (clientWidth == swap_.Width() && clientHeight == swap_.Height())
			return true;
		if (frameOpen_)
		{
			NOC_LOG_WARN("Render", "ResizeAttachedWindow deferred because a frame is open");
			return false;
		}

		sync_.WaitForGpu(device_.Queue());
		if (!swap_.Resize(device_.Device(), clientWidth, clientHeight))
			return false;

		frameIndex_ = swap_.FrameIndex();
		NOC_LOG_INFO("Render", "Dx12Renderer resized (%ux%u)", clientWidth, clientHeight);
		return true;
	}

	void Dx12Renderer::BeginFrame()
	{
		if (!attached_ || swap_.Width() == 0 || swap_.Height() == 0)
			return;

		if (device_.Device()->GetDeviceRemovedReason() != S_OK)
			return;

		if (frameOpen_)
		{
			NOC_LOG_ERROR("Render", "BeginFrame called while frame is already open");
			return;
		}
		frameOpen_ = true;

		deferred_.Collect(sync_.CompletedValue());

		if (FAILED(cmdAlloc_[frameIndex_]->Reset()))
		{
			LogDeviceRemoved_("cmdAlloc->Reset");
			frameOpen_ = false;
			return;
		}

		if (FAILED(cmdList_->Reset(cmdAlloc_[frameIndex_].Get(), nullptr)))
		{
			LogDeviceRemoved_("cmdList->Reset");
			frameOpen_ = false;
			return;
		}

		swap_.TransitionTo(cmdList_.Get(), frameIndex_, D3D12_RESOURCE_STATE_RENDER_TARGET);
	}

	void Dx12Renderer::EndFramePresent()
	{
		if (!attached_ || swap_.Width() == 0 || swap_.Height() == 0)
			return;

		if (device_.Device()->GetDeviceRemovedReason() != S_OK)
			return;

		if (!frameOpen_)
			return;
		struct Guard { bool& b; ~Guard() { b = false; } } g{ frameOpen_ };

		meshPass_.Record(device_.Device(), cmdList_.Get(), swap_, sync_, frameIndex_, deferred_, rm_, frameQueue_);

		swap_.TransitionTo(cmdList_.Get(), frameIndex_, D3D12_RESOURCE_STATE_PRESENT);

		if (FAILED(cmdList_->Close()))
		{
			LogDeviceRemoved_("cmdList->Close");
			return;
		}

		ID3D12CommandList* lists[] = { cmdList_.Get() };
		device_.Queue()->ExecuteCommandLists(1, lists);

		swap_.Present();

		sync_.MoveToNextFrame(device_.Queue(), swap_.SwapChain(), frameIndex_);
		swap_.UpdateFrameIndex(frameIndex_);
	}

	void Dx12Renderer::LogDeviceRemoved_(const char* where)
	{
		HRESULT hr = device_.Device()->GetDeviceRemovedReason();
		NOC_LOG_ERROR("Render", "Device removed at %s (hr=0x%08X)", where, (unsigned)hr);
	}
}
