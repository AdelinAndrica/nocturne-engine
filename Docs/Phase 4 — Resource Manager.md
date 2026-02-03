# Phase 4 — Resource Manager (Foundation) + Objective

> **Status:** IMPLEMENTED & VERIFIED ✅  
> **Scope:** Virtual File System (VFS), mount points, virtual paths, file I/O abstraction  
> **Depends on:** Phase 1 — Core Systems, Phase 2 — Window & Main Loop  
>
> **Primary source of truth:** Jason Gregory, *Game Engine Architecture (3rd Edition)*

**Objective:** Add an engine-owned **Resource Manager** that turns **virtual paths** into **stable resource identities**, loads data through the **VFS**, and **caches** results so the same resource isn’t loaded twice.

This is the logical next step after Phase 3’s VFS: Gregory’s model is “files are not resources” — resources must have stable identity independent of where/how bytes are stored. 

---

## Key concepts from *Game Engine Architecture* (book-grounded)

* **Resource identity is decoupled from physical storage** (virtual paths, mounts, packaging/portability). 
* A **Resource Manager** provides a central place to **request**, **cache**, and **share** resources so multiple systems don’t reload the same thing independently. 
* A practical resource system needs a notion of **handles/IDs** and a **lifecycle** (requested → loading → ready/failed), enabling streaming/async later. 
* The VFS exists so higher-level systems never need to know whether data comes from **loose files vs archives**. 

---

## What we implement now (tight scope)

We implement **one resource type**: a raw **binary blob** loaded from a VFS virtual path.

Capabilities:

* `RequestBinary(vpath)` returns a **ResourceHandle**
* background thread loads bytes via `VirtualFileSystem`
* main thread calls `Resources().Update()` to finalize completed loads
* you can check `IsReady(handle)` and access `GetBytes(handle)`

**Design choice (not directly from the book):**

* Resource IDs are **64-bit FNV-1a hashes** of normalized vpaths.
* Handles are **(index + generation)** to detect stale use.
* One dedicated loader thread (job system comes later).

---

## Implementation steps

1. Create the new `Engine/Resources/` files for IDs, handles, and the manager.
2. Update `Engine` to own a `ResourceManager`:

   * Init it **after VFS mounts**
   * Shutdown it **before** shutting down core subsystems (so memory/logging are still valid)
   * Call `resources_.Update()` during `Engine::Tick()`
3. Update `NocturneHost` to request `hello.txt` via the Resource Manager and log the results (host-side test, not in engine init).

---

## Verification checklist (Phase 4 done when…)

* [x] Running `NocturneHost` logs that the Resource Manager started.
* [x] `hello.txt` is requested via `engine.Resources().RequestBinary("hello.txt")`.
* [x] It becomes **Ready** and logs file size and a short preview.
* [x] Closing the window still shuts down cleanly with no leaks/crashes.
* [x] Requesting the same vpath twice returns the **same cached resource** (no duplicate load).

## Verification (Phase 4)

### Test Setup

- `NocturneHost` sets the content root explicitly via a build macro:
  - `engine.SetContentRoot(NOC_CONTENT_ROOT)`
- A test file exists at:
  - `${NOC_CONTENT_ROOT}/hello.txt`

### Expected Behavior

- The engine mounts the loose directory content root.
- The Resource Manager starts a loader thread.
- `hello.txt` is requested via `RequestBinary("hello.txt")`.
- The resource transitions to `READY`.
- Requesting the same virtual path twice returns the same cached handle.
- Shutdown is clean with no outstanding allocations.

### Verified Log Output

```txt
[INFO][VFS] Mounted loose directory: D:/Projects/Nocturne/Data (priority=0)
[INFO][Res] ResourceManager initialized (loader thread started)
[INFO][Res] Loader thread running
[INFO][Res] READY: hello.txt (11 bytes)
[INFO][Host] hello.txt loaded (11 bytes)
[INFO][Host] Preview (first 11 bytes): afafafafasf
[INFO][Host] Handle1 = (0, 1)
[INFO][Host] Handle2 = (0, 1)
[INFO][Res] Loader thread exiting
[INFO][Res] ResourceManager shutdown complete
[INFO][Core] Memory stats: total=8388619 outstanding=0 allocs=2
```

---

## Common pitfalls

* Calling `ResourceManager::Shutdown()` **after** Memory shutdown (you’ll crash/free invalid allocator).
* Accessing resource bytes without checking `IsReady()`.
* Forgetting to pump `Resources().Update()` each frame (loads will complete in the worker, but never get finalized).
* Treating OS paths as identities instead of virtual paths (breaks mounting/archives later).

