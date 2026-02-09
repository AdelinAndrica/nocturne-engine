# Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources)

> **Status:** READY FOR IMPLEMENTATION ⏳
> **Scope:** Default-heap GPU resources + upload staging, persistent descriptor heaps + allocation, minimal future-proof root signature, VFS-backed shader source, PSO cache, asset-driven mesh upload, GPU-safe destruction
> **Depends on:** Phase 9 — Rendering Engine Foundation (SRP DX12 renderer split + triangle draw) 
> **Primary sources:**
>
> * Gregory (*Game Engine Architecture*): engine owns lifecycle + deterministic handoff points; resource pipeline “worker produces, main thread finalizes” mindset generalized here for GPU work.
> * Lengyel (*Foundations Vol. 1/2*): renderer as a staged pipeline; keep responsibilities separated and feed the GPU only what’s needed each frame.
> * Luna (DX12): descriptors/heaps + root signature descriptor tables concepts (CBV/SRV/UAV + samplers).

This phase turns Phase 9’s “first draw” into a **real, asset-driven GPU foundation** while keeping the **existing**:

* frame indexing model (2 buffers) 
* fence-based synchronization 
* allocator/list lifetime rules 
* swap chain ownership model 
* RenderSystem → Dx12Renderer orchestration 

---

## 1) Phase objective

Build a **first-class GPU resource layer** for Nocturne Engine:

* GPU memory model (default heap + upload staging)
* reusable descriptor heaps with stable handles (no per-frame recreation)
* minimal root signature that scales to materials/textures
* runtime shader compilation from VFS-backed sources
* PSO cache (no per-pass duplication)
* asset-backed mesh upload (CPU bytes → GPU default buffers) with async “not ready yet” behavior
* GPU-safe destruction (no releasing in-flight resources)

---

## 2) Key concepts from the books

* **Deterministic handoff points**: background/async stages do work, then publish at a known point to keep behavior debuggable and avoid races—Phase 5/6 already established this for CPU assets; we apply the same discipline to GPU uploads and deferred releases.
* **Renderer responsibility separation**: submit draw work (application stage) and manage GPU state/resources cleanly; don’t leak platform/GPU details across layers. 
* **Descriptors and descriptor heaps**: views (CBV/SRV/UAV + Samplers) live in descriptor heaps and are what the GPU binds; you typically create descriptor heaps up front and allocate within them rather than recreating every frame.

---

## 3) What we implement now

### A) GPU memory + upload path

* `GpuBuffer` abstraction supporting:

  * static vertex buffer (default heap) + upload staging
  * static index buffer (default heap) + upload staging
  * per-frame constant buffer (upload heap, 256-byte aligned allocations)
* correct state transitions during upload:

  * `COMMON/COPY_DEST → VERTEX_AND_CONSTANT_BUFFER` or `INDEX_BUFFER`

### B) Descriptor heap infrastructure

* persistent shader-visible:

  * CBV/SRV/UAV heap (e.g., 1024 descriptors)
  * Sampler heap (e.g., 64 descriptors)
* simple linear allocator (stable handles across frames; no free yet)

### C) Minimal future-proof root signature

* Root parameter slots (documented):

  * `0`: per-frame CBV descriptor table (b0)
  * `1`: per-draw/per-material SRV descriptor table (t0..)
  * `2`: sampler descriptor table (s0..)

### D) VFS-backed shaders + PSO cache

* shader source loaded through ResourceManager as `TextResource` (VFS-backed)
* compile at runtime (dev-friendly)
* PSO cache keyed by “VS+PS+rootSig+rtvFormat+inputLayout”

### E) Asset-backed mesh upload

* mesh asset loaded through ResourceManager as **Binary** (VFS-backed)
* parse a tiny Nocturne mesh format (`NMSH`) into CPU arrays
* upload to default-heap VB/IB via the upload path
* if mesh/shader not ready: pass does “clear only” (no crash, no stalls)

### F) GPU-safe destruction

* deferred release queue tagged with “fence value that will be signaled for this frame”
* collect when fence completed value passes tag

---

## 4) Implementation steps

1. Add DX12 foundation modules:

   * `Dx12DescriptorAllocator`
   * `Dx12DeferredReleaseQueue`
   * `Dx12UploadTracker`
   * `GpuBuffer` + `GpuRingConstantBuffer`
2. Replace `TrianglePass` with `MeshPass`:

   * loads shader source (TextResource) and mesh bytes (Binary)
   * creates root signature + PSO
   * creates descriptors for per-frame CBVs
   * uploads mesh to default heap and draws indexed
3. Extend `Dx12FrameSync` with tiny accessors:

   * `FenceValueForFrame(frameIndex)`
   * `CompletedValue()`
4. Wire ResourceManager pointer into RenderSystem → Dx12Renderer (thin change).
5. Verification: run with debug layer and ensure **zero errors** while rendering a mesh from default heap.

---

# 5) Full C++ implementations (Phase 9.5)

Below are **every new or modified file** for Phase 9.5.
Each file is preceded by its **engine-relative path**.

---

## `Engine/Render/RenderSystem.h` (MODIFIED)

```cpp
#pragma once
#include <cstdint>

namespace noc
{
	class ResourceManager;

	class RenderSystem
	{
	public:
		RenderSystem() = default;

		bool Init(bool enableDebugLayer);
		void Shutdown();

		bool AttachToWindow(void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight);

		// Phase 9.5: allow renderer to consume engine assets via ResourceManager.
		// RenderSystem remains thin; this is just a pointer handoff.
		void SetResourceManager(ResourceManager* rm);

		void BeginFrame();
		void EndFramePresent();

	private:
		struct Impl;
		Impl* impl_ = nullptr;
	};
}
```

---

## `Engine/Render/RenderSystem.cpp` (MODIFIED)

```cpp
#include "RenderSystem.h"

#include "Core/Log.h"
#include "Resources/ResourceManager.h"
#include "Render/DX12/Dx12Renderer.h"

namespace noc
{
	struct RenderSystem::Impl
	{
		Dx12Renderer renderer;
		ResourceManager* rm = nullptr;
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

		if (!impl_->renderer.AttachToWindow(nativeHwnd, clientWidth, clientHeight))
			return false;

		// If RM was already provided, forward it now that renderer is attached.
		impl_->renderer.SetResourceManager(impl_->rm);
		return true;
	}

	void RenderSystem::SetResourceManager(ResourceManager* rm)
	{
		if (!impl_)
			return;

		impl_->rm = rm;
		impl_->renderer.SetResourceManager(rm);
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

## `Engine/Render/DX12/Dx12FrameSync.h` (MODIFIED)

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

		// Wait for the GPU to finish all work up to the last signaled fence.
		void WaitForGpu(ID3D12CommandQueue* queue);

		// Called at end of frame: signal fence for current frame, advance swapchain index, wait if needed.
		void MoveToNextFrame(ID3D12CommandQueue* queue, IDXGISwapChain3* swapChain, uint32_t& inOutFrameIndex);

		// Phase 9.5: accessors for deferred release + upload tracking.
		uint64_t FenceValueForFrame(uint32_t frameIndex) const { return fenceValues_[frameIndex]; }
		uint64_t CompletedValue() const { return fence_ ? fence_->GetCompletedValue() : 0; }

	private:
		void WaitForFenceValue_(uint64_t v);

	private:
		dx12::ComPtr<ID3D12Fence> fence_;
		HANDLE fenceEvent_ = nullptr;

		uint64_t fenceValues_[dx12::kFrameCount]{};
	};
}
```

