# Phase 5 — Typed Resources & Loader Registry

> **Status:** READY FOR IMPLEMENTATION ⏳  
> **Scope:** Typed resources on top of Phase 4 binary blobs: loader contracts, loader registry, one concrete typed resource example  
> **Depends on:** Phase 3 — VFS, Phase 4 — Resource Manager (binary foundation) :contentReference[oaicite:0]{index=0}  
>
> **Primary source of truth:** Jason Gregory, *Game Engine Architecture (3rd Ed.)*  
> **Architecture alignment:** “Resources/Typed → IResourceLoader → ResourceLoaderRegistry” in the Full Architecture Diagram :contentReference[oaicite:1]{index=1}

This document is the **authoritative design and implementation reference** for Phase 5 of Nocturne Engine.

Phase 4 gave us **engine-owned async I/O + caching** for **raw bytes**. :contentReference[oaicite:2]{index=2}  
Phase 5 turns those bytes into **typed runtime assets** via a **loader registry** + **typed handles**, while preserving the same engine principles: stable identity, caching, async loading, deterministic lifecycle.

---

## Phase name + objective

**Objective:** Extend the Phase 4 Resource Manager to support **typed resource requests** (e.g., Text, JSON later, Image later) using:

- a **ResourceType** tag on each record,
- a **ResourceLoaderRegistry** that selects the correct loader,
- an **IResourceLoader** contract that defines threading + ownership rules,
- one real example: **TextResource** (UTF-8 text file) end-to-end.

This keeps “files are not resources” intact: vpaths identify *what you want*, loaders define *how bytes become a runtime object*. :contentReference[oaicite:3]{index=3}

---

## Key concepts from the books

- **Resource identity and caching**: Resources need stable IDs and a manager to avoid duplicate loads and centralize lifecycle. (Gregory; Phase 4 implements the “foundation” of this.) :contentReference[oaicite:4]{index=4}
- **Asynchronous producer/consumer handoff**: When work spans threads, you need a queue/hand-off point and a clear “who owns processing” rule. This is the same architectural shape as an event-queue spanning threads. :contentReference[oaicite:5]{index=5}
- **Narrow interfaces**: Keep typed resource complexity behind the resource system; don’t leak loader details into gameplay code. (Pattern-aligned guidance on minimizing exposed surface area.) :contentReference[oaicite:6]{index=6}

---

## What we implement now

Tight scope (Phase 5 is *not* a job system, *not* rendering, *not* GPU uploads):

1. **ResourceType enum** (starts small).
2. **Typed handle** `ResourceHandleT<T>` (wraps existing `ResourceHandle`).
3. **IResourceLoader** interface:
   - advertises `ResourceType`
   - validates/decodes bytes into a runtime object
4. **ResourceLoaderRegistry**:
   - `RegisterLoader(type, loader*)`
   - `FindLoader(type) -> loader*`
5. **ResourceManager typed API**:
   - `RequestText(vpath)` returns `ResourceHandleT<TextResource>`
   - `GetText(handle)` returns pointer/ref when ready
6. **Concrete example**: **TextResource** + **TextResourceLoader**
   - loader reads bytes (VFS) via existing Phase 4 path
   - decodes UTF-8 into `std::string`
   - main thread finalizes state to READY/FAILED
7. **NocturneHost test**:
   - request `"hello.txt"` as Text
   - log its size + preview

**Design choice (not directly from the book):**
- We keep the Phase 4 dedicated loader thread and add typed decoding there (safe for CPU-only types). A job system replaces this later. :contentReference[oaicite:7]{index=7}

---

## Typed Resource Lifecycle (authoritative)

This is the lifecycle that every typed resource must follow.

### 1) Request
- Call: `RequestText("path.txt")`
- ResourceManager:
  - normalizes vpath → ResourceID (Phase 4)
  - looks up cache:
    - if present, returns existing handle
    - else creates a new record:
      - `state = REQUESTED`
      - `type = ResourceType::Text`
      - `loader = registry.FindLoader(Text)`
- Enqueues request for loader thread.