---

## Next chat handoff (exactly what to say/bring)

> “Phase 4 is complete. I can request `hello.txt` through the Resource Manager and it loads asynchronously via the VFS. Start Phase 5: typed resources + loaders (text, JSON, image/texture stub, mesh stub).”

---

# Implementations

Below are **every file implemented/updated in Phase 4**, each with its **path** and **full implementation**.

---

## `Engine/Resources/ResourceID.h`

```cpp
#pragma once
#include <cstdint>
#include <string_view>

namespace noc
{
    // Design choice (not directly from the book):
    // A 64-bit hash of the normalized virtual path.
    struct ResourceID
    {
        uint64_t value = 0;

        constexpr bool IsValid() const { return value != 0; }

        friend constexpr bool operator==(ResourceID a, ResourceID b) { return a.value == b.value; }
        friend constexpr bool operator!=(ResourceID a, ResourceID b) { return a.value != b.value; }
    };

    // Design choice (not directly from the book): 64-bit FNV-1a.
    inline constexpr uint64_t Fnv1a64(const char* data, size_t len)
    {
        uint64_t h = 1469598103934665603ull;
        for (size_t i = 0; i < len; ++i)
        {
            h ^= static_cast<uint8_t>(data[i]);
            h *= 1099511628211ull;
        }
        return h;
    }

    inline ResourceID MakeResourceID(std::string_view normalizedVPath)
    {
        ResourceID id{};
        id.value = Fnv1a64(normalizedVPath.data(), normalizedVPath.size());

        // Avoid 0 being a valid ID (tiny guard; extremely unlikely anyway).
        if (id.value == 0) id.value = 1;
        return id;
    }

} // namespace noc
```

---

## `Engine/Resources/ResourceHandle.h`

```cpp
#pragma once
#include <cstdint>

namespace noc
{
    // Design choice (not directly from the book):
    // index + generation to detect stale handles.
    struct ResourceHandle
    {
        uint32_t index = 0xFFFFFFFFu;
        uint32_t generation = 0;

        constexpr bool IsValid() const { return index != 0xFFFFFFFFu; }

        friend constexpr bool operator==(ResourceHandle a, ResourceHandle b)
        {
            return a.index == b.index && a.generation == b.generation;
        }

        friend constexpr bool operator!=(ResourceHandle a, ResourceHandle b)
        {
            return !(a == b);
        }
    };

} // namespace noc
```

---

## `Engine/Resources/ResourceManager.h`

```cpp
#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "Resources/ResourceHandle.h"

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

        // Optional helper for host-side testing.
        bool WaitUntilReady(ResourceHandle h, uint32_t timeoutMs);

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
    };

} // namespace noc
```

---

## `Engine/Resources/ResourceManager.cpp`

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

namespace noc
{
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
        std::vector<uint8_t> bytes;
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
            if (rp && rp->data)
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