---

## `Engine/Render/DX12/Dx12FrameSync.cpp` (MODIFIED)

```cpp
#include "Dx12FrameSync.h"

namespace noc
{
	bool Dx12FrameSync::Init(ID3D12Device* device)
	{
		if (!device)
			return false;

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

		// Match Phase 8/9 behavior: bump once so first MoveToNextFrame is sane.
		fenceValues_[0] = 1;
		fenceValues_[1] = 1;
		return true;
	}

	void Dx12FrameSync::Shutdown()
	{
		dx12::SafeCloseHandle(fenceEvent_);
		fence_.Reset();
	}

	void Dx12FrameSync::WaitForFenceValue_(uint64_t v)
	{
		if (!fence_ || !fenceEvent_)
			return;

		if (fence_->GetCompletedValue() >= v)
			return;

		fence_->SetEventOnCompletion(v, fenceEvent_);
		WaitForSingleObjectEx(fenceEvent_, INFINITE, FALSE);
	}

	void Dx12FrameSync::WaitForGpu(ID3D12CommandQueue* queue)
	{
		if (!queue || !fence_)
			return;

		// Use frame 0 fence slot as a “flush” lane.
		const uint64_t v = fenceValues_[0];
		queue->Signal(fence_.Get(), v);
		WaitForFenceValue_(v);
		fenceValues_[0] = v + 1;
	}

	void Dx12FrameSync::MoveToNextFrame(ID3D12CommandQueue* queue, IDXGISwapChain3* swapChain, uint32_t& inOutFrameIndex)
	{
		if (!queue || !swapChain || !fence_)
			return;

		const uint32_t frameIndex = inOutFrameIndex;

		// Signal for the frame we just submitted.
		const uint64_t currentFenceValue = fenceValues_[frameIndex];
		HRESULT hr = queue->Signal(fence_.Get(), currentFenceValue);
		if (FAILED(hr))
		{
			NOC_LOG_ERROR("Render", "MoveToNextFrame: Signal failed (hr=0x%08X)", (unsigned)hr);
			return;
		}

		// Update to the next back buffer.
		inOutFrameIndex = swapChain->GetCurrentBackBufferIndex();

		// Wait until that back buffer is ready (avoid allocator reuse hazards).
		const uint64_t nextFenceValue = fenceValues_[inOutFrameIndex];
		WaitForFenceValue_(nextFenceValue);

		// Set fence value for next time we signal on this frame.
		fenceValues_[inOutFrameIndex] = currentFenceValue + 1;
	}
}
```

---

## `Engine/Render/DX12/Dx12DescriptorAllocator.h` (NEW)

```cpp
#pragma once
#include "Dx12Common.h"
#include <cstdint>

namespace noc
{
	struct Dx12DescriptorHandle
	{
		D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
		D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
		uint32_t index = 0;
		bool shaderVisible = false;
	};

	class Dx12DescriptorAllocator
	{
	public:
		bool Init(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t capacity, bool shaderVisible);
		void Shutdown();

		Dx12DescriptorHandle Allocate(); // linear for Phase 9.5 (stable, no frees)
		ID3D12DescriptorHeap* Heap() const { return heap_.Get(); }
		uint32_t DescriptorSize() const { return descriptorSize_; }
		bool ShaderVisible() const { return shaderVisible_; }

	private:
		dx12::ComPtr<ID3D12DescriptorHeap> heap_;
		D3D12_DESCRIPTOR_HEAP_TYPE type_ = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		uint32_t descriptorSize_ = 0;
		uint32_t capacity_ = 0;
		uint32_t cursor_ = 0;
		bool shaderVisible_ = false;
	};
}
```

---

## `Engine/Render/DX12/Dx12DescriptorAllocator.cpp` (NEW)

```cpp
#include "Dx12DescriptorAllocator.h"

namespace noc
{
	bool Dx12DescriptorAllocator::Init(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t capacity, bool shaderVisible)
	{
		if (!device || capacity == 0)
			return false;

		type_ = type;
		capacity_ = capacity;
		cursor_ = 0;
		shaderVisible_ = shaderVisible;

		D3D12_DESCRIPTOR_HEAP_DESC hd{};
		hd.Type = type;
		hd.NumDescriptors = capacity;
		hd.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		if (!dx12::HrOk(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap_)), "CreateDescriptorHeap"))
			return false;

		descriptorSize_ = device->GetDescriptorHandleIncrementSize(type);
		return true;
	}

	void Dx12DescriptorAllocator::Shutdown()
	{
		heap_.Reset();
		capacity_ = 0;
		cursor_ = 0;
		descriptorSize_ = 0;
		shaderVisible_ = false;
	}

	Dx12DescriptorHandle Dx12DescriptorAllocator::Allocate()
	{
		Dx12DescriptorHandle h{};
		if (!heap_ || cursor_ >= capacity_)
		{
			NOC_LOG_ERROR("Render", "DescriptorAllocator out of space (type=%u cap=%u)", (unsigned)type_, capacity_);
			return h;
		}

		const uint32_t idx = cursor_++;
		D3D12_CPU_DESCRIPTOR_HANDLE cpu = heap_->GetCPUDescriptorHandleForHeapStart();
		cpu.ptr += (SIZE_T)idx * (SIZE_T)descriptorSize_;

		h.cpu = cpu;
		h.index = idx;
		h.shaderVisible = shaderVisible_;

		if (shaderVisible_)
		{
			D3D12_GPU_DESCRIPTOR_HANDLE gpu = heap_->GetGPUDescriptorHandleForHeapStart();
			gpu.ptr += (UINT64)idx * (UINT64)descriptorSize_;
			h.gpu = gpu;
		}

		return h;
	}
}
```

---

## `Engine/Render/DX12/Dx12DeferredReleaseQueue.h` (NEW)

```cpp
#pragma once
#include "Dx12Common.h"
#include <cstdint>
#include <vector>

namespace noc
{
	class Dx12DeferredReleaseQueue
	{
	public:
		void Enqueue(uint64_t fenceValue, dx12::ComPtr<IUnknown>&& obj);
		void Collect(uint64_t completedFenceValue);
		void Clear(); // drop everything immediately (only call after GPU idle)

	private:
		struct Item
		{
			uint64_t fenceValue = 0;
			dx12::ComPtr<IUnknown> obj;
		};

		std::vector<Item> items_;
	};
}
```

---

## `Engine/Render/DX12/Dx12DeferredReleaseQueue.cpp` (NEW)

```cpp
#include "Dx12DeferredReleaseQueue.h"

namespace noc
{
	void Dx12DeferredReleaseQueue::Enqueue(uint64_t fenceValue, dx12::ComPtr<IUnknown>&& obj)
	{
		if (!obj)
			return;

		Item it{};
		it.fenceValue = fenceValue;
		it.obj = std::move(obj);
		items_.push_back(std::move(it));
	}

	void Dx12DeferredReleaseQueue::Collect(uint64_t completedFenceValue)
	{
		// Compact in-place.
		size_t out = 0;
		for (size_t i = 0; i < items_.size(); ++i)
		{
			if (items_[i].fenceValue <= completedFenceValue)
			{
				// eligible: let ComPtr drop
				continue;
			}
			if (out != i)
				items_[out] = std::move(items_[i]);
			++out;
		}
		items_.resize(out);
	}

	void Dx12DeferredReleaseQueue::Clear()
	{
		items_.clear();
	}
}
```