### 2) Load (loader thread)
- ResourceManager performs **I/O** via VFS (Phase 4 style).
- On success: passes raw bytes to loader for **Decode**:
  - `Decode(bytes) -> object or error`
- Stores decoded object into a “completed” payload and pushes it to the completion queue.

### 3) Finalize (main thread)
- `ResourceManager::Update()` drains completion queue.
- For each completed item:
  - installs the decoded object into the record
  - transitions:
    - READY if decode succeeded
    - FAILED if I/O or decode failed
- From here on:
  - `GetText(handle)` is valid iff READY
  - cached object is shared by all requesters

### 4) Use
- Gameplay/systems only see:
  - `ResourceHandleT<TextResource>`
  - `const TextResource*` (or `std::string_view`)
- No file paths, no loaders, no VFS knowledge outside resources.

### 5) Shutdown
- ResourceManager shuts down loader thread
- Clears records after thread joins (no concurrent access).

This matches the diagram’s intent: async worker produces results, main thread finalizes ownership/state. :contentReference[oaicite:8]{index=8}

---

## Loader contracts (non-negotiable)

### A) Registration & identity
- Each loader handles exactly one `ResourceType` (initially).
- Registry registration must happen during engine init (or subsystem startup), before gameplay requests.

### B) Threading rules
- `Decode()` **runs on the loader thread** in Phase 5.
  - It must be **CPU-only** and must not touch OS windowing, input state, GPU objects, or global singletons that are not thread-safe.
- `Finalize()` is performed by the ResourceManager on the **main thread** (inside `Resources().Update()`).

Why: we want the same shape as a producer/consumer queue spanning threads. :contentReference[oaicite:9]{index=9}

### C) Ownership rules
- A loader returns a result whose memory is **owned by ResourceManager** after completion.
- A typed resource object becomes immutable once READY.
- Consumers must treat returned pointers/refs as read-only.

### D) Error handling
- Loader must produce a stable error string on failure.
- ResourceManager stores the error on the record for debugging:
  - `HasFailed(handle)`
  - `GetError(handle)`

### E) Determinism
- Given identical bytes and loader version, `Decode()` must be deterministic.

---

## Implementation steps

1. Add **typed resource core types**:
   - `ResourceType`
   - `ResourceHandleT<T>`
   - `ResourceLoadContext`, `ResourceLoadResult`
2. Add loader interfaces:
   - `IResourceLoader`
   - `ResourceLoaderRegistry`
3. Extend `ResourceManager`:
   - add typed request path
   - store per-record: `ResourceType`, decoded object pointer (type-erased)
4. Implement **TextResource + TextResourceLoader**
5. Integrate into engine startup:
   - create registry
   - register `TextResourceLoader`
6. Host test:
   - request text resource
   - log success + preview

---

## Verification checklist (Phase 5 done when…)

- [ ] Engine boots and initializes ResourceLoaderRegistry.
- [ ] `TextResourceLoader` is registered.
- [ ] `RequestText("hello.txt")` returns a handle immediately (non-blocking).
- [ ] Within a few frames, resource transitions to READY.
- [ ] You can call `GetText(handle)` and log:
  - full size
  - first N characters preview
- [ ] Duplicate requests for the same vpath return the same cached resource (no double-load).
- [ ] Bad path transitions to FAILED and provides error string.
- [ ] Shutdown joins loader thread and exits cleanly (no leaks, no deadlocks).

---

## Common pitfalls

- **Letting loaders touch GPU or renderer state**: that will deadlock later when a render thread exists.
- **Returning views into temporary memory**: decoded objects must outlive the completion handoff.
- **Skipping main-thread finalize**: READY/FAILED transitions must be single-threaded to keep record state simple.
- **Leaking loader details into gameplay**: gameplay should not know “which loader” or “where bytes came from”.

---

## Implementations

> Notes:
> - Paths assume your existing layout (`Engine/Resources/...`) from Phase 4. :contentReference[oaicite:10]{index=10}
> - Names follow the architecture diagram vocabulary. :contentReference[oaicite:11]{index=11}

