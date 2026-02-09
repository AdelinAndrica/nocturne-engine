# Phase 9 — First Real Draw (Clean SRP Version)

> **Status:** READY FOR IMPLEMENTATION ⏳  
> **Scope:** Draw a single triangle every frame, but with a clean internal module split (SOC/SRP).  
> **Depends on:** Phase 8 — Rendering Bootstrap  
> **Primary sources:** Luna on root signatures, shader compilation, PSO validation concepts  

## 1) Phase name + objective

Implement “first triangle” while keeping:

* swap chain + per-frame allocator/list reset + fences model intact 
* debug layer clean (no errors)
* RenderSystem file size under control by splitting responsibilities

## 2) Key concepts from the books

* Root signature defines what resources shaders expect; PSO creation validates root signature + shaders compatibility.
* Runtime shader compilation via `D3DCompileFromFile` (we’ll support both file and string; file is recommended long-term). 
* Minimize root signature changes; keep it small. 

## 3) What we implement now

**RenderSystem orchestration:**

* delegates to an internal DX12 renderer object

**DX12 modules (private headers):**

* `Dx12Device` — factory/adapter/device/queue
* `Dx12SwapChain` — swapchain, RTV heap, back buffers, transitions, current index
* `Dx12FrameSync` — fence/event, per-frame fence values, wait/advance
* `ShaderCompiler` — compile HLSL (string or file)
* `TrianglePass` — root signature + PSO + VB, and records the draw commands

## 4) Implementation steps

1. Add `Engine/Render/DX12/` private module files.
2. RenderSystem becomes thin wrapper: `Init/Attach/BeginFrame/EndFramePresent`.
3. `Dx12SwapChain` owns back buffers + RTV heap and provides RTV handle + transition helper.
4. `TrianglePass::Init()` creates root sig + shaders + PSO + vertex buffer.
5. `TrianglePass::Record()` records: set RT, clear, bind PSO/root sig, viewport/scissor, IA, draw.
6. Keep present + fence advance exactly as Phase 8. 

## 5) Verification checklist

* [ ] Triangle visible (not just clear)
* [ ] Debug layer: 0 errors, 0 warnings
* [ ] No device removed
* [ ] Shutdown waits for GPU and cleanly releases
* [ ] Frame indexing/fences still correct (no allocator reuse hazards) 

## 6) Common pitfalls

* Root signature mismatch with shaders → PSO creation failure
* RTV format mismatch between swapchain and PSO
* Forgetting viewport/scissor or topology
* Resource state transitions missing present↔RT (debug layer will complain) 

## 7) Next chat handoff

> “Phase 9 SRP version is implemented: RenderSystem is thin, DX12 is split into Device/SwapChain/Sync/ShaderCompiler/TrianglePass. The engine draws a triangle every frame with zero debug layer errors. Start Phase 10 — GPU Resource Foundations (default heap + upload staging + descriptor heaps + basic texture/SRV).”

---

# Implementations (Clean Phase 9)

## `Engine/Render/RenderSystem.h` (UNCHANGED)

*(Same as Phase 8; keep public header STL-free and platform-opaque.)* 

```cpp
#pragma once
#include <cstdint>

namespace noc
{
	class RenderSystem
	{
	public:
		RenderSystem() = default;

		bool Init(bool enableDebugLayer);
		void Shutdown();

		bool AttachToWindow(void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight);

		void BeginFrame();
		void EndFramePresent();

	private:
		struct Impl;
		Impl* impl_ = nullptr;
	};
}
```

---

## `Engine/Render/RenderSystem.cpp` (REPLACED — now small)

```cpp
#include "RenderSystem.h"

#include "Core/Log.h"

#include "Render/DX12/Dx12Renderer.h"

namespace noc
{
	struct RenderSystem::Impl
	{
		Dx12Renderer renderer;
	};

	bool RenderSystem::Init(bool enableDebugLayer)
	{
		if (impl_)
			return true;

		impl_ = new Impl();
		if (!impl_->renderer.Init(enableDebugLayer))
		{
			NOC_LOG_ERROR("Render", "Dx12Renderer::Init failed");
			delete impl_;
			impl_ = nullptr;
			return false;
		}

		NOC_LOG_INFO("Render", "RenderSystem initialized");
		return true;
	}

	void RenderSystem::Shutdown()
	{
		if (!impl_)
			return;

		impl_->renderer.Shutdown();

		delete impl_;
		impl_ = nullptr;

		NOC_LOG_INFO("Render", "RenderSystem shutdown");
	}

	bool RenderSystem::AttachToWindow(void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight)
	{
		if (!impl_)
			return false;

		return impl_->renderer.AttachToWindow(nativeHwnd, clientWidth, clientHeight);
	}

	void RenderSystem::BeginFrame()
	{
		if (!impl_)
			return;

		impl_->renderer.BeginFrame();
	}

	void RenderSystem::EndFramePresent()
	{
		if (!impl_)
			return;

		impl_->renderer.EndFramePresent();
	}
}
```