---

## `Engine/Render/DX12/GpuBuffer.h` (NEW)

```cpp
#pragma once
#include "Dx12Common.h"
#include <cstdint>
#include <cstddef>

namespace noc
{
	class Dx12DeferredReleaseQueue;
	class Dx12FrameSync;

	class GpuBuffer
	{
	public:
		enum class Kind : uint8_t { Vertex, Index };

		bool CreateStatic(
			ID3D12Device* device,
			ID3D12GraphicsCommandList* cmd,
			Dx12DeferredReleaseQueue& deferred,
			const Dx12FrameSync& sync,
			uint32_t frameIndex,
			Kind kind,
			const void* srcData,
			size_t numBytes,
			uint32_t strideBytes);

		void ShutdownNow();

		ID3D12Resource* Resource() const { return resource_.Get(); }
		size_t SizeBytes() const { return sizeBytes_; }

		D3D12_VERTEX_BUFFER_VIEW VertexView() const;
		D3D12_INDEX_BUFFER_VIEW IndexView(DXGI_FORMAT fmt) const;

	private:
		dx12::ComPtr<ID3D12Resource> resource_;
		size_t sizeBytes_ = 0;
		uint32_t strideBytes_ = 0;
		Kind kind_ = Kind::Vertex;
	};
}
```

---

## `Engine/Render/DX12/GpuBuffer.cpp` (NEW)

```cpp
#include "GpuBuffer.h"
#include "Dx12DeferredReleaseQueue.h"
#include "Dx12FrameSync.h"

namespace noc
{
	static D3D12_RESOURCE_DESC BufferDesc_(UINT64 bytes)
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

	bool GpuBuffer::CreateStatic(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmd,
		Dx12DeferredReleaseQueue& deferred,
		const Dx12FrameSync& sync,
		uint32_t frameIndex,
		Kind kind,
		const void* srcData,
		size_t numBytes,
		uint32_t strideBytes)
	{
		if (!device || !cmd || !srcData || numBytes == 0)
			return false;

		ShutdownNow();

		kind_ = kind;
		sizeBytes_ = numBytes;
		strideBytes_ = strideBytes;

		// Default heap (GPU-only)
		D3D12_HEAP_PROPERTIES hpDefault{};
		hpDefault.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC desc = BufferDesc_((UINT64)numBytes);

		if (!dx12::HrOk(device->CreateCommittedResource(
			&hpDefault,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&resource_)), "CreateCommittedResource(DefaultBuffer)"))
		{
			return false;
		}

		// Upload heap (CPU-visible staging)
		D3D12_HEAP_PROPERTIES hpUpload{};
		hpUpload.Type = D3D12_HEAP_TYPE_UPLOAD;

		dx12::ComPtr<ID3D12Resource> upload;
		if (!dx12::HrOk(device->CreateCommittedResource(
			&hpUpload,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&upload)), "CreateCommittedResource(UploadBuffer)"))
		{
			resource_.Reset();
			return false;
		}

		// Copy bytes into upload.
		void* mapped = nullptr;
		D3D12_RANGE r{ 0, 0 };
		if (!dx12::HrOk(upload->Map(0, &r, &mapped), "UploadBuffer.Map"))
		{
			resource_.Reset();
			upload.Reset();
			return false;
		}
		memcpy(mapped, srcData, numBytes);
		upload->Unmap(0, nullptr);

		// Record copy into default buffer.
		cmd->CopyBufferRegion(resource_.Get(), 0, upload.Get(), 0, (UINT64)numBytes);

		// Transition to final state for binding.
		D3D12_RESOURCE_STATES finalState =
			(kind == Kind::Vertex) ? D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
			                       : D3D12_RESOURCE_STATE_INDEX_BUFFER;

		D3D12_RESOURCE_BARRIER b = dx12::TransitionBarrier(resource_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, finalState);
		cmd->ResourceBarrier(1, &b);

		// Keep upload alive until the fence value for this frame has completed.
		// We tag with the value that *will be signaled* for this frame index.
		{
			dx12::ComPtr<IUnknown> asUnknown;
			upload.As(&asUnknown);
			deferred.Enqueue(sync.FenceValueForFrame(frameIndex), std::move(asUnknown));
		}

		return true;
	}

	void GpuBuffer::ShutdownNow()
	{
		resource_.Reset();
		sizeBytes_ = 0;
		strideBytes_ = 0;
		kind_ = Kind::Vertex;
	}

	D3D12_VERTEX_BUFFER_VIEW GpuBuffer::VertexView() const
	{
		D3D12_VERTEX_BUFFER_VIEW v{};
		if (!resource_)
			return v;

		v.BufferLocation = resource_->GetGPUVirtualAddress();
		v.SizeInBytes = (UINT)sizeBytes_;
		v.StrideInBytes = strideBytes_;
		return v;
	}

	D3D12_INDEX_BUFFER_VIEW GpuBuffer::IndexView(DXGI_FORMAT fmt) const
	{
		D3D12_INDEX_BUFFER_VIEW v{};
		if (!resource_)
			return v;

		v.BufferLocation = resource_->GetGPUVirtualAddress();
		v.SizeInBytes = (UINT)sizeBytes_;
		v.Format = fmt;
		return v;
	}
}
```

---

## `Engine/Render/DX12/GpuRingConstantBuffer.h` (NEW)

```cpp
#pragma once
#include "Dx12Common.h"
#include <cstdint>
#include <cstddef>

namespace noc
{
	// Per-frame upload-heap constant buffer (one resource per frame).
	// Constants are allocated linearly; reset once per frame.
	class GpuRingConstantBuffer
	{
	public:
		bool Init(ID3D12Device* device, size_t bytesPerFrame);
		void Shutdown();

		void BeginFrame(uint32_t frameIndex);

		// Allocates aligned (256B) constant data in the *current* frame buffer.
		// Returns GPU virtual address and CPU pointer to write into.
		bool Allocate(size_t bytes, D3D12_GPU_VIRTUAL_ADDRESS& outGpu, void*& outCpu);

		ID3D12Resource* Resource(uint32_t frameIndex) const { return frames_[frameIndex].resource.Get(); }
		uint8_t* Mapped(uint32_t frameIndex) const { return frames_[frameIndex].mapped; }
		size_t CapacityBytes() const { return capacity_; }

	private:
		static size_t Align256_(size_t x) { return (x + 255u) & ~255u; }

		struct Frame
		{
			dx12::ComPtr<ID3D12Resource> resource;
			uint8_t* mapped = nullptr;
		};

		Frame frames_[dx12::kFrameCount]{};
		size_t capacity_ = 0;
		size_t cursor_ = 0;
		uint32_t curFrame_ = 0;
	};
}
```

---

## `Engine/Render/DX12/GpuRingConstantBuffer.cpp` (NEW)

