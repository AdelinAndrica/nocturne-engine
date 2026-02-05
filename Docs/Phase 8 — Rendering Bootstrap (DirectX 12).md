# Phase 8 — Rendering Bootstrap (DirectX 12)

> **Status:** READY FOR IMPLEMENTATION ⏳  
> **Scope:** Minimal DX12 backend: device, queue, swapchain, RTV heap, per-frame alloc/list, fences; clear + present every frame  
> **Depends on:** Phase 2 (Win32 window + loop), Phase 1 (log/assert/time/memory), Phase 7 (Engine::AttachWindow pattern)   
> **Architecture alignment:** “RENDER → RHI” and “Dx12Device/Dx12SwapChain/Dx12CommandQueue/Fence” in the Full Architecture Diagram :contentReference[oaicite:1]{index=1}  
> **Roadmap alignment:** Phase 8 entry in `nocturne_engine_architecture.md` :contentReference[oaicite:2]{index=2}  

This document is the **authoritative design + implementation reference** for Phase 8 of Nocturne Engine.

---

## 1) Phase name + objective

**Objective:** Add an engine-owned **RenderSystem** (DX12 bootstrap) that:

1. Creates a D3D12 **device** (hardware adapter), **command queue**, and **swap chain**.
2. Allocates an **RTV descriptor heap** and RTVs for the back buffers.
3. Maintains **per-frame command allocators** and a single **graphics command list**.
4. Uses **fences** to safely reuse per-frame allocators and present.
5. Integrates into the engine loop:
   - `Engine::BeginFrame()` resets command structures
   - `Engine::EndFrame()` clears the current back buffer and presents

Result: a window that clears to a solid color every frame (no triangle yet).

---

## 2) Key concepts from the books (book-grounded)

- **Engine owns the main loop and frame boundaries**; rendering is a subsystem called deterministically each frame. (Main loop orchestration pattern already established in Phase 2/7.)   
- **Systems should finalize/publish on the main thread**; Phase 4/5 established this “do work then publish at a known point” shape (we apply the same discipline to GPU submission/present). :contentReference[oaicite:4]{index=4}  
- **Layering + dependency rules:** Render sits below gameplay and uses platform only through narrow handles (HWND as `void*`).   

**Design choice (not directly from the book):** This phase is a single-threaded render path (main-thread submits). A dedicated render thread is deferred.

---

## 3) What we implement now (tight scope)

### Render bootstrap only
- DXGI factory + adapter selection
- D3D12 device
- command queue
- swap chain (flip-discard, 2 buffers)
- RTV heap + back buffer RTVs
- per-frame command allocator
- one command list
- fence + event
- per-frame:
  - transition Present→RT
  - clear RTV
  - transition RT→Present
  - execute, present, signal fence

### Not included (explicitly out of scope)
- Resize handling (we store client size for later; no swapchain resize path yet)
- Root signatures, PSOs, shaders
- GPU resource allocator, descriptor allocators beyond RTV heap
- Render graph / frame graph

---

## 4) Implementation steps

1. Add `Engine/Render/RenderSystem.h/.cpp` (DX12 bootstrap hidden in `.cpp`).
2. Extend `WinWindow` to store and expose client size (`ClientWidth/ClientHeight`) and update on `WM_SIZE`.
3. Modify `Engine`:
   - own `RenderSystem render_;`
   - init in `Engine::Init()` (lightweight; no HWND)
   - attach in `Engine::AttachWindow()` (create device/swapchain using HWND)
   - call `render_.BeginFrame()` in `Engine::BeginFrame()`
   - call `render_.EndFramePresent()` in `Engine::EndFrame()`
4. Verification: see clear color + present continuously; clean shutdown; no D3D12 debug errors (when enabled).

---

## 5) Verification checklist (Phase 8 done when…)

- [ ] Window opens and continuously clears to a solid color.
- [ ] GPU validation/debug layer can be toggled (debug builds).
- [ ] No device removed / DXGI errors during normal run.
- [ ] Closing window exits cleanly (fence wait + release is safe).
- [ ] Logs show render init success and orderly shutdown.

---

## 6) Common pitfalls