---

## `Engine/Render/DX12/Dx12Common.h` (NEW)

```cpp
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <dxgi1_6.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

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
```

---

## `Engine/Render/DX12/Dx12Device.h` (NEW)

```cpp
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
```

---

## `Engine/Render/DX12/Dx12Device.cpp` (NEW)

```cpp
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
```

---

## `Engine/Render/DX12/Dx12FrameSync.h` (NEW)

```cpp
#pragma once
#include "Dx12Common.h"

namespace noc
{
	class Dx12FrameSync
	{
	public:
		bool Init(ID3D12Device* device);
		void Shutdown();

		// Wait for all queued GPU work.
		void WaitForGpu(ID3D12CommandQueue* queue, uint32_t frameIndex);

		// Called after Present; signals fence for the submitted work and waits for next back buffer slot if needed.
		void MoveToNextFrame(ID3D12CommandQueue* queue, IDXGISwapChain3* swapChain, uint32_t& inOutFrameIndex);

	private:
		dx12::ComPtr<ID3D12Fence> fence_;
		uint64_t fenceValues_[dx12::kFrameCount]{};
		HANDLE fenceEvent_ = nullptr;
	};
}
```

---

## `Engine/Render/DX12/Dx12FrameSync.cpp` (NEW)

```cpp
#include "Dx12FrameSync.h"

namespace noc
{
	bool Dx12FrameSync::Init(ID3D12Device* device)
	{
		if (!dx12::HrOk(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)), "CreateFence"))
			return false;

		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
			fenceValues_[i] = 0;

		fenceEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
		if (!fenceEvent_)
		{
			NOC_LOG_ERROR("Render", "CreateEvent failed (err=%lu)", GetLastError());
			return false;
		}

		// Match Phase 8 behavior: bump current slot at first use.
		// (We’ll do it in MoveToNextFrame logic by ensuring values are incremented.)
		return true;
	}

	void Dx12FrameSync::Shutdown()
	{
		dx12::SafeCloseHandle(fenceEvent_);
		fence_.Reset();
	}

	void Dx12FrameSync::WaitForGpu(ID3D12CommandQueue* queue, uint32_t frameIndex)
	{
		const uint64_t v = fenceValues_[frameIndex] + 1;
		fenceValues_[frameIndex] = v;

		queue->Signal(fence_.Get(), v);
		fence_->SetEventOnCompletion(v, fenceEvent_);
		WaitForSingleObjectEx(fenceEvent_, INFINITE, FALSE);
	}

	void Dx12FrameSync::MoveToNextFrame(ID3D12CommandQueue* queue, IDXGISwapChain3* swapChain, uint32_t& inOutFrameIndex)
	{
		const uint32_t submittedIndex = inOutFrameIndex;

		// Signal fence for the work we just submitted on this slot.
		const uint64_t currentFenceValue = fenceValues_[submittedIndex] + 1;
		fenceValues_[submittedIndex] = currentFenceValue;

		HRESULT hr = queue->Signal(fence_.Get(), currentFenceValue);
		if (FAILED(hr))
		{
			NOC_LOG_ERROR("Render", "MoveToNextFrame: Signal failed (hr=0x%08X)", (unsigned)hr);
			return;
		}

		// Advance swapchain index.
		inOutFrameIndex = swapChain->GetCurrentBackBufferIndex();

		// Wait if the next frame slot isn't ready.
		if (fence_->GetCompletedValue() < fenceValues_[inOutFrameIndex])
		{
			fence_->SetEventOnCompletion(fenceValues_[inOutFrameIndex], fenceEvent_);
			WaitForSingleObjectEx(fenceEvent_, INFINITE, FALSE);
		}
	}
}
```

---

## `Engine/Render/DX12/Dx12SwapChain.h` (NEW)