```cpp
#include "GpuRingConstantBuffer.h"

namespace noc
{
	bool GpuRingConstantBuffer::Init(ID3D12Device* device, size_t bytesPerFrame)
	{
		if (!device || bytesPerFrame == 0)
			return false;

		capacity_ = Align256_(bytesPerFrame);

		D3D12_HEAP_PROPERTIES hp{};
		hp.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC d{};
		d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		d.Width = (UINT64)capacity_;
		d.Height = 1;
		d.DepthOrArraySize = 1;
		d.MipLevels = 1;
		d.SampleDesc.Count = 1;
		d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
		{
			if (!dx12::HrOk(device->CreateCommittedResource(
				&hp,
				D3D12_HEAP_FLAG_NONE,
				&d,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&frames_[i].resource)), "CreateCommittedResource(FrameCB)"))
			{
				return false;
			}

			void* mapped = nullptr;
			D3D12_RANGE r{ 0, 0 };
			if (!dx12::HrOk(frames_[i].resource->Map(0, &r, &mapped), "FrameCB.Map"))
				return false;

			frames_[i].mapped = (uint8_t*)mapped;
		}

		return true;
	}

	void GpuRingConstantBuffer::Shutdown()
	{
		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
		{
			if (frames_[i].resource && frames_[i].mapped)
				frames_[i].resource->Unmap(0, nullptr);

			frames_[i].mapped = nullptr;
			frames_[i].resource.Reset();
		}
		capacity_ = 0;
		cursor_ = 0;
		curFrame_ = 0;
	}

	void GpuRingConstantBuffer::BeginFrame(uint32_t frameIndex)
	{
		curFrame_ = frameIndex;
		cursor_ = 0;
	}

	bool GpuRingConstantBuffer::Allocate(size_t bytes, D3D12_GPU_VIRTUAL_ADDRESS& outGpu, void*& outCpu)
	{
		const size_t aligned = Align256_(bytes);
		if (cursor_ + aligned > capacity_)
			return false;

		auto* res = frames_[curFrame_].resource.Get();
		if (!res || !frames_[curFrame_].mapped)
			return false;

		outGpu = res->GetGPUVirtualAddress() + (UINT64)cursor_;
		outCpu = frames_[curFrame_].mapped + cursor_;
		cursor_ += aligned;
		return true;
	}
}
```

---

## `Engine/Render/DX12/Dx12PsoCache.h` (NEW)

```cpp
#pragma once
#include "Dx12Common.h"
#include <cstdint>
#include <unordered_map>

namespace noc
{
	struct Dx12PsoKey
	{
		const void* vs = nullptr;
		const void* ps = nullptr;
		ID3D12RootSignature* rootSig = nullptr;
		DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		uint64_t inputLayoutHash = 0;

		bool operator==(const Dx12PsoKey& o) const
		{
			return vs == o.vs && ps == o.ps && rootSig == o.rootSig && rtvFormat == o.rtvFormat && inputLayoutHash == o.inputLayoutHash;
		}
	};

	struct Dx12PsoKeyHash
	{
		size_t operator()(const Dx12PsoKey& k) const noexcept
		{
			size_t h = 1469598103934665603ull;
			auto mix = [&](size_t v) { h ^= v; h *= 1099511628211ull; };
			mix((size_t)k.vs);
			mix((size_t)k.ps);
			mix((size_t)k.rootSig);
			mix((size_t)k.rtvFormat);
			mix((size_t)k.inputLayoutHash);
			return h;
		}
	};

	class Dx12PsoCache
	{
	public:
		ID3D12PipelineState* Find(const Dx12PsoKey& key) const;
		void Insert(const Dx12PsoKey& key, dx12::ComPtr<ID3D12PipelineState>&& pso);
		void Clear();

	private:
		std::unordered_map<Dx12PsoKey, dx12::ComPtr<ID3D12PipelineState>, Dx12PsoKeyHash> map_;
	};
}
```

---

## `Engine/Render/DX12/Dx12PsoCache.cpp` (NEW)

```cpp
#include "Dx12PsoCache.h"

namespace noc
{
	ID3D12PipelineState* Dx12PsoCache::Find(const Dx12PsoKey& key) const
	{
		auto it = map_.find(key);
		return (it == map_.end()) ? nullptr : it->second.Get();
	}

	void Dx12PsoCache::Insert(const Dx12PsoKey& key, dx12::ComPtr<ID3D12PipelineState>&& pso)
	{
		map_[key] = std::move(pso);
	}

	void Dx12PsoCache::Clear()
	{
		map_.clear();
	}
}
```

---

## `Engine/Render/DX12/MeshFormat.h` (NEW)

```cpp
#pragma once
#include <cstdint>
#include <vector>

namespace noc
{
	// Minimal binary mesh format for Phase 9.5:
	// Header:
	//   char     magic[4] = "NMSH"
	//   uint32   version  = 1
	//   uint32   vertexCount
	//   uint32   indexCount
	// Vertex:
	//   float3 position
	//   float4 color
	// Indices:
	//   uint16 indexCount entries
	struct MeshVertexPC
	{
		float px, py, pz;
		float r, g, b, a;
	};

	struct CpuMeshPC
	{
		std::vector<MeshVertexPC> vertices;
		std::vector<uint16_t> indices;
	};

	// Returns false on parse error.
	bool ParseNocMeshPC(const uint8_t* bytes, size_t size, CpuMeshPC& out, const char*& outErr);
}
```

---

## `Engine/Render/DX12/MeshFormat.cpp` (NEW)

```cpp
#include "MeshFormat.h"
#include <cstring>

namespace noc
{
	static bool ReadU32_(const uint8_t*& p, const uint8_t* end, uint32_t& out)
	{
		if (p + 4 > end) return false;
		memcpy(&out, p, 4);
		p += 4;
		return true;
	}

	bool ParseNocMeshPC(const uint8_t* bytes, size_t size, CpuMeshPC& out, const char*& outErr)
	{
		outErr = nullptr;
		out.vertices.clear();
		out.indices.clear();

		if (!bytes || size < 16)
		{
			outErr = "mesh: too small";
			return false;
		}

		const uint8_t* p = bytes;
		const uint8_t* end = bytes + size;

		char magic[4]{};
		memcpy(magic, p, 4);
		p += 4;

		if (memcmp(magic, "NMSH", 4) != 0)
		{
			outErr = "mesh: bad magic";
			return false;
		}

		uint32_t ver = 0, vc = 0, ic = 0;
		if (!ReadU32_(p, end, ver) || !ReadU32_(p, end, vc) || !ReadU32_(p, end, ic))
		{
			outErr = "mesh: header truncated";
			return false;
		}

		if (ver != 1)
		{
			outErr = "mesh: unsupported version";
			return false;
		}

		const size_t vBytes = (size_t)vc * sizeof(MeshVertexPC);
		const size_t iBytes = (size_t)ic * sizeof(uint16_t);

		if ((size_t)(end - p) < vBytes + iBytes)
		{
			outErr = "mesh: payload truncated";
			return false;
		}

		out.vertices.resize(vc);
		memcpy(out.vertices.data(), p, vBytes);
		p += vBytes;

		out.indices.resize(ic);
		memcpy(out.indices.data(), p, iBytes);
		p += iBytes;

		return true;
	}
}
```

---

## `Engine/Render/DX12/ShaderCompiler.h` (MODIFIED)