        // Look up existing
        {
            std::lock_guard<std::mutex> lock(st->mtx);
            auto it = st->idToIndex.find(id.value);
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
        rec->state.store(ResourceState::Requested, std::memory_order_release);

        uint32_t index = 0;
        {
            std::lock_guard<std::mutex> lock(st->mtx);
            index = (uint32_t)st->records.size();
            st->records.push_back(std::move(rec));
            st->idToIndex.emplace(id.value, index);
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

            Record& r = *st->records[c.index];

            if (!c.ok)
            {
                r.error = std::move(c.error);
                r.state.store(ResourceState::Failed, std::memory_order_release);
                NOC_LOG_ERROR("Res", "FAILED: %s (%s)", r.vpathNormalized.c_str(), r.error.c_str());
                continue;
            }

            // Replace any existing blob
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

            r.state.store(ResourceState::Ready, std::memory_order_release);
            NOC_LOG_INFO("Res", "READY: %s (%zu bytes)", r.vpathNormalized.c_str(), r.size);
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
```

---

## `Engine/Runtime/Engine.h` (updated)

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

        bool InitMemory();
        void KillMemory();

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
        ResourceManager resources_;

        EngineConfig cfg_{};
        bool initialized_ = false;
    };

} // namespace noc
```

---

## `Engine/Runtime/Engine.cpp` (updated)

```cpp
#include "Engine.h"

#include <cstring>

#include "Core/Log.h"
#include "Core/Assert.h"
#include "Core/Clock.h"
#include "Platform/Win32/WinWindow.h"
#include "Runtime/MainLoop.h"

namespace noc {

    IAllocator& Engine::Allocator() { return *alloc_; }
    LinearArena& Engine::FrameArena() { return frameArena_; }

    EngineConfig& Engine::ConfigMutable()
    {
        NOC_ASSERT_MSG(IsConfigMutable(), "Engine config is immutable after Engine::Init()");
        return cfg_;
    }

    static bool SetCString_(const char*& dst, const char* src)
    {
        dst = src;
        return true;
    }

    bool Engine::SetContentRoot(const char* path)
    {
        if (!IsConfigMutable())
        {
            NOC_LOG_WARN("Core", "SetContentRoot ignored (called after Init)");
            return false;
        }
        return SetCString_(cfg_.contentRoot, path);
    }

    bool Engine::SetOverrideRoot(const char* path)
    {
        if (!IsConfigMutable())
        {
            NOC_LOG_WARN("Core", "SetOverrideRoot ignored (called after Init)");
            return false;
        }
        return SetCString_(cfg_.overrideRoot, path);
    }

    bool Engine::SetArchivePath(const char* path)
    {
        if (!IsConfigMutable())
        {
            NOC_LOG_WARN("Core", "SetArchivePath ignored (called after Init)");
            return false;
        }
        return SetCString_(cfg_.archivePath, path);
    }

    bool Engine::InitMemory()
    {
        constexpr std::size_t kFrameArenaBytes = 8 * 1024 * 1024; // Design choice
        frameArenaMem_ = Allocator().Allocate(kFrameArenaBytes, 64);
        frameArena_.Init(frameArenaMem_, kFrameArenaBytes);

        NOC_LOG_INFO("Core", "Memory system initialized (frame arena=%zu bytes)", kFrameArenaBytes);
        return true;
    }

    void Engine::KillMemory()
    {
        if (frameArenaMem_)
        {
            Allocator().Deallocate(frameArenaMem_);
            frameArenaMem_ = nullptr;
        }

#if NOC_ENABLE_ASSERTS
        NOC_LOG_INFO("Core", "Memory stats: total=%zu outstanding=%zu allocs=%zu",
            debugAlloc_.TotalAllocatedBytes(),
            debugAlloc_.OutstandingBytes(),
            debugAlloc_.AllocationCount());
#endif
    }

    static bool StartupLog(void*)
    {
        noc::GetLogger().Init();
        NOC_LOG_INFO("Core", "Logger initialized");
        return true;
    }

    static void ShutdownLog(void*)
    {
        NOC_LOG_INFO("Core", "Logger shutting down");
        noc::GetLogger().Shutdown();
    }

    static bool StartupTime(void*)
    {
        noc::GetTime().Init();
        NOC_LOG_INFO("Core", "Time system initialized");
        return true;
    }

    static void ShutdownTime(void*)
    {
        NOC_LOG_INFO("Core", "Time system shutting down");
    }

    static bool StartupAssert(void*)
    {
        NOC_LOG_INFO("Core", "Assert system initialized");
        return true;
    }

    static void ShutdownAssert(void*)
    {
        NOC_LOG_INFO("Core", "Assert system shutting down");
    }

    static bool StartupMemory(void* ctx)
    {
        auto* e = static_cast<noc::Engine*>(ctx);
        return e->InitMemory();
    }

    static void ShutdownMemory(void* ctx)
    {
        NOC_LOG_INFO("Core", "Memory system shutting down");
        auto* e = static_cast<noc::Engine*>(ctx);
        e->KillMemory();
    }

    static bool StartupWindow(void*)
    {
        NOC_LOG_INFO("Win32", "Window subsystem ready");
        return true;
    }

    static void ShutdownWindow(void*)
    {
        NOC_LOG_INFO("Win32", "Window subsystem shutdown");
    }

    bool Engine::Init()
    {
        // Freeze config from this point on.
        initialized_ = true;

        std::span<const char* const> depsLog{};
        static const char* kDepsNeedLog[] = { "Log" };
        std::span<const char* const> depsNeedLog{ kDepsNeedLog, 1 };

        registry_.Register(SubsystemDesc{ "Log",    depsLog,     &StartupLog,    &ShutdownLog });
        registry_.Register(SubsystemDesc{ "Time",   depsNeedLog, &StartupTime,   &ShutdownTime });
        registry_.Register(SubsystemDesc{ "Memory", depsNeedLog, &StartupMemory, &ShutdownMemory });
        registry_.Register(SubsystemDesc{ "Assert", depsNeedLog, &StartupAssert, &ShutdownAssert });
        registry_.Register(SubsystemDesc{ "Window", depsNeedLog, &StartupWindow, &ShutdownWindow });

        if (!registry_.StartupAll(this))
            return false;

        // Phase 3 policy (locked): later mounts override earlier mounts.
        if (cfg_.archivePath && cfg_.archivePath[0] != 0)
        {
            if (!vfs_.MountArchive(cfg_.archivePath))
                NOC_LOG_WARN("VFS", "Failed to mount archivePath: %s", cfg_.archivePath);
        }

        if (cfg_.contentRoot && cfg_.contentRoot[0] != 0)
        {
            if (!vfs_.MountLooseDirectory(cfg_.contentRoot))
                NOC_LOG_WARN("VFS", "Failed to mount contentRoot: %s", cfg_.contentRoot);
        }

        if (cfg_.overrideRoot && cfg_.overrideRoot[0] != 0)
        {
            if (!vfs_.MountLooseDirectory(cfg_.overrideRoot))
                NOC_LOG_WARN("VFS", "Failed to mount overrideRoot: %s", cfg_.overrideRoot);
        }

        // Phase 4: ResourceManager init (must be after VFS mounts).
        if (!resources_.Init(*this, vfs_))
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

        MainLoop loop;
        loop.Run(*this, window);

        window.Destroy();
        return 0;
    }

    void Engine::BeginFrame()
    {
        GetTime().BeginFrame();
        FrameArena().Reset();
    }

    void Engine::Tick()
    {
        // Phase 4: pump completed resource loads.
        resources_.Update();
    }

    void Engine::EndFrame()
    {
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
        // Phase 4: shutdown resource manager BEFORE core teardown.
        resources_.Shutdown();

        registry_.ShutdownAll(this);
    }

} // namespace noc
```

---

## `Apps/NocturneHost/main.cpp` (updated)

```cpp
#include "Runtime/Engine.h"
#include "Core/Log.h"

#include <algorithm>
#include <cctype>
#include <string>

#ifndef NOC_CONTENT_ROOT
#define NOC_CONTENT_ROOT "Data"
#endif

static void LogPreview(const uint8_t* bytes, size_t size)
{
    if (!bytes || size == 0)
    {
        NOC_LOG_INFO("Host", "File empty");
        return;
    }

    const size_t n = std::min<size_t>(size, 64);
    std::string s;
    s.reserve(n);

    for (size_t i = 0; i < n; ++i)
    {
        const char c = static_cast<char>(bytes[i]);
        s.push_back((std::isprint((unsigned char)c) ? c : '.'));
    }

    NOC_LOG_INFO("Host", "Preview (first %zu bytes): %s", n, s.c_str());
}

int main()
{
    noc::Engine engine;

    // Phase 3 config defaults to contentRoot="Data".
    // If your repo uses a different layout, you can override here before Init():
     engine.SetContentRoot(NOC_CONTENT_ROOT);
    // engine.SetOverrideRoot("DataOverrides");
    // engine.SetArchivePath("Data.pak");

    if (!engine.Init())
        return -1;

    // Phase 4 smoke test (host-side): load hello.txt via ResourceManager.
    auto h = engine.Resources().RequestBinary("hello.txt");
    if (!h.IsValid())
    {
        NOC_LOG_ERROR("Host", "Failed to request hello.txt");
    }
    else
    {
        const bool ok = engine.Resources().WaitUntilReady(h, /*timeoutMs*/ 2000);
        if (!ok)
        {
            NOC_LOG_ERROR("Host", "hello.txt did not become ready (failed or timed out)");
        }
        else
        {
            const uint8_t* bytes = engine.Resources().GetBytes(h);
            const size_t size = engine.Resources().GetSize(h);
            NOC_LOG_INFO("Host", "hello.txt loaded (%zu bytes)", size);
            LogPreview(bytes, size);
        }
    }

    auto h1 = engine.Resources().RequestBinary("hello.txt");
    auto h2 = engine.Resources().RequestBinary("hello.txt");

    NOC_LOG_INFO("Host", "Handle1 = (%u, %u)", h1.index, h1.generation);
    NOC_LOG_INFO("Host", "Handle2 = (%u, %u)", h2.index, h2.generation);


    const int rc = engine.Run();

    engine.Shutdown();
    return rc;
}
```

---