```cpp
#pragma once
#include "Dx12Common.h"

namespace noc
{
	class Dx12SwapChain
	{
	public:
		bool Init(
			IDXGIFactory6* factory,
			ID3D12CommandQueue* queue,
			void* hwnd,
			uint32_t clientWidth,
			uint32_t clientHeight);

		bool CreateRtvHeapAndViews(ID3D12Device* device);

		void Shutdown();

		uint32_t FrameIndex() const { return frameIndex_; }
		void SetFrameIndex(uint32_t i) { frameIndex_ = i; }

		IDXGISwapChain3* SwapChain() const { return swapChain_.Get(); }
		ID3D12Resource* BackBuffer(uint32_t i) const { return backBuffers_[i].Get(); }

		D3D12_CPU_DESCRIPTOR_HANDLE CurrentRtv() const;

		D3D12_RESOURCE_STATES CurrentBackBufferState() const { return bbState_[frameIndex_]; }
		void TransitionCurrent(ID3D12GraphicsCommandList* cmd, D3D12_RESOURCE_STATES to);

		uint32_t Width() const { return w_; }
		uint32_t Height() const { return h_; }

	private:
		dx12::ComPtr<IDXGISwapChain3> swapChain_;

		dx12::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
		UINT rtvDescriptorSize_ = 0;

		dx12::ComPtr<ID3D12Resource> backBuffers_[dx12::kFrameCount];
		D3D12_RESOURCE_STATES bbState_[dx12::kFrameCount]{
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_PRESENT
		};

		uint32_t w_ = 0;
		uint32_t h_ = 0;
		uint32_t frameIndex_ = 0;
	};
}
```

---

## `Engine/Render/DX12/Dx12SwapChain.cpp` (NEW)

```cpp
#include "Dx12SwapChain.h"

namespace noc
{
	bool Dx12SwapChain::Init(
		IDXGIFactory6* factory,
		ID3D12CommandQueue* queue,
		void* hwnd,
		uint32_t clientWidth,
		uint32_t clientHeight)
	{
		if (!factory || !queue || !hwnd || clientWidth == 0 || clientHeight == 0)
			return false;

		w_ = clientWidth;
		h_ = clientHeight;

		DXGI_SWAP_CHAIN_DESC1 sd{};
		sd.Width = clientWidth;
		sd.Height = clientHeight;
		sd.Format = dx12::kBackBufferFormat;
		sd.Stereo = FALSE;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.BufferCount = dx12::kFrameCount;
		sd.Scaling = DXGI_SCALING_STRETCH;
		sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		sd.Flags = 0;

		dx12::ComPtr<IDXGISwapChain1> sc1;
		if (!dx12::HrOk(factory->CreateSwapChainForHwnd(queue, (HWND)hwnd, &sd, nullptr, nullptr, &sc1), "CreateSwapChainForHwnd"))
			return false;

		if (!dx12::HrOk(sc1.As(&swapChain_), "As IDXGISwapChain3"))
			return false;

		frameIndex_ = swapChain_->GetCurrentBackBufferIndex();
		return true;
	}

	bool Dx12SwapChain::CreateRtvHeapAndViews(ID3D12Device* device)
	{
		D3D12_DESCRIPTOR_HEAP_DESC hd{};
		hd.NumDescriptors = dx12::kFrameCount;
		hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		hd.NodeMask = 0;

		if (!dx12::HrOk(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&rtvHeap_)), "CreateDescriptorHeap(RTV)"))
			return false;

		rtvDescriptorSize_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

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

	void Dx12SwapChain::Shutdown()
	{
		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
			backBuffers_[i].Reset();

		rtvHeap_.Reset();
		swapChain_.Reset();

		rtvDescriptorSize_ = 0;
		w_ = h_ = 0;
		frameIndex_ = 0;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE Dx12SwapChain::CurrentRtv() const
	{
		D3D12_CPU_DESCRIPTOR_HANDLE h = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
		h.ptr += (SIZE_T)frameIndex_ * (SIZE_T)rtvDescriptorSize_;
		return h;
	}

	void Dx12SwapChain::TransitionCurrent(ID3D12GraphicsCommandList* cmd, D3D12_RESOURCE_STATES to)
	{
		const uint32_t i = frameIndex_;
		if (bbState_[i] == to)
			return;

		auto b = dx12::TransitionBarrier(backBuffers_[i].Get(), bbState_[i], to);
		cmd->ResourceBarrier(1, &b);
		bbState_[i] = to;
	}
}
```

---

## `Engine/Render/DX12/ShaderCompiler.h` (NEW)

```cpp
#pragma once
#include "Dx12Common.h"

namespace noc
{
	class ShaderCompiler
	{
	public:
		// Compile from in-memory string.
		static bool CompileFromString(
			const char* source,
			const char* entry,
			const char* target,
			dx12::ComPtr<ID3DBlob>& outBytecode);

		// Compile from file (recommended once we introduce shader assets properly).
		static bool CompileFromFile(
			const wchar_t* filepath,
			const char* entry,
			const char* target,
			dx12::ComPtr<ID3DBlob>& outBytecode);
	};
}
```