```cpp
#pragma once
#include "Dx12Common.h"
#include <string>

namespace noc
{
	class ShaderCompiler
	{
	public:
		// Compile from an in-memory HLSL string (VFS-backed source).
		static bool CompileFromMemory(
			const char* debugName, // used for error messages
			const char* sourceUtf8,
			size_t sourceBytes,
			const char* entry,
			const char* target,
			dx12::ComPtr<ID3DBlob>& outBytecode);

		// Kept from Phase 9 (if you already had it).
		static bool CompileFromFile(
			const wchar_t* filePath,
			const char* entry,
			const char* target,
			dx12::ComPtr<ID3DBlob>& outBytecode);
	};
}
```

---

## `Engine/Render/DX12/ShaderCompiler.cpp` (MODIFIED)

```cpp
#include "ShaderCompiler.h"

namespace noc
{
	bool ShaderCompiler::CompileFromMemory(
		const char* debugName,
		const char* sourceUtf8,
		size_t sourceBytes,
		const char* entry,
		const char* target,
		dx12::ComPtr<ID3DBlob>& outBytecode)
	{
		outBytecode.Reset();

		if (!sourceUtf8 || sourceBytes == 0 || !entry || !target)
			return false;

		UINT flags = 0;
#if defined(_DEBUG)
		flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		flags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

		dx12::ComPtr<ID3DBlob> errors;
		HRESULT hr = D3DCompile(
			sourceUtf8,
			sourceBytes,
			debugName ? debugName : "noc_shader",
			nullptr,
			nullptr,
			entry,
			target,
			flags,
			0,
			&outBytecode,
			&errors);

		if (FAILED(hr))
		{
			const char* e = errors ? (const char*)errors->GetBufferPointer() : "unknown";
			NOC_LOG_ERROR("Render", "D3DCompile failed (%s:%s/%s): %s", debugName ? debugName : "mem", entry, target, e);
			return false;
		}

		return true;
	}

	bool ShaderCompiler::CompileFromFile(
		const wchar_t* filePath,
		const char* entry,
		const char* target,
		dx12::ComPtr<ID3DBlob>& outBytecode)
	{
		outBytecode.Reset();
		if (!filePath || !entry || !target)
			return false;

		UINT flags = 0;
#if defined(_DEBUG)
		flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		flags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

		dx12::ComPtr<ID3DBlob> errors;
		HRESULT hr = D3DCompileFromFile(
			filePath,
			nullptr,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			entry,
			target,
			flags,
			0,
			&outBytecode,
			&errors);

		if (FAILED(hr))
		{
			const char* e = errors ? (const char*)errors->GetBufferPointer() : "unknown";
			NOC_LOG_ERROR("Render", "D3DCompileFromFile failed: %s", e);
			return false;
		}
		return true;
	}
}
```

---

## `Engine/Render/DX12/MeshPass.h` (NEW — replaces TrianglePass)

```cpp
#pragma once
#include "Dx12Common.h"

#include "Dx12DescriptorAllocator.h"
#include "Dx12DeferredReleaseQueue.h"
#include "Dx12PsoCache.h"
#include "GpuBuffer.h"
#include "GpuRingConstantBuffer.h"
#include "MeshFormat.h"

#include "Resources/ResourceHandle.h"
#include "Resources/Typed/ResourceHandleT.h"
#include "Resources/Typed/TextResource.h"

namespace noc
{
	class ResourceManager;
	class Dx12SwapChain;
	class Dx12FrameSync;

	// Phase 9.5: a minimal “real” draw pass:
	// - shader source from VFS (TextResource)
	// - mesh bytes from VFS (Binary)
	// - default-heap VB/IB
	// - descriptor table for per-frame CBV
	class MeshPass
	{
	public:
		bool Init(
			ID3D12Device* device,
			Dx12DescriptorAllocator& cbvSrvUav,
			Dx12DescriptorAllocator& samplers,
			Dx12PsoCache& psoCache);

		void Shutdown(Dx12DeferredReleaseQueue& deferred, uint64_t safeFenceValue);

		// Called every frame after cmd list is reset and RT is in RT state.
		void Record(
			ID3D12Device* device,
			ID3D12GraphicsCommandList* cmd,
			Dx12SwapChain& swap,
			const Dx12FrameSync& sync,
			uint32_t frameIndex,
			Dx12DeferredReleaseQueue& deferred,
			ResourceManager* rm);

	private:
		bool EnsureRootSigAndPso_(
			ID3D12Device* device,
			Dx12PsoCache& cache,
			ResourceManager* rm);

		bool EnsureMeshUploaded_(
			ID3D12Device* device,
			ID3D12GraphicsCommandList* cmd,
			Dx12DeferredReleaseQueue& deferred,
			const Dx12FrameSync& sync,
			uint32_t frameIndex,
			ResourceManager* rm);

		void EnsurePerFrameCbv_(ID3D12Device* device);

	private:
		// --- Assets (CPU) ---
		ResourceHandle meshBin_{};                 // RequestBinary("Meshes/triangle.nmsh")
		ResourceHandleT<TextResource> shaderHlsl_; // RequestText("Shaders/Basic.hlsl")

		// --- GPU objects ---
		dx12::ComPtr<ID3D12RootSignature> rootSig_;
		dx12::ComPtr<ID3D12PipelineState> pso_;

		GpuBuffer vb_;
		GpuBuffer ib_;
		uint32_t indexCount_ = 0;

		GpuRingConstantBuffer perFrameCB_;
		Dx12DescriptorAllocator* cbvSrvUav_ = nullptr;
		Dx12DescriptorHandle perFrameCbv_[dx12::kFrameCount]{};

		Dx12PsoCache* psoCache_ = nullptr;

		// state flags
		bool rootReady_ = false;
		bool psoReady_ = false;
		bool meshReady_ = false;
		bool cbReady_ = false;
	};
}
```

---

## `Engine/Render/DX12/MeshPass.cpp` (NEW)