### Engine/Resources/Typed/ResourceType.h
```cpp
#pragma once
#include <cstdint>

namespace noc {

enum class ResourceType : uint8_t {
    Unknown = 0,
    Binary,
    Text,
    // Future:
    // Json,
    // Image,
    // Mesh,
};

} // namespace noc
````

### Engine/Resources/Typed/ResourceHandleT.h

```cpp
#pragma once
#include "Engine/Resources/ResourceHandle.h"

namespace noc {

// Strongly-typed wrapper around an untyped ResourceHandle.
template <class T>
class ResourceHandleT {
public:
    ResourceHandleT() = default;
    explicit ResourceHandleT(ResourceHandle h) : h_(h) {}

    ResourceHandle Untyped() const { return h_; }
    bool IsValid() const { return h_.IsValid(); }

    friend bool operator==(const ResourceHandleT& a, const ResourceHandleT& b) { return a.h_ == b.h_; }
    friend bool operator!=(const ResourceHandleT& a, const ResourceHandleT& b) { return !(a == b); }

private:
    ResourceHandle h_{};
};

} // namespace noc
```

### Engine/Resources/Typed/IResourceLoader.h

```cpp
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

#include "Engine/Resources/Typed/ResourceType.h"

namespace noc {

struct ResourceLoadResult {
    bool ok = false;
    std::string error;

    // Type-erased pointer to decoded runtime object.
    // Owned by ResourceManager after completion; must be heap allocated.
    void* object = nullptr;
};

class IResourceLoader {
public:
    virtual ~IResourceLoader() = default;

    virtual ResourceType Type() const = 0;

    // Optional: basic sniffing based on vpath extension, etc.
    // In Phase 5 we route primarily by ResourceType at request time.
    virtual bool CanLoad(const char* /*normalizedVPath*/) const { return true; }

    // Runs on the loader thread in Phase 5.
    // Must be CPU-only and thread-safe.
    virtual ResourceLoadResult Decode(const uint8_t* bytes, size_t size) = 0;
};

} // namespace noc
```

### Engine/Resources/Typed/ResourceLoaderRegistry.h

```cpp
#pragma once
#include <array>
#include <cstddef>

#include "Engine/Resources/Typed/IResourceLoader.h"
#include "Engine/Resources/Typed/ResourceType.h"

namespace noc {

class ResourceLoaderRegistry {
public:
    ResourceLoaderRegistry() { loaders_.fill(nullptr); }

    bool RegisterLoader(IResourceLoader* loader) {
        if (!loader) return false;
        const auto t = loader->Type();
        const auto idx = static_cast<size_t>(t);
        if (idx >= loaders_.size()) return false;
        loaders_[idx] = loader;
        return true;
    }

    IResourceLoader* FindLoader(ResourceType t) const {
        const auto idx = static_cast<size_t>(t);
        if (idx >= loaders_.size()) return nullptr;
        return loaders_[idx];
    }

private:
    // Small fixed table; expand as ResourceType grows.
    std::array<IResourceLoader*, 16> loaders_{};
};

} // namespace noc
```

### Engine/Resources/Typed/TextResource.h

```cpp
#pragma once
#include <string>
#include <string_view>

namespace noc {

class TextResource {
public:
    explicit TextResource(std::string s) : text_(std::move(s)) {}

    std::string_view View() const { return text_; }
    const std::string& Str() const { return text_; }
    size_t Size() const { return text_.size(); }

private:
    std::string text_;
};

} // namespace noc
```

### Engine/Resources/Typed/TextResourceLoader.h

```cpp
#pragma once
#include "Engine/Resources/Typed/IResourceLoader.h"
#include "Engine/Resources/Typed/TextResource.h"

namespace noc {

class TextResourceLoader final : public IResourceLoader {
public:
    ResourceType Type() const override { return ResourceType::Text; }