---

## `Engine/Render/DX12/ShaderCompiler.cpp` (NEW)

```cpp
#include "ShaderCompiler.h"
#include <cstring>

namespace noc
{
	static UINT ShaderFlags_()
	{
		UINT flags = 0;
#if defined(_DEBUG)
		flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
		return flags;
	}

	bool ShaderCompiler::CompileFromString(
		const char* source,
		const char* entry,
		const char* target,
		dx12::ComPtr<ID3DBlob>& outBytecode)
	{
		if (!source || !entry || !target)
			return false;

		dx12::ComPtr<ID3DBlob> errors;
		HRESULT hr = D3DCompile(
			source, std::strlen(source),
			nullptr,
			nullptr,
			nullptr,
			entry, target,
			ShaderFlags_(), 0,
			&outBytecode,
			&errors);

		if (FAILED(hr))
		{
			if (errors)
				NOC_LOG_ERROR("Render", "Shader compile failed (%s/%s): %s", entry, target, (const char*)errors->GetBufferPointer());
			else
				NOC_LOG_ERROR("Render", "Shader compile failed (%s/%s), hr=0x%08X", entry, target, (unsigned)hr);
			return false;
		}

		return true;
	}

	bool ShaderCompiler::CompileFromFile(
		const wchar_t* filepath,
		const char* entry,
		const char* target,
		dx12::ComPtr<ID3DBlob>& outBytecode)
	{
		if (!filepath || !entry || !target)
			return false;

		dx12::ComPtr<ID3DBlob> errors;
		HRESULT hr = D3DCompileFromFile(
			filepath,
			nullptr,
			nullptr,
			entry,
			target,
			ShaderFlags_(), 0,
			&outBytecode,
			&errors);

		if (FAILED(hr))
		{
			if (errors)
				NOC_LOG_ERROR("Render", "Shader compile failed (%s/%s): %s", entry, target, (const char*)errors->GetBufferPointer());
			else
				NOC_LOG_ERROR("Render", "Shader compile failed (%s/%s), hr=0x%08X", entry, target, (unsigned)hr);
			return false;
		}

		return true;
	}
}
```

---

## `Engine/Render/DX12/TrianglePass.h` (NEW)

```cpp
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
```

---

## `Engine/Render/DX12/TrianglePass.cpp` (NEW)

```cpp
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
```

---

## `Engine/Render/DX12/Dx12Renderer.h` (NEW)

```cpp
#pragma once
#include "Dx12Common.h"

#include "Dx12Device.h"
#include "Dx12SwapChain.h"
#include "Dx12FrameSync.h"
#include "TrianglePass.h"

namespace noc
{
	class Dx12Renderer
	{
	public:
		bool Init(bool enableDebugLayer);
		void Shutdown();

		bool AttachToWindow(void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight);

		void BeginFrame();
		void EndFramePresent();

	private:
		void LogDeviceRemoved_(const char* where);

	private:
		bool inited_ = false;
		bool attached_ = false;

		Dx12Device device_;
		Dx12SwapChain swap_;
		Dx12FrameSync sync_;

		dx12::ComPtr<ID3D12CommandAllocator> cmdAlloc_[dx12::kFrameCount];
		dx12::ComPtr<ID3D12GraphicsCommandList> cmdList_;

		uint32_t frameIndex_ = 0;
		bool frameOpen_ = false;

		TrianglePass triangle_;
	};
}
```

---

## `Engine/Render/DX12/Dx12Renderer.cpp` (NEW)