```cpp
#include "MeshPass.h"

#include "Dx12SwapChain.h"
#include "Dx12FrameSync.h"
#include "ShaderCompiler.h"

#include "Resources/ResourceManager.h"

namespace noc
{
	struct PerFrameConstants
	{
		float time;
		float pad[3];
	};

	static uint64_t HashInputLayoutPC_()
	{
		// Stable constant for Phase 9.5: position+color layout.
		// (Design choice) Replace with real hashing later.
		return 0xA0C0CA11u;
	}

	bool MeshPass::Init(
		ID3D12Device* device,
		Dx12DescriptorAllocator& cbvSrvUav,
		Dx12DescriptorAllocator& samplers,
		Dx12PsoCache& psoCache)
	{
		(void)samplers;

		if (!device)
			return false;

		cbvSrvUav_ = &cbvSrvUav;
		psoCache_ = &psoCache;

		// Request assets (vpaths are relative to your mounted content root).
		// These return immediately; readiness is polled in Record().
		// NOTE: shader file should live at Data/Shaders/Basic.hlsl, and mesh at Data/Meshes/triangle.nmsh.
		// VFS mount in Phase 3/6 already maps Data/ as root.
		// If your VFS uses different conventions, adjust vpaths accordingly.
		rootReady_ = false;
		psoReady_ = false;
		meshReady_ = false;
		cbReady_ = false;

		// Per-frame constant buffers (upload heap, one per swap buffer).
		if (!perFrameCB_.Init(device, 64 * 1024))
			return false;

		return true;
	}

	void MeshPass::Shutdown(Dx12DeferredReleaseQueue& deferred, uint64_t safeFenceValue)
	{
		// Queue GPU objects for safe release (or release now after GPU idle).
		if (pso_)
		{
			dx12::ComPtr<IUnknown> u;
			pso_.As(&u);
			deferred.Enqueue(safeFenceValue, std::move(u));
			pso_.Reset();
		}
		if (rootSig_)
		{
			dx12::ComPtr<IUnknown> u;
			rootSig_.As(&u);
			deferred.Enqueue(safeFenceValue, std::move(u));
			rootSig_.Reset();
		}

		vb_.ShutdownNow();
		ib_.ShutdownNow();

		perFrameCB_.Shutdown();
	}

	void MeshPass::EnsurePerFrameCbv_(ID3D12Device* device)
	{
		if (cbReady_ || !device || !cbvSrvUav_)
			return;

		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
		{
			perFrameCbv_[i] = cbvSrvUav_->Allocate();

			D3D12_CONSTANT_BUFFER_VIEW_DESC d{};
			d.BufferLocation = perFrameCB_.Resource(i)->GetGPUVirtualAddress();
			// CBV size must be 256-byte aligned.
			d.SizeInBytes = (UINT)((sizeof(PerFrameConstants) + 255u) & ~255u);

			device->CreateConstantBufferView(&d, perFrameCbv_[i].cpu);
		}

		cbReady_ = true;
	}

	bool MeshPass::EnsureRootSigAndPso_(ID3D12Device* device, Dx12PsoCache& cache, ResourceManager* rm)
	{
		if (!device || !rm)
			return false;

		// Request handles once.
		if (!shaderHlsl_.IsValid())
			shaderHlsl_ = rm->RequestText("Shaders/Basic.hlsl");

		// Root signature: build once (no dependency on asset readiness).
		if (!rootReady_)
		{
			// Root parameters:
			// 0: CBV table (b0) per-frame
			// 1: SRV table (t0..t7) per-draw/material (future)
			// 2: Sampler table (s0..s7) (future)
			D3D12_DESCRIPTOR_RANGE ranges[3]{};

			ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
			ranges[0].NumDescriptors = 1;
			ranges[0].BaseShaderRegister = 0;
			ranges[0].RegisterSpace = 0;
			ranges[0].OffsetInDescriptorsFromTableStart = 0;

			ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			ranges[1].NumDescriptors = 8;
			ranges[1].BaseShaderRegister = 0;
			ranges[1].RegisterSpace = 0;
			ranges[1].OffsetInDescriptorsFromTableStart = 0;

			ranges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
			ranges[2].NumDescriptors = 8;
			ranges[2].BaseShaderRegister = 0;
			ranges[2].RegisterSpace = 0;
			ranges[2].OffsetInDescriptorsFromTableStart = 0;

			D3D12_ROOT_PARAMETER params[3]{};

			params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			params[0].DescriptorTable.NumDescriptorRanges = 1;
			params[0].DescriptorTable.pDescriptorRanges = &ranges[0];
			params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

			params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			params[1].DescriptorTable.NumDescriptorRanges = 1;
			params[1].DescriptorTable.pDescriptorRanges = &ranges[1];
			params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

			params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			params[2].DescriptorTable.NumDescriptorRanges = 1;
			params[2].DescriptorTable.pDescriptorRanges = &ranges[2];
			params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

			D3D12_ROOT_SIGNATURE_DESC rs{};
			rs.NumParameters = 3;
			rs.pParameters = params;
			rs.NumStaticSamplers = 0;
			rs.pStaticSamplers = nullptr;
			rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

			dx12::ComPtr<ID3DBlob> blob;
			dx12::ComPtr<ID3DBlob> err;
			if (!dx12::HrOk(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err), "SerializeRootSignature"))
			{
				const char* e = err ? (const char*)err->GetBufferPointer() : "unknown";
				NOC_LOG_ERROR("Render", "RootSig serialize error: %s", e);
				return false;
			}

			if (!dx12::HrOk(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSig_)), "CreateRootSignature"))
				return false;

			rootReady_ = true;
		}

		// Shader compilation requires the TextResource to be ready.
		const TextResource* src = rm->GetText(shaderHlsl_);
		if (!src)
			return false; // not ready yet

		dx12::ComPtr<ID3DBlob> vs;
		dx12::ComPtr<ID3DBlob> ps;

		if (!ShaderCompiler::CompileFromMemory("Shaders/Basic.hlsl", src->Str().c_str(), src->Str().size(), "VSMain", "vs_5_1", vs))
			return false;
		if (!ShaderCompiler::CompileFromMemory("Shaders/Basic.hlsl", src->Str().c_str(), src->Str().size(), "PSMain", "ps_5_1", ps))
			return false;

		// PSO cache lookup.
		Dx12PsoKey key{};
		key.vs = vs.Get();
		key.ps = ps.Get();
		key.rootSig = rootSig_.Get();
		key.rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		key.inputLayoutHash = HashInputLayoutPC_();

		if (auto* cached = cache.Find(key))
		{
			pso_ = cached;
			psoReady_ = true;
			return true;
		}

		// Create PSO
		D3D12_INPUT_ELEMENT_DESC layout[2]{};
		layout[0].SemanticName = "POSITION";
		layout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		layout[0].InputSlot = 0;
		layout[0].AlignedByteOffset = 0;
		layout[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

		layout[1].SemanticName = "COLOR";
		layout[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		layout[1].InputSlot = 0;
		layout[1].AlignedByteOffset = 12;
		layout[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
		pso.pRootSignature = rootSig_.Get();
		pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
		pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
		pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		pso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		pso.DepthStencilState.DepthEnable = FALSE;
		pso.DepthStencilState.StencilEnable = FALSE;
		pso.SampleMask = UINT_MAX;
		pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pso.NumRenderTargets = 1;
		pso.RTVFormats[0] = key.rtvFormat;
		pso.SampleDesc.Count = 1;
		pso.InputLayout = { layout, 2 };

		dx12::ComPtr<ID3D12PipelineState> created;
		if (!dx12::HrOk(device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&created)), "CreateGraphicsPipelineState"))
			return false;

		pso_ = created;
		cache.Insert(key, std::move(created));
		psoReady_ = true;
		return true;
	}

	bool MeshPass::EnsureMeshUploaded_(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmd,
		Dx12DeferredReleaseQueue& deferred,
		const Dx12FrameSync& sync,
		uint32_t frameIndex,
		ResourceManager* rm)
	{
		if (meshReady_)
			return true;

		if (!meshBin_.IsValid())
			meshBin_ = rm->RequestBinary("Meshes/triangle.nmsh");

		if (!rm->IsReady(meshBin_))
			return false;

		const uint8_t* bytes = rm->GetBytes(meshBin_);
		const size_t size = rm->GetSize(meshBin_);
		if (!bytes || size == 0)
			return false;

		CpuMeshPC cpu{};
		const char* err = nullptr;
		if (!ParseNocMeshPC(bytes, size, cpu, err))
		{
			NOC_LOG_ERROR("Render", "Mesh parse failed: %s", err ? err : "unknown");
			return false;
		}

		indexCount_ = (uint32_t)cpu.indices.size();

		if (!vb_.CreateStatic(device, cmd, deferred, sync, frameIndex, GpuBuffer::Kind::Vertex,
			cpu.vertices.data(), cpu.vertices.size() * sizeof(MeshVertexPC), sizeof(MeshVertexPC)))
			return false;

		if (!ib_.CreateStatic(device, cmd, deferred, sync, frameIndex, GpuBuffer::Kind::Index,
			cpu.indices.data(), cpu.indices.size() * sizeof(uint16_t), 0))
			return false;

		meshReady_ = true;
		return true;
	}

	void MeshPass::Record(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmd,
		Dx12SwapChain& swap,
		const Dx12FrameSync& sync,
		uint32_t frameIndex,
		Dx12DeferredReleaseQueue& deferred,
		ResourceManager* rm)
	{
		if (!device || !cmd)
			return;

		// Always ensure per-frame CBV descriptors exist.
		EnsurePerFrameCbv_(device);

		// Clear RT (swap already provides RTV).
		auto rtv = swap.CurrentRtv(frameIndex);
		cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

		const float clearColor[4] = { 0.05f, 0.05f, 0.08f, 1.0f };
		cmd->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

		if (!rm)
			return;

		// Build root/PSO when shader becomes ready.
		if (!EnsureRootSigAndPso_(device, *psoCache_, rm))
			return;

		// Upload mesh when bytes become ready.
		if (!EnsureMeshUploaded_(device, cmd, deferred, sync, frameIndex, rm))
			return;

		// Write per-frame constants.
		perFrameCB_.BeginFrame(frameIndex);
		D3D12_GPU_VIRTUAL_ADDRESS gpu = 0;
		void* cpu = nullptr;
		if (perFrameCB_.Allocate(sizeof(PerFrameConstants), gpu, cpu))
		{
			auto* c = (PerFrameConstants*)cpu;
			c->time = 0.0f; // (Design choice) wire real time later
		}

		// Bind descriptor heaps (shader-visible).
		ID3D12DescriptorHeap* heaps[] = { cbvSrvUav_->Heap() };
		cmd->SetDescriptorHeaps(1, heaps);

		cmd->SetGraphicsRootSignature(rootSig_.Get());
		cmd->SetPipelineState(pso_.Get());

		// Root slot 0: per-frame CBV table (b0)
		cmd->SetGraphicsRootDescriptorTable(0, perFrameCbv_[frameIndex].gpu);

		// Viewport/scissor from swapchain size.
		D3D12_VIEWPORT vp{};
		vp.Width = (float)swap.Width();
		vp.Height = (float)swap.Height();
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;

		D3D12_RECT sc{};
		sc.left = 0;
		sc.top = 0;
		sc.right = (LONG)swap.Width();
		sc.bottom = (LONG)swap.Height();

		cmd->RSSetViewports(1, &vp);
		cmd->RSSetScissorRects(1, &sc);

		// IA bind
		auto vbv = vb_.VertexView();
		auto ibv = ib_.IndexView(DXGI_FORMAT_R16_UINT);

		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmd->IASetVertexBuffers(0, 1, &vbv);
		cmd->IASetIndexBuffer(&ibv);

		cmd->DrawIndexedInstanced(indexCount_, 1, 0, 0, 0);
	}
}
```