- Reusing a command allocator before the GPU is done (missing fence wait).
- Forgetting resource barriers Present↔RenderTarget.
- Creating swap chain without the correct HWND or wrong format.
- Clearing without setting the correct RTV handle for the current back buffer.

---

## 7) Next chat handoff (ONLY what you should say/bring next)

> “Phase 8 is implemented. DX12 device/queue/swapchain/RTV heap/fences are working. The window clears and presents every frame. Here are logs + a screenshot. Start Phase 9 — Rendering Engine Foundation (GPU resources + PSO/shaders + first triangle).”

---

# Implementations (Phase 8)

Below are **all files implemented/modified in Phase 8**, each with:
- `path/to/file`
- full implementation

---

## `Engine/Render/RenderSystem.h`

```cpp
#pragma once
#include <cstdint>

namespace noc
{
	// Minimal render bootstrap system (DX12 implementation lives in .cpp).
	// Public header stays platform-agnostic: only uses void* for HWND.
	class RenderSystem
	{
	public:
		RenderSystem() = default;

		// Engine lifecycle.
		bool Init(bool enableDebugLayer);
		void Shutdown();

		// Must be called after window creation (needs HWND + client size).
		bool AttachToWindow(void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight);

		// Per-frame.
		void BeginFrame();
		void EndFramePresent(); // clears + presents

	private:
		struct Impl;
		Impl* impl_ = nullptr;
	};
}
````

---

## `Engine/Render/RenderSystem.cpp`

```cpp
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
```

---

## `Engine/Platform/Win32/WinWindow.h` (MODIFIED: expose client size)

```cpp
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstdint>

namespace noc
{
	namespace platform
	{
		struct RawMouseEvent
		{
			int dx = 0;
			int dy = 0;
			int wheel = 0;
			uint16_t buttonDownMask = 0; // bit0=L, bit1=R, bit2=M
			uint16_t buttonUpMask = 0;
		};

		struct RawKeyboardEvent
		{
			uint16_t vkey = 0;
			bool down = false;
		};

		struct IRawInputSink
		{
			virtual ~IRawInputSink() = default;
			virtual void OnRawMouse(const RawMouseEvent& e) = 0;
			virtual void OnRawKeyboard(const RawKeyboardEvent& e) = 0;
			virtual void OnFocusLost() = 0;
		};
	}

	struct WinWindowDesc
	{
		const wchar_t* title = L"Nocturne";
		int width = 1280;
		int height = 720;
		bool resizable = true;
	};

	class WinWindow
	{
	public:
		bool Create(const WinWindowDesc& desc);
		void Destroy();

		bool ShouldQuit() const { return shouldQuit_; }
		void RequestQuit() { shouldQuit_ = true; }

		void* Handle() const { return (void*)hwnd_; }

		// Client size (updated on WM_SIZE).
		uint32_t ClientWidth() const { return clientW_; }
		uint32_t ClientHeight() const { return clientH_; }

		// Phase 7: forward raw input to engine input system through an abstract sink.
		void SetRawInputSink(platform::IRawInputSink* sink) { rawSink_ = sink; }

	private:
		static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

		void RegisterClassOnce();
		void ApplyClientSize(int clientW, int clientH, bool resizable);

		static void DecodeRawInput_(HRAWINPUT hRawInput, WinWindow* win);

	private:
		HWND hwnd_ = nullptr;
		bool shouldQuit_ = false;

		uint32_t clientW_ = 0;
		uint32_t clientH_ = 0;

