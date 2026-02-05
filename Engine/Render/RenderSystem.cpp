#include "RenderSystem.h"

#include "Core/Log.h"
#include "Core/Assert.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <dxgi1_6.h>
#include <d3d12.h>
#include <wrl/client.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

namespace noc
{
    using Microsoft::WRL::ComPtr;

    static inline void SafeCloseHandle(HANDLE& h)
    {
        if (h)
        {
            CloseHandle(h);
            h = nullptr;
        }
    }

    static inline bool HrOk(HRESULT hr, const char* what)
    {
        if (SUCCEEDED(hr))
            return true;

        NOC_LOG_ERROR("Render", "%s failed (hr=0x%08X)", what, (unsigned)hr);
        return false;
    }

    struct RenderSystem::Impl
    {
        static constexpr uint32_t kFrameCount = 2;

        bool debugLayer = false;

        ComPtr<IDXGIFactory6> factory;
        ComPtr<IDXGIAdapter1> adapter;

        ComPtr<ID3D12Device> device;
        ComPtr<ID3D12CommandQueue> queue;

        ComPtr<IDXGISwapChain3> swapChain;

        ComPtr<ID3D12DescriptorHeap> rtvHeap;
        uint32_t rtvDescriptorSize = 0;

        ComPtr<ID3D12Resource> backBuffers[kFrameCount];

        ComPtr<ID3D12CommandAllocator> cmdAlloc[kFrameCount];
        ComPtr<ID3D12GraphicsCommandList> cmdList;

        ComPtr<ID3D12Fence> fence;
        uint64_t fenceValues[kFrameCount] = {}; // indexed by back buffer
        HANDLE fenceEvent = nullptr;

        uint32_t frameIndex = 0; // current swapchain backbuffer index

        uint32_t clientW = 0;
        uint32_t clientH = 0;
        HWND hwnd = nullptr;

        bool frameOpen = false;

        // Track backbuffer state to avoid barrier mismatch
        D3D12_RESOURCE_STATES bbState[kFrameCount] = {
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_PRESENT
        };

        void LogDeviceRemoved_(const char* where) const
        {
            if (!device) return;
            HRESULT reason = device->GetDeviceRemovedReason();
            if (reason != S_OK)
                NOC_LOG_ERROR("Render", "Device removed reason at %s: hr=0x%08X", where, (unsigned)reason);
        }

        bool CreateFactory_()
        {
#if defined(_DEBUG)
            UINT flags = 0;
            if (debugLayer)
                flags |= DXGI_CREATE_FACTORY_DEBUG;
            return HrOk(CreateDXGIFactory2(flags, IID_PPV_ARGS(&factory)), "CreateDXGIFactory2");
#else
            return HrOk(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "CreateDXGIFactory2");
#endif
        }