---

## `Engine/Render/DX12/Dx12Renderer.h` (MODIFIED)

```cpp
#pragma once
#include "Dx12Common.h"

#include "Dx12Device.h"
#include "Dx12SwapChain.h"
#include "Dx12FrameSync.h"

#include "Dx12DescriptorAllocator.h"
#include "Dx12DeferredReleaseQueue.h"
#include "Dx12PsoCache.h"
#include "MeshPass.h"

namespace noc
{
	class ResourceManager;

	class Dx12Renderer
	{
	public:
		bool Init(bool enableDebugLayer);
		void Shutdown();

		bool AttachToWindow(void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight);

		void SetResourceManager(ResourceManager* rm) { rm_ = rm; }

		void BeginFrame();
		void EndFramePresent();

	private:
		void LogDeviceRemoved_(const char* where);

	private:
		bool inited_ = false;
		bool attached_ = false;

		ResourceManager* rm_ = nullptr;

		Dx12Device device_;
		Dx12SwapChain swap_;
		Dx12FrameSync sync_;

		dx12::ComPtr<ID3D12CommandAllocator> cmdAlloc_[dx12::kFrameCount];
		dx12::ComPtr<ID3D12GraphicsCommandList> cmdList_;

		uint32_t frameIndex_ = 0;
		bool frameOpen_ = false;

		// Phase 9.5: resource foundation
		Dx12DescriptorAllocator cbvSrvUavHeap_;
		Dx12DescriptorAllocator samplerHeap_; // not used yet, but reserved for materials
		Dx12DeferredReleaseQueue deferred_;
		Dx12PsoCache psoCache_;

		MeshPass meshPass_;
	};
}
```

---

## `Engine/Render/DX12/Dx12Renderer.cpp` (MODIFIED)

```cpp
#include "Dx12Renderer.h"

#include "Resources/ResourceManager.h"

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

		// After GPU idle, it is safe to clear deferred queue immediately.
		deferred_.Clear();

		meshPass_.Shutdown(deferred_, /*safeFenceValue*/sync_.CompletedValue());

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
			0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc_[frameIndex_].Get(), nullptr, IID_PPV_ARGS(&cmdList_)),
			"CreateCommandList"))
		{
			return false;
		}
		dx12::HrOk(cmdList_->Close(), "cmdList->Close (initial)");

		// Phase 9.5: descriptor heaps (persistent; no per-frame recreation).
		if (!cbvSrvUavHeap_.Init(device_.Device(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024, true))
			return false;
		if (!samplerHeap_.Init(device_.Device(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 64, true))
			return false;

		// Phase 9.5: mesh pass
		if (!meshPass_.Init(device_.Device(), cbvSrvUavHeap_, samplerHeap_, psoCache_))
			return false;

		attached_ = true;
		NOC_LOG_INFO("Render", "Dx12Renderer attached (%ux%u)", clientWidth, clientHeight);
		return true;
	}

	void Dx12Renderer::BeginFrame()
	{
		if (!attached_)
			return;

		if (device_.Device()->GetDeviceRemovedReason() != S_OK)
			return;

		if (frameOpen_)
		{
			NOC_LOG_ERROR("Render", "BeginFrame called while frame is already open");
			return;
		}
		frameOpen_ = true;

		// Collect deferred releases for everything the GPU has finished.
		deferred_.Collect(sync_.CompletedValue());

		// Reset allocator/list for current back buffer.
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

		// Transition Present->RT for current back buffer.
		swap_.TransitionTo(cmdList_.Get(), frameIndex_, D3D12_RESOURCE_STATE_RENDER_TARGET);
	}

	void Dx12Renderer::EndFramePresent()
	{
		if (!attached_)
			return;

		if (device_.Device()->GetDeviceRemovedReason() != S_OK)
			return;

		if (!frameOpen_)
		{
			NOC_LOG_ERROR("Render", "EndFramePresent called without BeginFrame");
			return;
		}
		struct Guard { bool& b; ~Guard() { b = false; } } g{ frameOpen_ };

		// Record pass into cmd list (clear + draw if assets ready).
		meshPass_.Record(device_.Device(), cmdList_.Get(), swap_, sync_, frameIndex_, deferred_, rm_);

		// Transition RT->Present.
		swap_.TransitionTo(cmdList_.Get(), frameIndex_, D3D12_RESOURCE_STATE_PRESENT);

		if (FAILED(cmdList_->Close()))
		{
			LogDeviceRemoved_("cmdList->Close");
			return;
		}

		ID3D12CommandList* lists[] = { cmdList_.Get() };
		device_.Queue()->ExecuteCommandLists(1, lists);

		// Present
		swap_.Present();

		// Signal + advance/wait using Phase 8/9 model.
		sync_.MoveToNextFrame(device_.Queue(), swap_.SwapChain(), frameIndex_);
		swap_.UpdateFrameIndex(frameIndex_);
	}

	void Dx12Renderer::LogDeviceRemoved_(const char* where)
	{
		HRESULT hr = device_.Device()->GetDeviceRemovedReason();
		NOC_LOG_ERROR("Render", "Device removed at %s (hr=0x%08X)", where, (unsigned)hr);
	}
}
```