		platform::IRawInputSink* rawSink_ = nullptr;
	};
}
```

---

## `Engine/Platform/Win32/WinWindow.cpp` (MODIFIED: track WM_SIZE)

```cpp
#include <malloc.h>
#include "WinWindow.h"
#include "Core/Log.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace noc
{
	static const wchar_t* kWndClassName = L"NocturneWndClass";

	void WinWindow::RegisterClassOnce()
	{
		static bool registered = false;
		if (registered)
			return;

		WNDCLASSEXW wc{};
		wc.cbSize = sizeof(wc);
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = &WinWindow::WndProc;
		wc.hInstance = GetModuleHandleW(nullptr);
		wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
		wc.lpszClassName = kWndClassName;

		if (!RegisterClassExW(&wc))
		{
			NOC_LOG_FATAL("Win32", "RegisterClassExW failed (err=%lu)", GetLastError());
		}

		registered = true;
	}

	void WinWindow::ApplyClientSize(int clientW, int clientH, bool resizable)
	{
		DWORD style = WS_OVERLAPPEDWINDOW;
		if (!resizable)
		{
			style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
		}

		RECT r{ 0, 0, clientW, clientH };
		AdjustWindowRect(&r, style, FALSE);

		const int winW = r.right - r.left;
		const int winH = r.bottom - r.top;

		SetWindowLongPtrW(hwnd_, GWL_STYLE, (LONG_PTR)style);
		SetWindowPos(hwnd_, nullptr, 100, 100, winW, winH, SWP_NOZORDER | SWP_FRAMECHANGED);
	}

	bool WinWindow::Create(const WinWindowDesc& desc)
	{
		RegisterClassOnce();

		HINSTANCE hInst = GetModuleHandleW(nullptr);

		hwnd_ = CreateWindowExW(
			0,
			kWndClassName,
			desc.title,
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT,
			desc.width, desc.height,
			nullptr, nullptr,
			hInst,
			this
		);

		if (!hwnd_)
		{
			NOC_LOG_FATAL("Win32", "CreateWindowExW failed (err=%lu)", GetLastError());
			return false;
		}

		ApplyClientSize(desc.width, desc.height, desc.resizable);

		clientW_ = (uint32_t)desc.width;
		clientH_ = (uint32_t)desc.height;

		ShowWindow(hwnd_, SW_SHOW);
		UpdateWindow(hwnd_);

		return true;
	}

	void WinWindow::Destroy()
	{
		if (hwnd_)
		{
			DestroyWindow(hwnd_);
			hwnd_ = nullptr;
		}
	}

	void WinWindow::DecodeRawInput_(HRAWINPUT hRawInput, WinWindow* win)
	{
		if (!win) return;

		UINT size = 0;
		GetRawInputData(hRawInput, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
		if (!size) return;

		uint8_t stackBuf[512];
		uint8_t* buf = stackBuf;

		if (size > sizeof(stackBuf))
			buf = (uint8_t*)_alloca(size);

		if (GetRawInputData(hRawInput, RID_INPUT, buf, &size, sizeof(RAWINPUTHEADER)) != size)
			return;

		const RAWINPUT* ri = reinterpret_cast<const RAWINPUT*>(buf);

		if (ri->header.dwType == RIM_TYPEMOUSE)
		{
			const RAWMOUSE& m = ri->data.mouse;
			platform::RawMouseEvent e{};
			e.dx = m.lLastX;
			e.dy = m.lLastY;

			if (m.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)  e.buttonDownMask |= 0x1;
			if (m.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)    e.buttonUpMask |= 0x1;
			if (m.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN) e.buttonDownMask |= 0x2;
			if (m.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)   e.buttonUpMask |= 0x2;
			if (m.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN) e.buttonDownMask |= 0x4;
			if (m.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)   e.buttonUpMask |= 0x4;

			if (m.usButtonFlags & RI_MOUSE_WHEEL)
				e.wheel = (short)m.usButtonData;

			if (win->rawSink_) win->rawSink_->OnRawMouse(e);
		}
		else if (ri->header.dwType == RIM_TYPEKEYBOARD)
		{
			const RAWKEYBOARD& k = ri->data.keyboard;

			platform::RawKeyboardEvent e{};
			e.vkey = (uint16_t)k.VKey;
			e.down = (k.Flags & RI_KEY_BREAK) == 0;

			if (win->rawSink_) win->rawSink_->OnRawKeyboard(e);
		}
	}

	LRESULT CALLBACK WinWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (msg == WM_NCCREATE)
		{
			auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
			auto* win = reinterpret_cast<WinWindow*>(cs->lpCreateParams);
			SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(win));
			return DefWindowProcW(hWnd, msg, wParam, lParam);
		}

		auto* win = reinterpret_cast<WinWindow*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

		switch (msg)
		{
		case WM_CLOSE:
			if (win) win->RequestQuit();
			return 0;

		case WM_DESTROY:
			if (win) win->RequestQuit();
			PostQuitMessage(0);
			return 0;

		case WM_INPUT:
			if (win)
				DecodeRawInput_((HRAWINPUT)lParam, win);
			return 0;

		case WM_KILLFOCUS:
			if (win && win->rawSink_)
				win->rawSink_->OnFocusLost();
			return 0;

		case WM_SIZE:
			if (win)
			{
				const UINT w = LOWORD(lParam);
				const UINT h = HIWORD(lParam);
				win->clientW_ = (uint32_t)w;
				win->clientH_ = (uint32_t)h;
			}
			return 0;

		default:
			return DefWindowProcW(hWnd, msg, wParam, lParam);
		}
	}
}
```

---

## `Engine/Runtime/Engine.h` (MODIFIED: owns RenderSystem)

```cpp
#pragma once
#include "Core/BuildConfig.h"
#include "Core/Subsystems/SubsystemRegistry.h"
#include "Core/Memory/Allocator.h"
#include "Core/Memory/LinearArena.h"