        bool PickAdapter_()
        {
            ComPtr<IDXGIAdapter1> best;

            for (UINT i = 0;; ++i)
            {
                ComPtr<IDXGIAdapter1> a;
                if (factory->EnumAdapters1(i, &a) == DXGI_ERROR_NOT_FOUND)
                    break;

                DXGI_ADAPTER_DESC1 desc{};
                a->GetDesc1(&desc);

                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                    continue;

                if (SUCCEEDED(D3D12CreateDevice(a.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
                {
                    best = a;
                    break;
                }
            }

            if (!best)
            {
                NOC_LOG_ERROR("Render", "No suitable hardware DX12 adapter found");
                return false;
            }

            adapter = best;
            return true;
        }

        bool CreateDevice_()
        {
#if defined(_DEBUG)
            if (debugLayer)
            {
                ComPtr<ID3D12Debug> dbg;
                if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dbg))))
                {
                    dbg->EnableDebugLayer();
                    NOC_LOG_INFO("Render", "D3D12 debug layer enabled");
                }
                else
                {
                    NOC_LOG_WARN("Render", "D3D12 debug layer requested but not available");
                }
            }
#endif
            return HrOk(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)), "D3D12CreateDevice");
        }

        bool CreateQueue_()
        {
            D3D12_COMMAND_QUEUE_DESC q{};
            q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            q.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
            q.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
            q.NodeMask = 0;

            return HrOk(device->CreateCommandQueue(&q, IID_PPV_ARGS(&queue)), "CreateCommandQueue");
        }

        bool CreateSwapChain_(HWND h, uint32_t w, uint32_t hh)
        {
            hwnd = h;
            clientW = w;
            clientH = hh;

            DXGI_SWAP_CHAIN_DESC1 sc{};
            sc.BufferCount = kFrameCount;
            sc.Width = w;
            sc.Height = hh;
            sc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            sc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            sc.SampleDesc.Count = 1;

            ComPtr<IDXGISwapChain1> sc1;
            if (!HrOk(factory->CreateSwapChainForHwnd(
                queue.Get(),
                hwnd,
                &sc,
                nullptr,
                nullptr,
                &sc1), "CreateSwapChainForHwnd"))
            {
                return false;
            }

            factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

            if (!HrOk(sc1.As(&swapChain), "SwapChain1.As(SwapChain3)"))
                return false;

            frameIndex = swapChain->GetCurrentBackBufferIndex();
            return true;
        }

        bool CreateRtvHeapAndViews_()
        {
            D3D12_DESCRIPTOR_HEAP_DESC hd{};
            hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            hd.NumDescriptors = kFrameCount;
            hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

            if (!HrOk(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&rtvHeap)), "CreateDescriptorHeap(RTV)"))
                return false;

            rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

            D3D12_CPU_DESCRIPTOR_HANDLE base = rtvHeap->GetCPUDescriptorHandleForHeapStart();

            for (uint32_t i = 0; i < kFrameCount; ++i)
            {
                if (!HrOk(swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffers[i])), "SwapChain.GetBuffer"))
                    return false;

                D3D12_CPU_DESCRIPTOR_HANDLE h = base;
                h.ptr += (SIZE_T)i * (SIZE_T)rtvDescriptorSize;
                device->CreateRenderTargetView(backBuffers[i].Get(), nullptr, h);

                bbState[i] = D3D12_RESOURCE_STATE_PRESENT;
            }

            return true;
        }

        bool CreateCommands_()
        {
            for (uint32_t i = 0; i < kFrameCount; ++i)
            {
                if (!HrOk(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc[i])),
                    "CreateCommandAllocator"))
                {
                    return false;
                }
            }

            // Create list using allocator for current frameIndex.
            if (!HrOk(device->CreateCommandList(
                0,
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                cmdAlloc[frameIndex].Get(),
                nullptr,
                IID_PPV_ARGS(&cmdList)), "CreateCommandList"))
            {
                return false;
            }

            // Close it; BeginFrame will Reset it.
            if (!HrOk(cmdList->Close(), "cmdList->Close (initial)"))
                return false;

            return true;
        }

        bool CreateFence_()
        {
            // Create fence with initial value fenceValues[frameIndex] (0).
            if (!HrOk(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)), "CreateFence"))
                return false;

            for (uint32_t i = 0; i < kFrameCount; ++i)
                fenceValues[i] = 0;

            fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            if (!fenceEvent)
            {
                NOC_LOG_ERROR("Render", "CreateEvent failed (err=%lu)", GetLastError());
                return false;
            }

            // Match the sample: bump the current frame's fence value once
            // so first MoveToNextFrame() has currentFenceValue >= 1.
            fenceValues[frameIndex] = 1;
            return true;
        }

        void MoveToNextFrame_()
        {
            // Signal for the frame we just submitted.
            const uint64_t currentFenceValue = fenceValues[frameIndex];
            HRESULT hr = queue->Signal(fence.Get(), currentFenceValue);
            if (FAILED(hr))
            {
                NOC_LOG_ERROR("Render", "MoveToNextFrame_: Signal failed (hr=0x%08X)", (unsigned)hr);
                LogDeviceRemoved_("MoveToNextFrame_:Signal");
                return;
            }

            // Update to the next back buffer.
            frameIndex = swapChain->GetCurrentBackBufferIndex();

            // Wait until that back buffer is ready.
            if (fence->GetCompletedValue() < fenceValues[frameIndex])
            {
                hr = fence->SetEventOnCompletion(fenceValues[frameIndex], fenceEvent);
                if (FAILED(hr))
                {
                    NOC_LOG_ERROR("Render", "MoveToNextFrame_: SetEventOnCompletion failed (hr=0x%08X)", (unsigned)hr);
                    LogDeviceRemoved_("MoveToNextFrame_:SetEventOnCompletion");
                    return;
                }
                WaitForSingleObjectEx(fenceEvent, INFINITE, FALSE);
            }

            // Fence value for the next time we render this buffer.
            fenceValues[frameIndex] = currentFenceValue + 1;
        }

        void WaitForGpu_()
        {
            // Flush: signal and wait using current frameIndex
            const uint64_t v = fenceValues[frameIndex];
            queue->Signal(fence.Get(), v);
            fence->SetEventOnCompletion(v, fenceEvent);
            WaitForSingleObjectEx(fenceEvent, INFINITE, FALSE);
            fenceValues[frameIndex] = v + 1;
        }

        void Shutdown_()
        {
            if (device && queue && fence && fenceEvent)
            {
                WaitForGpu_();
            }

            SafeCloseHandle(fenceEvent);

            for (uint32_t i = 0; i < kFrameCount; ++i)
            {
                backBuffers[i].Reset();
                cmdAlloc[i].Reset();
            }

            cmdList.Reset();
            rtvHeap.Reset();
            swapChain.Reset();
            queue.Reset();
            fence.Reset();
            device.Reset();
            adapter.Reset();
            factory.Reset();

            hwnd = nullptr;
            clientW = clientH = 0;
        }
    };

    bool RenderSystem::Init(bool enableDebugLayer)
    {
        if (impl_)
            return true;

        impl_ = new Impl();
        impl_->debugLayer = enableDebugLayer;

        NOC_LOG_INFO("Render", "RenderSystem initialized (awaiting AttachToWindow)");
        return true;
    }

    void RenderSystem::Shutdown()
    {
        if (!impl_)
            return;

        impl_->Shutdown_();
        delete impl_;
        impl_ = nullptr;

        NOC_LOG_INFO("Render", "RenderSystem shutdown");
    }

    bool RenderSystem::AttachToWindow(void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight)
    {
        if (!impl_)
            return false;

        if (!nativeHwnd || clientWidth == 0 || clientHeight == 0)
        {
            NOC_LOG_ERROR("Render", "AttachToWindow invalid args");
            return false;
        }

        if (!impl_->CreateFactory_()) return false;
        if (!impl_->PickAdapter_()) return false;
        if (!impl_->CreateDevice_()) return false;
        if (!impl_->CreateQueue_()) return false;
        if (!impl_->CreateSwapChain_((HWND)nativeHwnd, clientWidth, clientHeight)) return false;
        if (!impl_->CreateRtvHeapAndViews_()) return false;
        if (!impl_->CreateCommands_()) return false;
        if (!impl_->CreateFence_()) return false;

        NOC_LOG_INFO("Render", "DX12 ready (%ux%u, buffers=%u)", clientWidth, clientHeight, Impl::kFrameCount);
        return true;
    }

    void RenderSystem::BeginFrame()
    {
        if (!impl_ || !impl_->device) return;
        if (impl_->device->GetDeviceRemovedReason() != S_OK) return;

        if (impl_->frameOpen)
        {
            NOC_LOG_ERROR("Render", "BeginFrame called while a frame is already open. Ignoring.");
            return;
        }
        impl_->frameOpen = true;

        // Reset allocator for current backbuffer index.
        HRESULT hr = impl_->cmdAlloc[impl_->frameIndex]->Reset();
        if (FAILED(hr))
        {
            NOC_LOG_ERROR("Render", "cmdAlloc->Reset failed (hr=0x%08X)", (unsigned)hr);
            impl_->LogDeviceRemoved_("cmdAlloc->Reset");
            impl_->frameOpen = false;
            return;
        }

        hr = impl_->cmdList->Reset(impl_->cmdAlloc[impl_->frameIndex].Get(), nullptr);
        if (FAILED(hr))
        {
            NOC_LOG_ERROR("Render", "cmdList->Reset failed (hr=0x%08X)", (unsigned)hr);
            impl_->LogDeviceRemoved_("cmdList->Reset");
            impl_->frameOpen = false;
            return;
        }
    }

    void RenderSystem::EndFramePresent()
    {
        if (!impl_ || !impl_->device) return;
        if (impl_->device->GetDeviceRemovedReason() != S_OK) return;

        if (!impl_->frameOpen)
        {
            NOC_LOG_ERROR("Render", "EndFramePresent called with no open frame. Ignoring.");
            return;
        }

        struct FrameCloseGuard { bool& open; ~FrameCloseGuard() { open = false; } } guard{ impl_->frameOpen };

        const uint32_t i = impl_->frameIndex;

        // RTV handle
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = impl_->rtvHeap->GetCPUDescriptorHandleForHeapStart();
        rtv.ptr += (SIZE_T)i * (SIZE_T)impl_->rtvDescriptorSize;

        auto Transition = [&](D3D12_RESOURCE_STATES to)
            {
                if (impl_->bbState[i] == to) return;

                D3D12_RESOURCE_BARRIER b{};
                b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                b.Transition.pResource = impl_->backBuffers[i].Get();
                b.Transition.StateBefore = impl_->bbState[i];
                b.Transition.StateAfter = to;
                b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

                impl_->cmdList->ResourceBarrier(1, &b);
                impl_->bbState[i] = to;
            };

        Transition(D3D12_RESOURCE_STATE_RENDER_TARGET);

        const float clear[4] = { 0.02f, 0.02f, 0.05f, 1.0f };
        impl_->cmdList->ClearRenderTargetView(rtv, clear, 0, nullptr);

        Transition(D3D12_RESOURCE_STATE_PRESENT);

        HRESULT hr = impl_->cmdList->Close();
        if (FAILED(hr))
        {
            NOC_LOG_ERROR("Render", "cmdList->Close failed (hr=0x%08X)", (unsigned)hr);
            impl_->LogDeviceRemoved_("cmdList->Close");
            return;
        }

        ID3D12CommandList* lists[] = { impl_->cmdList.Get() };
        impl_->queue->ExecuteCommandLists(1, lists);

        hr = impl_->swapChain->Present(1, 0);
        if (FAILED(hr))
        {
            NOC_LOG_ERROR("Render", "swapChain->Present failed (hr=0x%08X)", (unsigned)hr);
            impl_->LogDeviceRemoved_("Present");
            return;
        }

        impl_->MoveToNextFrame_();
    }

} // namespace noc