    ResourceLoadResult Decode(const uint8_t* bytes, size_t size) override {
        ResourceLoadResult r{};
        if (!bytes && size != 0) { r.ok = false; r.error = "Text decode: null bytes"; return r; }

        // Design choice (not directly from the book):
        // Treat input bytes as UTF-8 without validation (good enough for Phase 5).
        // Later: validate UTF-8 and/or support UTF-16.
        std::string s(reinterpret_cast<const char*>(bytes), reinterpret_cast<const char*>(bytes) + size);

        // Heap allocate runtime object; ResourceManager takes ownership.
        r.object = new TextResource(std::move(s));
        r.ok = true;
        return r;
    }
};

} // namespace noc
```

### Engine/Resources/ResourceManager.h

```cpp
#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "Resources/ResourceHandle.h"
#include "Resources/Typed/ResourceLoaderRegistry.h"
#include "Resources/Typed/ResourceHandleT.h"
#include "Resources/Typed/TextResource.h"
#include "Resources/Typed/ResourceType.h"

namespace noc
{
    class Engine;
    class VirtualFileSystem;

    class ResourceManager
    {
    public:
        ResourceManager() = default;
        ~ResourceManager();

        ResourceManager(const ResourceManager&) = delete;
        ResourceManager& operator=(const ResourceManager&) = delete;

        bool Init(Engine& engine, VirtualFileSystem& vfs);
        void Shutdown();

        // Call once per frame on the main thread.
        void Update();

        // ---- Binary blob API (Phase 4 scope) ----
        ResourceHandle RequestBinary(std::string_view vpath);

        bool IsReady(ResourceHandle h) const;
        bool HasFailed(ResourceHandle h) const;

        const uint8_t* GetBytes(ResourceHandle h) const;
        size_t GetSize(ResourceHandle h) const;

        // Returns nullptr if:
        // - handle invalid
        // - resource has not failed
        // Otherwise returns a stable, null-terminated error message.
        const char* GetError(ResourceHandle h) const;

        // Optional helper for host-side testing.
        bool WaitUntilReady(ResourceHandle h, uint32_t timeoutMs);

        ResourceHandleT<TextResource> RequestText(const char* vpath);

        const TextResource* GetText(ResourceHandleT<TextResource> h) const;

        ResourceLoaderRegistry& Loaders() { return loaders_; }
        const ResourceLoaderRegistry& Loaders() const { return loaders_; }

    private:
        bool ValidateHandle_(ResourceHandle h, uint32_t* outIndex) const;
        void LoaderThreadMain_();

    private:
        Engine* engine_ = nullptr;
        VirtualFileSystem* vfs_ = nullptr;

        // Internal opaque state (allocated in .cpp)
        void* state_ = nullptr;

        void* loaderThread_ = nullptr;
        bool running_ = false;

        ResourceLoaderRegistry loaders_;
    };

} // namespace noc
```

### Engine/Resources/ResourceManager.cpp (Phase 5 additions/updates)

```cpp
#include "ResourceManager.h"

#include <atomic>
#include <condition_variable>
#include <chrono>
#include <cstring>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <memory>
#include <vector>

#include "Core/Assert.h"
#include "Core/Log.h"
#include "Core/Memory/Allocator.h"
#include "Resources/ResourceID.h"
#include "Resources/VirtualFileSystem.h"
#include "Resources/VPath.h"
#include "Resources/FileHandle.h"
#include "Runtime/Engine.h"
#include "Resources/Typed/TextResource.h"
#include "Resources/Typed/TextResourceLoader.h"

namespace noc
{
	static TextResourceLoader g_textLoader;

	enum class ResourceState : uint8_t
	{
		Empty = 0,
		Requested,
		Loading,
		Ready,
		Failed
	};

	struct Record
	{
		ResourceID id{};
		std::string vpathNormalized;
		std::atomic<ResourceState> state{ ResourceState::Empty };

		uint32_t generation = 1;

		uint8_t* data = nullptr;
		size_t size = 0;

		std::string error;

		// -------- Phase 5 additions --------
		ResourceType type = ResourceType::Binary; // default matches Phase 4 behavior
		void* typedObject = nullptr;              // points to decoded runtime object when READY