---

## `Engine/Render/DX12/Dx12SwapChain.h` (MODIFIED — small additions)

```cpp
#pragma once
#include "Dx12Common.h"
#include <cstdint>

namespace noc
{
	class Dx12SwapChain
	{
	public:
		bool Init(IDXGIFactory6* factory, ID3D12CommandQueue* queue, void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight);
		void Shutdown();

		bool CreateRtvHeapAndViews(ID3D12Device* device);

		void Present();
		uint32_t FrameIndex() const { return frameIndex_; }
		void UpdateFrameIndex(uint32_t i) { frameIndex_ = i; }

		IDXGISwapChain3* SwapChain() const { return swapChain_.Get(); }

		uint32_t Width() const { return width_; }
		uint32_t Height() const { return height_; }

		D3D12_CPU_DESCRIPTOR_HANDLE CurrentRtv(uint32_t frameIndex) const;

		void TransitionTo(ID3D12GraphicsCommandList* cmd, uint32_t frameIndex, D3D12_RESOURCE_STATES to);

	private:
		uint32_t width_ = 0, height_ = 0;
		uint32_t frameIndex_ = 0;

		dx12::ComPtr<IDXGISwapChain3> swapChain_;

		dx12::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
		uint32_t rtvDescriptorSize_ = 0;

		dx12::ComPtr<ID3D12Resource> backBuffers_[dx12::kFrameCount];
		D3D12_RESOURCE_STATES bbState_[dx12::kFrameCount]{};
	};
}
```

---

## `Engine/Render/DX12/Dx12SwapChain.cpp` (MODIFIED — provides helpers used by MeshPass)

```cpp
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
		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
			backBuffers_[i].Reset();

		rtvHeap_.Reset();
		swapChain_.Reset();
		width_ = height_ = 0;
		frameIndex_ = 0;
		rtvDescriptorSize_ = 0;
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

	D3D12_CPU_DESCRIPTOR_HANDLE Dx12SwapChain::CurrentRtv(uint32_t frameIndex) const
	{
		D3D12_CPU_DESCRIPTOR_HANDLE h{};
		if (!rtvHeap_)
			return h;

		h = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
		h.ptr += (SIZE_T)frameIndex * (SIZE_T)rtvDescriptorSize_;
		return h;
	}

	void Dx12SwapChain::TransitionTo(ID3D12GraphicsCommandList* cmd, uint32_t frameIndex, D3D12_RESOURCE_STATES to)
	{
		if (!cmd)
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
```

---

## `Engine/Runtime/Engine.cpp` (MODIFIED — wire ResourceManager into RenderSystem)

> Keep everything else as-is; Phase 9.5 adds one call after render init.

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
		// --- existing init path (core + vfs + jobs + resources + input) ---
		if (!registry_.StartupAll(this))
			return false;

#if NOC_ENABLE_ASSERTS
		const bool enableDebugLayer = true;
#else
		const bool enableDebugLayer = false;
#endif
		if (!render_.Init(enableDebugLayer))
			return false;

		// Phase 9.5: allow renderer to request assets through the ResourceManager.
		render_.SetResourceManager(&resources_);

		initialized_ = true;
		return true;
	}

	// ... rest of file unchanged (AttachWindow/Run/BeginFrame/Tick/EndFrame/Shutdown) ...
}
```

---

# 6) Verification checklist (Phase 9.5 done when…)

**Assets**

* [ ] You have `Data/Shaders/Basic.hlsl` (see below sample) and `Data/Meshes/triangle.nmsh` (binary in the `NMSH` format).
* [ ] Logs show resources become READY (shader text + mesh binary).

**GPU correctness**

* [ ] The triangle/mesh is drawn using **default heap** vertex/index buffers (not upload-only).
* [ ] Constant buffer updates do not stall and are aligned to 256 bytes.
* [ ] Descriptor heaps are created once and reused (no per-frame recreation).

**Synchronization & lifetime**

* [ ] No crash on shutdown; renderer waits for GPU then releases.
* [ ] No debug layer errors about:

  * resource state
  * descriptor heap binding
  * releasing in-use resources

**Debug layer**

* [ ] **0 D3D12 debug layer errors** while running and when closing the window.

---

# 7) Common pitfalls (Phase 9.5)

* **Destroying upload buffers too early**: staging resources must outlive the GPU copy. We solve this by deferred release tagged with the frame fence value that will be signaled.
* **CBV size not 256B aligned**: CBVs require 256-byte alignment; we enforce it in `GpuRingConstantBuffer` and CBV creation.
* **Forgetting to bind descriptor heaps before `SetGraphicsRootDescriptorTable`**: leads to debug errors or invalid bindings.
* **Using the wrong resource state**: default buffers must be transitioned from `COPY_DEST` to `VERTEX_AND_CONSTANT_BUFFER` / `INDEX_BUFFER`.
* **Per-frame heap recreation**: forbidden; we allocate from persistent heaps.

---

# 8) Next chat handoff (ONLY what you should say/bring next)

> “Phase 9.5 is implemented. We now have default-heap GPU buffers with upload staging, persistent CBV/SRV/UAV + sampler descriptor heaps with stable handles, a minimal root signature with descriptor tables, shader source loaded from VFS (TextResource) and compiled at runtime, a PSO cache, asset-backed mesh upload from Binary resource, and GPU-safe deferred destruction keyed off fence completion. The MeshPass draws indexed geometry with zero D3D12 debug errors. Start Phase 11 — Textures, SRVs, Samplers, and a Minimal Material System (VFS textures → GPU textures → SRV descriptors → per-material bindings).”

---

## Appendix: `Data/Shaders/Basic.hlsl` (you create this file)

```hlsl
struct VSIn
{
    float3 pos   : POSITION;
    float4 color : COLOR;
};

struct VSOut
{
    float4 pos   : SV_Position;
    float4 color : COLOR;
};

cbuffer PerFrame : register(b0)
{
    float gTime;
    float3 _pad;
};

VSOut VSMain(VSIn v)
{
    VSOut o;
    o.pos = float4(v.pos, 1.0);
    o.color = v.color;
    return o;
}

float4 PSMain(VSOut i) : SV_Target
{
    return i.color;
}
```

If you want, I can also give you a tiny C++ helper to **author** `triangle.nmsh` from a struct (so you don’t hand-roll the binary).