```cpp
#include "Dx12Renderer.h"

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

		// Ensure GPU is idle before releasing.
		if (attached_)
			sync_.WaitForGpu(device_.Queue());

		triangle_.Shutdown();

		cmdList_.Reset();
		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
			cmdAlloc_[i].Reset();

		swap_.Shutdown();
		sync_.Shutdown();
		device_.Shutdown();

		inited_ = false;
		attached_ = false;

		NOC_LOG_INFO("Render", "Dx12Renderer shutdown");
	}

	bool Dx12Renderer::AttachToWindow(void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight)
	{
		if (!inited_)
			return false;

		if (!swap_.Init(device_.Factory(), device_.Queue(), nativeHwnd, clientWidth, clientHeight))
			return false;

		if (!swap_.CreateRtvHeapAndViews(device_.Device()))
			return false;

		frameIndex_ = swap_.FrameIndex();

		// Commands: allocators + one list.
		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
		{
			if (!dx12::HrOk(device_.Device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc_[i])),
				"CreateCommandAllocator"))
			{
				return false;
			}
		}

		if (!dx12::HrOk(device_.Device()->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			cmdAlloc_[frameIndex_].Get(),
			nullptr,
			IID_PPV_ARGS(&cmdList_)),
			"CreateCommandList"))
		{
			return false;
		}

		// Close it; BeginFrame will Reset it (Phase 8 pattern). :contentReference[oaicite:16]{index=16}
		if (!dx12::HrOk(cmdList_->Close(), "cmdList->Close (initial)"))
			return false;

		// Phase 9: triangle pass.
		if (!triangle_.Init(device_.Device(), swap_.Width(), swap_.Height()))
			return false;

		attached_ = true;
		NOC_LOG_INFO("Render", "DX12 ready (%ux%u, buffers=%u)", clientWidth, clientHeight, dx12::kFrameCount);
		return true;
	}

	void Dx12Renderer::LogDeviceRemoved_(const char* where)
	{
		if (!device_.Device())
			return;

		HRESULT dr = device_.Device()->GetDeviceRemovedReason();
		if (dr != S_OK)
			NOC_LOG_ERROR("Render", "Device removed at %s (hr=0x%08X)", where, (unsigned)dr);
	}

	void Dx12Renderer::BeginFrame()
	{
		if (!attached_ || !device_.Device())
			return;

		if (device_.Device()->GetDeviceRemovedReason() != S_OK)
			return;

		if (frameOpen_)
		{
			NOC_LOG_ERROR("Render", "BeginFrame called while frame is already open");
			return;
		}
		frameOpen_ = true;

		// Reset allocator for current frame.
		if (FAILED(cmdAlloc_[frameIndex_]->Reset()))
		{
			NOC_LOG_ERROR("Render", "cmdAlloc->Reset failed");
			LogDeviceRemoved_("cmdAlloc->Reset");
			frameOpen_ = false;
			return;
		}

		if (FAILED(cmdList_->Reset(cmdAlloc_[frameIndex_].Get(), nullptr)))
		{
			NOC_LOG_ERROR("Render", "cmdList->Reset failed");
			LogDeviceRemoved_("cmdList->Reset");
			frameOpen_ = false;
			return;
		}
	}

	void Dx12Renderer::EndFramePresent()
	{
		if (!attached_ || !device_.Device())
			return;

		if (device_.Device()->GetDeviceRemovedReason() != S_OK)
			return;

		if (!frameOpen_)
		{
			NOC_LOG_ERROR("Render", "EndFramePresent called with no open frame");
			return;
		}

		struct Guard { bool& b; ~Guard() { b = false; } } guard{ frameOpen_ };

		// Back buffer to RT.
		swap_.SetFrameIndex(frameIndex_);
		swap_.TransitionCurrent(cmdList_.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET);

		// Record triangle pass (clear + draw).
		triangle_.Record(cmdList_.Get(), swap_.CurrentRtv());

		// RT back to present.
		swap_.TransitionCurrent(cmdList_.Get(), D3D12_RESOURCE_STATE_PRESENT);

		if (FAILED(cmdList_->Close()))
		{
			NOC_LOG_ERROR("Render", "cmdList->Close failed");
			LogDeviceRemoved_("cmdList->Close");
			return;
		}

		ID3D12CommandList* lists[] = { cmdList_.Get() };
		device_.Queue()->ExecuteCommandLists(1, lists);

		HRESULT hr = swap_.SwapChain()->Present(1, 0);
		if (FAILED(hr))
		{
			NOC_LOG_ERROR("Render", "Present failed (hr=0x%08X)", (unsigned)hr);
			LogDeviceRemoved_("Present");
			return;
		}

		// Advance frame index + sync (keeps Phase 8 model intact). :contentReference[oaicite:17]{index=17}
		sync_.MoveToNextFrame(device_.Queue(), swap_.SwapChain(), frameIndex_);
	}
}
```

---

## Why this structure stays clean in Phase 10+

This decomposition maps naturally to what your architecture diagram implies for render responsibilities (“CreateBuffer / CreateTexture / CreatePipelineState / RenderFrame”) :

* `Dx12Device` grows into feature checks, descriptor heap managers, allocator hooks.
* `Dx12SwapChain` later gets resize + HDR choices, but stays swapchain-only.
* `TrianglePass` becomes your first “pass object” and evolves into mesh draw, constants, etc.
* `ShaderCompiler` becomes an asset-backed system later (compile offline / cache), but API stays stable.

And crucially: `RenderSystem.cpp` stays tiny, stable, and boring.