		Record() = default;
		Record(const Record&) = delete;
		Record& operator=(const Record&) = delete;
		Record(Record&&) = delete;
		Record& operator=(Record&&) = delete;
	};


	struct PendingReq { uint32_t index = 0; };

	struct CompletedReq
	{
		uint32_t index = 0;
		bool ok = false;

		// Phase 5: what kind of payload is being completed?
		ResourceType type = ResourceType::Binary;

		// Binary payload (Phase 4)
		std::vector<uint8_t> bytes;

		// Typed payload (Phase 5): heap object produced by loader Decode()
		void* typedObject = nullptr;

		std::string error;
	};


	struct InternalState
	{
		std::mutex mtx;
		std::condition_variable cv;
		std::queue<PendingReq> pending;
		std::queue<CompletedReq> completed;

		std::unordered_map<uint64_t, uint32_t> idToIndex;

		std::vector<std::unique_ptr<Record>> records;
	};

	static std::string NormalizeVPath_(std::string_view vpath)
	{
		char buf[512]{};
		if (!noc::vpath::NormalizeToRelative(buf, sizeof(buf), vpath))
			return {};
		return std::string(buf);
	}

	static uint64_t MakeCacheKey_(uint64_t idValue, ResourceType type)
	{
		// Combine id + type into a single stable key.
		// idValue is already a hash; xor-mix the type.
		return idValue ^ (static_cast<uint64_t>(type) * 0x9E3779B97F4A7C15ull);
	}

	ResourceManager::~ResourceManager()
	{
		Shutdown();
	}

	bool ResourceManager::Init(Engine& engine, VirtualFileSystem& vfs)
	{
		if (running_)
			return true;

		engine_ = &engine;
		vfs_ = &vfs;

		auto* st = new InternalState();
		st->records.reserve(256);
		state_ = st;

		// Register built-in loaders
		loaders_.RegisterLoader(&g_textLoader);

		running_ = true;
		loaderThread_ = new std::thread([this]() { LoaderThreadMain_(); });

		NOC_LOG_INFO("Res", "ResourceManager initialized (loader thread started)");
		return true;
	}

	void ResourceManager::Shutdown()
	{
		if (!running_)
			return;

		running_ = false;

		auto* st = static_cast<InternalState*>(state_);
		st->cv.notify_all();

		if (loaderThread_)
		{
			auto* t = static_cast<std::thread*>(loaderThread_);
			if (t->joinable())
				t->join();
			delete t;
			loaderThread_ = nullptr;
		}

		// Free blobs
		for (auto& rp : st->records)
		{
			if (!rp) continue;

			if (rp->typedObject)
			{
				if (rp->type == ResourceType::Text)
					delete static_cast<TextResource*>(rp->typedObject);

				rp->typedObject = nullptr;
			}

			if (rp->data)
			{
				engine_->Allocator().Deallocate(rp->data);
				rp->data = nullptr;
				rp->size = 0;
			}
		}


		delete st;
		state_ = nullptr;

		engine_ = nullptr;
		vfs_ = nullptr;

		NOC_LOG_INFO("Res", "ResourceManager shutdown complete");
	}

	bool ResourceManager::ValidateHandle_(ResourceHandle h, uint32_t* outIndex) const
	{
		if (!h.IsValid() || !state_)
			return false;

		auto* st = static_cast<InternalState*>(state_);
		if (h.index >= st->records.size())
			return false;

		const Record* r = st->records[h.index].get();
		if (!r)
			return false;

		if (r->generation != h.generation)
			return false;

		*outIndex = h.index;
		return true;
	}