#if NOC_ENABLE_ASSERTS
#include "Core/Memory/DebugAlloc.h"
#endif

#include "Resources/VirtualFileSystem.h"
#include "Resources/ResourceManager.h"
#include "EngineConfig.h"
#include "Core/Jobs/JobSystem.h"
#include "Input/InputSystem.h"
#include "Render/RenderSystem.h"

namespace noc {

	class WinWindow;
	class MainLoop;

	class Engine
	{
	public:
		// Config access: only valid BEFORE Init().
		EngineConfig& ConfigMutable();
		const EngineConfig& Config() const { return cfg_; }

		bool SetContentRoot(const char* path);
		bool SetOverrideRoot(const char* path);
		bool SetArchivePath(const char* path);

		bool Init();
		void TickOnce();
		void Shutdown();

		int Run();
		void BeginFrame();
		void Tick();
		void EndFrame();

		IAllocator& Allocator();
		LinearArena& FrameArena();

		VirtualFileSystem& VFS() { return vfs_; }
		ResourceManager& Resources() { return resources_; }
		const ResourceManager& Resources() const { return resources_; }

		InputSystem& Input() { return input_; }
		const InputSystem& Input() const { return input_; }

		JobSystem& Jobs() { return jobs_; }
		const JobSystem& Jobs() const { return jobs_; }

		bool InitMemory();
		void KillMemory();

		bool AttachWindow(WinWindow& window);

	private:
		bool IsConfigMutable() const { return !initialized_; }

	private:
		SubsystemRegistry registry_;

		MallocAllocator baseAlloc_;

#if NOC_ENABLE_ASSERTS
		DebugAlloc debugAlloc_{ baseAlloc_ };
		IAllocator* alloc_ = &debugAlloc_;
#else
		IAllocator* alloc_ = &baseAlloc_;
#endif

		void* frameArenaMem_ = nullptr;
		LinearArena frameArena_;

		VirtualFileSystem vfs_;
		JobSystem jobs_;
		ResourceManager resources_;
		InputSystem input_;

		RenderSystem render_;

		EngineConfig cfg_{};
		bool initialized_ = false;
	};

} // namespace noc
```

---

## `Engine/Runtime/Engine.cpp` (MODIFIED: attach + frame integration)

```cpp
#include "Engine.h"

#include <algorithm>
#include <span>

#include "Core/Assert.h"
#include "Core/Log.h"
#include "Core/Clock.h"

#include "Platform/Win32/WinWindow.h"
#include "Runtime/MainLoop.h"

namespace noc {

	IAllocator& Engine::Allocator() { return *alloc_; }
	LinearArena& Engine::FrameArena() { return frameArena_; }

	EngineConfig& Engine::ConfigMutable()
	{
		if (initialized_)
		{
			NOC_LOG_ERROR("Runtime", "EngineConfig is frozen after Init(). Modify config before calling Init().");
			return cfg_;
		}
		return cfg_;
	}