	ResourceHandle ResourceManager::RequestBinary(std::string_view vpath)
	{
		if (!running_ || !state_)
			return {};

		auto* st = static_cast<InternalState*>(state_);

		const std::string norm = NormalizeVPath_(vpath);
		if (norm.empty())
		{
			NOC_LOG_ERROR("Res", "Invalid vpath: %.*s", (int)vpath.size(), vpath.data());
			return {};
		}

		const ResourceID id = MakeResourceID(norm);

		// Get key
		const uint64_t key = MakeCacheKey_(id.value, ResourceType::Binary);

		// Look up existing
		{
			std::lock_guard<std::mutex> lock(st->mtx);

			auto it = st->idToIndex.find(key);
			if (it != st->idToIndex.end())
			{
				const uint32_t idx = it->second;
				Record* r = st->records[idx].get();
				return ResourceHandle{ idx, r->generation };
			}
		}

		// Create new record (stable index)
		auto rec = std::make_unique<Record>();
		rec->id = id;
		rec->vpathNormalized = norm;
		rec->type = ResourceType::Binary;
		rec->typedObject = nullptr;
		rec->state.store(ResourceState::Requested, std::memory_order_release);

		uint32_t index = 0;
		{
			std::lock_guard<std::mutex> lock(st->mtx);
			index = (uint32_t)st->records.size();
			st->records.push_back(std::move(rec));
			st->idToIndex.emplace(key, index);
		}

		// Enqueue load
		Record* r = st->records[index].get();
		r->state.store(ResourceState::Loading, std::memory_order_release);

		{
			std::lock_guard<std::mutex> lock(st->mtx);
			st->pending.push(PendingReq{ index });
		}
		st->cv.notify_one();

		return ResourceHandle{ index, r->generation };
	}

	void ResourceManager::Update()
	{
		if (!running_ || !state_)
			return;

		auto* st = static_cast<InternalState*>(state_);

		for (;;)
		{
			CompletedReq c{};
			{
				std::lock_guard<std::mutex> lock(st->mtx);
				if (st->completed.empty())
					break;
				c = std::move(st->completed.front());
				st->completed.pop();
			}

			if (c.index >= st->records.size())
				continue;

			// Lock while mutating record's non-atomic fields (string, pointers, sizes).
			std::lock_guard<std::mutex> lock(st->mtx);

			Record& r = *st->records[c.index];

			if (!c.ok)
			{
				if (c.typedObject)
				{
					// Phase 5 currently only has TextResource typed objects.
					delete static_cast<TextResource*>(c.typedObject);
					c.typedObject = nullptr;
				}

				r.error = std::move(c.error);
				r.state.store(ResourceState::Failed, std::memory_order_release);
				NOC_LOG_ERROR("Res", "FAILED: %s (%s)", r.vpathNormalized.c_str(), r.error.c_str());
				continue;
			}

			// success
			r.error.clear();

			if (c.type == ResourceType::Binary)
			{
				if (r.data)
				{
					engine_->Allocator().Deallocate(r.data);
					r.data = nullptr;
					r.size = 0;
				}

				r.size = c.bytes.size();
				if (r.size > 0)
				{
					r.data = static_cast<uint8_t*>(engine_->Allocator().Allocate(r.size, 16));
					std::memcpy(r.data, c.bytes.data(), r.size);
				}
			}
			else if (c.type == ResourceType::Text)
			{
				if (r.typedObject)
				{
					delete static_cast<TextResource*>(r.typedObject);
					r.typedObject = nullptr;
				}

				r.typedObject = c.typedObject;
				c.typedObject = nullptr;
			}

			r.state.store(ResourceState::Ready, std::memory_order_release);
			NOC_LOG_INFO("Res", "READY: %s", r.vpathNormalized.c_str());

		}
	}


	bool ResourceManager::IsReady(ResourceHandle h) const
	{
		uint32_t idx = 0;
		if (!ValidateHandle_(h, &idx))
			return false;

		auto* st = static_cast<InternalState*>(state_);
		return st->records[idx]->state.load(std::memory_order_acquire) == ResourceState::Ready;
	}

	bool ResourceManager::HasFailed(ResourceHandle h) const
	{
		uint32_t idx = 0;
		if (!ValidateHandle_(h, &idx))
			return true;

		auto* st = static_cast<InternalState*>(state_);
		return st->records[idx]->state.load(std::memory_order_acquire) == ResourceState::Failed;
	}

	const uint8_t* ResourceManager::GetBytes(ResourceHandle h) const
	{
		uint32_t idx = 0;
		if (!ValidateHandle_(h, &idx))
			return nullptr;

		auto* st = static_cast<InternalState*>(state_);
		const Record& r = *st->records[idx];
		if (r.state.load(std::memory_order_acquire) != ResourceState::Ready)
			return nullptr;

		return r.data;
	}

	size_t ResourceManager::GetSize(ResourceHandle h) const
	{
		uint32_t idx = 0;
		if (!ValidateHandle_(h, &idx))
			return 0;

		auto* st = static_cast<InternalState*>(state_);
		const Record& r = *st->records[idx];
		if (r.state.load(std::memory_order_acquire) != ResourceState::Ready)
			return 0;

		return r.size;
	}

	const char* ResourceManager::GetError(ResourceHandle h) const
	{
		uint32_t idx = 0;
		if (!ValidateHandle_(h, &idx))
			return nullptr;

		auto* st = static_cast<InternalState*>(state_);

		// The error string is a std::string and is mutated during Update() on failure,
		// so we must guard access with the same mutex that protects record access.
		std::lock_guard<std::mutex> lock(st->mtx);

		const Record& r = *st->records[idx];
		if (r.state.load(std::memory_order_acquire) != ResourceState::Failed)
			return nullptr;

		if (r.error.empty())
			return nullptr;

		return r.error.c_str();
	}