	bool Engine::SetContentRoot(const char* path)
	{
		if (!IsConfigMutable())
		{
			NOC_LOG_ERROR("Runtime", "SetContentRoot() called after Init(); ignored.");
			return false;
		}
		return cfg_.SetContentRoot(path);
	}

	bool Engine::SetOverrideRoot(const char* path)
	{
		if (!IsConfigMutable())
		{
			NOC_LOG_ERROR("Runtime", "SetOverrideRoot() called after Init(); ignored.");
			return false;
		}
		return cfg_.SetOverrideRoot(path);
	}

	bool Engine::SetArchivePath(const char* path)
	{
		if (!IsConfigMutable())
		{
			NOC_LOG_ERROR("Runtime", "SetArchivePath() called after Init(); ignored.");
			return false;
		}
		return cfg_.SetArchivePath(path);
	}

	bool Engine::Init()
	{
		// Phase 1 subsystem registration/startup already exists in your file.
		// Keep that part unchanged; below is only the new Phase 8 wiring.

		// --- existing init path (core + vfs + jobs + resources + input) ---
		if (!registry_.StartupAll(this))
			return false;

		// Phase 8: render system init (no HWND yet).
#if NOC_ENABLE_ASSERTS
		const bool enableDebugLayer = true;
#else
		const bool enableDebugLayer = false;
#endif
		if (!render_.Init(enableDebugLayer))
			return false;

		initialized_ = true;
		return true;
	}

	bool Engine::AttachWindow(WinWindow& window)
	{
		// Forward WM_INPUT + focus loss -> InputSystem.
		window.SetRawInputSink(input_.RawSink());

		// Register Raw Input devices (requires HWND).
		if (!input_.AttachToWindow(window.Handle()))
			return false;

		// Phase 8: DX12 attach (requires HWND + client size).
		if (!render_.AttachToWindow(window.Handle(), window.ClientWidth(), window.ClientHeight()))
			return false;

		return true;
	}

	int Engine::Run()
	{
		WinWindow window;
		WinWindowDesc wd{};
		wd.title = L"NocturneHost";
		wd.width = 1280;
		wd.height = 720;
		wd.resizable = true;

		if (!window.Create(wd))
		{
			NOC_LOG_FATAL("Win32", "Failed to create window");
			return -1;
		}

		if (!AttachWindow(window))
		{
			NOC_LOG_FATAL("Runtime", "Failed to attach systems to window");
			window.Destroy();
			return -1;
		}

		MainLoop loop;
		loop.Run(*this, window);

		window.Destroy();
		return 0;
	}

	void Engine::BeginFrame()
	{
		GetTime().BeginFrame();
		FrameArena().Reset();

		input_.BeginFrame();
		render_.BeginFrame();
	}

	void Engine::Tick()
	{
		input_.Update();
		resources_.Update();
	}

	void Engine::EndFrame()
	{
		// Phase 8: clear + present.
		render_.EndFramePresent();

		GetTime().EndFrame();
	}

	void Engine::TickOnce()
	{
		GetTime().BeginFrame();
		FrameArena().Reset();

		void* a = FrameArena().Allocate(256, 16);
		void* b = FrameArena().Allocate(1024, 64);
		(void)a; (void)b;

		GetTime().EndFrame();

		NOC_LOG_INFO("Core", "TickOnce() dt=%.6f sec arenaUsed=%zu bytes",
			GetTime().DeltaSeconds(),
			FrameArena().Used());
	}

	void Engine::Shutdown()
	{
		// Make sure GPU is idle before tearing down core allocators/logging.
		render_.Shutdown();

		// Resource manager must shutdown while jobs + memory + log still exist.
		resources_.Shutdown();
		input_.Shutdown();
		registry_.ShutdownAll(this);
	}

} // namespace noc
```

---

## Notes you will likely need in your build system (FYI)

* Ensure you link: `d3d12`, `dxgi`, `dxguid` (the `.cpp` also has `#pragma comment(lib, ...)` to help on MSVC).
* Windows SDK must include D3D12 headers (VS2022 default is fine).

---

If you want, paste your current CMakeLists for the engine target and I’ll give you the exact `target_link_libraries(...)` lines (but the code above should already compile under MSVC due to the pragmas).

```