	bool ResourceManager::WaitUntilReady(ResourceHandle h, uint32_t timeoutMs)
	{
		const auto start = std::chrono::steady_clock::now();
		while (true)
		{
			Update();

			if (IsReady(h))
				return true;
			if (HasFailed(h))
				return false;

			if (timeoutMs != 0)
			{
				const auto now = std::chrono::steady_clock::now();
				const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
				if ((uint32_t)elapsed >= timeoutMs)
					return false;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	ResourceHandleT<TextResource> ResourceManager::RequestText(const char* vpath)
	{
		if (!running_ || !state_)
			return {};

		auto* st = static_cast<InternalState*>(state_);

		const std::string norm = NormalizeVPath_(std::string_view{ vpath ? vpath : "" });
		if (norm.empty())
		{
			NOC_LOG_ERROR("Res", "Invalid vpath: %s", vpath ? vpath : "(null)");
			return {};
		}

		const ResourceID id = MakeResourceID(norm);
		const uint64_t key = MakeCacheKey_(id.value, ResourceType::Text);

		// Look up existing
		{
			std::lock_guard<std::mutex> lock(st->mtx);
			auto it = st->idToIndex.find(key);
			if (it != st->idToIndex.end())
			{
				const uint32_t idx = it->second;
				Record* r = st->records[idx].get();
				return ResourceHandleT<TextResource>(ResourceHandle{ idx, r->generation });
			}
		}

		// Ensure loader exists (Phase 5 contract)
		if (!loaders_.FindLoader(ResourceType::Text))
		{
			NOC_LOG_ERROR("Res", "RequestText: no loader registered for ResourceType::Text");
			return {};
		}

		// Create record
		auto rec = std::make_unique<Record>();
		rec->id = id;
		rec->vpathNormalized = norm;
		rec->type = ResourceType::Text;
		rec->typedObject = nullptr;
		rec->state.store(ResourceState::Requested, std::memory_order_release);

		uint32_t index = 0;
		{
			std::lock_guard<std::mutex> lock(st->mtx);
			index = (uint32_t)st->records.size();
			st->records.push_back(std::move(rec));
			st->idToIndex.emplace(key, index);
		}

		// Enqueue load
		Record* r = st->records[index].get();
		r->state.store(ResourceState::Loading, std::memory_order_release);

		{
			std::lock_guard<std::mutex> lock(st->mtx);
			st->pending.push(PendingReq{ index });
		}
		st->cv.notify_one();

		return ResourceHandleT<TextResource>(ResourceHandle{ index, r->generation });
	}

	const TextResource* ResourceManager::GetText(ResourceHandleT<TextResource> h) const
	{
		if (!running_ || !state_)
			return nullptr;

		const ResourceHandle uh = h.Untyped();

		uint32_t idx = 0;
		if (!ValidateHandle_(uh, &idx))
			return nullptr;

		auto* st = static_cast<InternalState*>(state_);
		const Record& r = *st->records[idx];

		if (r.type != ResourceType::Text)
			return nullptr;

		if (r.state.load(std::memory_order_acquire) != ResourceState::Ready)
			return nullptr;

		return static_cast<const TextResource*>(r.typedObject);
	}


	void ResourceManager::LoaderThreadMain_()
	{
		NOC_LOG_INFO("Res", "Loader thread running");

		auto* st = static_cast<InternalState*>(state_);

		while (running_)
		{
			PendingReq req{};
			{
				std::unique_lock<std::mutex> lock(st->mtx);
				st->cv.wait(lock, [&]() { return !running_ || !st->pending.empty(); });

				if (!running_)
					break;

				req = st->pending.front();
				st->pending.pop();
			}

			CompletedReq done{};
			done.index = req.index;

			// Load via Phase 3 VFS API (OpenRead returns FileHandle)
			Record* r = nullptr;
			{
				std::lock_guard<std::mutex> lock(st->mtx);
				if (req.index < st->records.size())
					r = st->records[req.index].get();
			}

			if (!r)
			{
				done.ok = false;
				done.error = "Invalid record index";
			}
			else
			{
				FileHandle fh = vfs_->OpenRead(r->vpathNormalized);
				if (!fh.valid)
				{
					done.ok = false;
					done.error = "OpenRead failed";
				}
				else
				{
					const uint64_t sz64 = vfs_->Size(fh);
					const size_t sz = (sz64 > SIZE_MAX) ? SIZE_MAX : (size_t)sz64;

					done.bytes.resize(sz);

					if (sz > 0)
					{
						const size_t read = vfs_->Read(fh, done.bytes.data(), sz);
						if (read != sz)
						{
							done.ok = false;
							done.error = "Short read";
							done.bytes.clear();
						}
						else
						{
							done.ok = true;
							done.type = r->type;

							if (done.ok && r->type != ResourceType::Binary)
							{
								IResourceLoader* loader = loaders_.FindLoader(r->type);
								if (!loader)
								{
									done.ok = false;
									done.error = "No loader registered for requested type";
									done.bytes.clear();
								}
								else
								{
									const ResourceLoadResult rr = loader->Decode(done.bytes.data(), done.bytes.size());
									if (!rr.ok)
									{
										done.ok = false;
										done.error = rr.error;
										done.bytes.clear();
									}
									else
									{
										done.typedObject = rr.object;
										done.bytes.clear(); // no longer needed after decoding
									}
								}
							}

						}
					}
					else
					{
						done.ok = true;
					}

					vfs_->Close(fh);
				}
			}

			{
				std::lock_guard<std::mutex> lock(st->mtx);
				st->completed.push(std::move(done));
			}
		}

		NOC_LOG_INFO("Res", "Loader thread exiting");
	}

} // namespace noc
````

### NocturneHost (Phase 5 test)

Minimum acceptance behavior:

* request `"hello.txt"` as Text
* wait until ready
* log size + preview

(Integrate into wherever you already tested `RequestBinary` in Phase 4.) 

Pseudo-usage:

```cpp
auto h = engine.Resources().RequestText("hello.txt");
...
engine.Resources().Update(); // already called by Engine::Tick()
if (engine.Resources().IsReady(h.Untyped())) {
    const auto* txt = engine.Resources().GetText(h);
    NOC_LOG_INFO("Text bytes=%zu preview='%.64s'", txt->Size(), std::string(txt->View().substr(0,64)).c_str());
}
```

---

## Next chat handoff (ONLY what to say/bring next)

“Phase 5 is implemented. Here are the new files and the ResourceManager diffs. I can request TextResource and it becomes READY. Now start Phase 6 — Job System & Async Infrastructure.”