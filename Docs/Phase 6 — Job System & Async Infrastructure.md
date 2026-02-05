# Phase 6 — Job System & Async Infrastructure

> **Status:** READY FOR IMPLEMENTATION ⏳  
> **Scope:** Engine-owned Job System (thread pool + job graph primitives) + refactor ResourceManager to use jobs instead of a dedicated loader thread  
> **Depends on:** Phase 1 (Core), Phase 2 (Window/Main Loop), Phase 3 (VFS), Phase 4 (Resource Manager foundation), Phase 5 (Typed Resources & Loader Registry) :contentReference[oaicite:0]{index=0} :contentReference[oaicite:1]{index=1}  
>
> **Primary source of truth:** Jason Gregory, *Game Engine Architecture (3rd Ed.)*  
> **Architecture alignment:** “JOBS → JobSystem” + Resource pipeline in Full Architecture Diagram :contentReference[oaicite:2]{index=2}

This document is the **authoritative design and implementation reference** for Phase 6 of Nocturne Engine.

Phase 6 introduces the engine’s **general-purpose async execution substrate**: a **Job System** that replaces ad-hoc background threads (starting with the ResourceManager loader thread) and becomes the foundation for streaming, async gameplay tasks, background cooking, and later (eventually) render prep and simulation work.

We will **keep the existing completion/finalize model**:
- **Worker threads do the heavy work** (I/O, decode/parse, CPU processing).
- **Main thread finalizes** results deterministically in `ResourceManager::Update()` (and later other systems’ “finalize” stages).

---

## Phase name + objective

**Objective:** Implement a minimal but production-leaning **Job System** and refactor ResourceManager to schedule work on it, replacing the dedicated loader thread while preserving:

- stable resource identity + caching
- explicit states (Requested → Loading → Ready/Failed)
- **main-thread finalization** and predictable frame boundaries
- clean shutdown and leak-free behavior

---

## Key concepts from the books (book-grounded)

- **Engine subsystems own execution**: the engine controls when and how work happens (main loop is the orchestrator). :contentReference[oaicite:3]{index=3}  
- **Asynchronous work needs explicit handoff**: background production + deterministic consumption/finalization is a common engine pattern for correctness and debuggability (resource loading is the canonical example). :contentReference[oaicite:4]{index=4} :contentReference[oaicite:5]{index=5}  
- **Avoid bespoke threads per system**: consolidate async into shared infrastructure (job system / task system) to reduce complexity and contention (engine architecture principle; implementation details are our design choices).

---

## What we implement now (tight scope)

### A) Job System (Engine/Jobs)
1. **JobSystem** with a fixed worker thread pool.
2. A way to **enqueue** work items (`JobHandle` returned).
3. A way to **wait** on a job (for tests and controlled shutdown).
4. A per-frame **pump point** (optional now, but we’ll include `BeginFrame()` to support future budgeting).

### B) ResourceManager refactor
1. Remove the dedicated loader thread and its condition-variable loop.
2. On request, schedule the resource load pipeline using jobs:
   - **Job 1:** VFS read bytes (and/or decode/parse, depending on your current typed loader rules).
   - **Completion record:** push to `completed` queue for main thread.
3. `ResourceManager::Update()` remains the sole finalizer that flips READY/FAILED and publishes resource payloads.

**Non-goals (explicitly out of scope for Phase 6):**
- work stealing
- fibers / job continuation stacks
- per-job priorities beyond a small enum stub
- hard budgeting / frame time slicing
- CPU affinity / NUMA tuning

**Design choice (not directly from the book):**
- Start with a simple MPMC queue + worker threads + `JobHandle` built on an atomic counter (or a small shared state). We will evolve it later if needed.

---

## Execution & ownership model (locked for Phase 6)

### Threads
- **Main thread**:
  - owns subsystem `Init/Shutdown`
  - calls `Engine::Tick()` and `ResourceManager::Update()`
  - does **finalize/publish** only
- **Job worker threads**:
  - perform background tasks (I/O, parse, CPU transforms)
  - never touch Win32 windowing APIs
  - never call into renderer (future rule)
  - never mutate global engine state except via thread-safe queues

### Resource pipeline (job-backed)
1. Request creates/finds a record in cache.
2. If new, transition to `Loading` and enqueue a job that:
   - reads bytes from VFS
   - runs loader decode (if Phase 5 allows decode off-thread)
   - writes a **Completion** struct into a thread-safe `completed` queue
3. Main thread `Update()`:
   - drains `completed`
   - validates handle + generation
   - commits payload to the resource record
   - marks `Ready` or `Failed`

This preserves the “completion/finalize model” you already have from Phase 4/5. :contentReference[oaicite:6]{index=6} :contentReference[oaicite:7]{index=7}

---

## Implementation steps

### 1) Add Jobs module scaffolding
- Create: `Engine/Jobs/`
  - `JobSystem.h/.cpp`
  - `JobHandle.h`
  - `JobQueue.h/.cpp` (or internal-only inside JobSystem)
  - `JobPriority.h` (stub enum)

### 2) Implement the minimal JobSystem
- `Init(workerCount)`
- `Shutdown()`
- `Enqueue(JobDesc) -> JobHandle`
- `Wait(JobHandle)` (used only by tests/shutdown paths)
- Worker loop pops jobs until shutdown flag set.

### 3) Engine owns JobSystem
- Add to `Engine`:
  - `JobSystem jobs_;`
  - init order: after core systems; before resources
  - shutdown order: resources before jobs (so pending resource jobs can be drained or canceled cleanly)

### 4) Refactor ResourceManager to use jobs
- Remove:
  - loader thread creation
  - loader thread loop
  - CV-based wakeup
- Replace:
  - on resource request, enqueue a job that performs `LoadOne_()` work
  - keep the **same completion queue**
- Ensure `ResourceManager::Update()` still drains completions and finalizes.

### 5) Verification in NocturneHost
- Existing tests must still pass:
  - load hello.txt → READY
  - cache returns same handle
  - missing file fails cleanly
- Add one additional stress test:
  - request N text files quickly (even duplicates) and confirm no deadlocks and all finalize on main thread.

---

## Verification checklist (Phase 6 done when…)

- [ ] Engine starts and shuts down cleanly with JobSystem enabled.
- [ ] ResourceManager no longer logs “loader thread started/running/exiting”.
- [ ] `hello.txt` loads successfully via job-backed work and finalizes in `Resources().Update()`.
- [ ] Missing file still transitions to FAILED with the same error surfaced to host code.
- [ ] No outstanding allocations at shutdown (same memory stats expectation as Phase 4/5).
- [ ] Rapid repeated requests don’t duplicate loads and don’t deadlock.

---

## Common pitfalls

- **Finalizing on worker threads** (breaks deterministic lifetime rules and will bite you later with GPU/OS objects).
- **Capturing raw pointers into jobs** that outlive the owning subsystem during shutdown.
- **Holding ResourceManager mutex while doing I/O** (kills concurrency and can deadlock with finalize).
- **Shutdown ordering**: if jobs stop before resources drain/cancel, completions may be lost or records left stuck in Loading.

---

## Next chat handoff (ONLY what you should say/bring next)

> “Phase 6 JobSystem is implemented and Engine owns it. Here are the logs and the changed files. ResourceManager no longer has a loader thread and hello.txt still loads. Let’s review correctness, shutdown safety, and whether decode happens on workers or main thread.”

## 6) Implementations

## `Engine/Core/Jobs/JobSystem.h`

```cpp
#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace noc {

class JobSystem {
public:
    using JobFn = std::function<void()>;

    JobSystem() = default;
    ~JobSystem() { Shutdown(); }

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    bool Init(uint32_t workerCount);
    void Shutdown();

    // Fire-and-forget jobs. (Phase 6 scope)
    void Enqueue(JobFn fn);

    // For orderly shutdown / subsystems that need to wait.
    void WaitIdle();

    uint32_t WorkerCount() const { return workerCount_; }
    bool IsRunning() const { return running_.load(std::memory_order_acquire); }

private:
    void WorkerMain_(uint32_t workerIndex);

private:
    std::atomic<bool> running_{ false };

    std::mutex mtx_;
    std::condition_variable cvWork_;
    std::condition_variable cvIdle_;
    std::queue<JobFn> queue_;

    std::atomic<uint32_t> inflight_{ 0 };

    std::vector<std::thread> workers_;
    uint32_t workerCount_ = 0;
};

} // namespace noc
```

## `Engine/Core/Jobs/JobSystem.cpp`

```cpp
#include "JobSystem.h"

#include "Core/Log.h"

#include <algorithm>

namespace noc {

bool JobSystem::Init(uint32_t workerCount)
{
    if (running_.load(std::memory_order_acquire))
        return true;

    // Design choice (not directly from the book): clamp worker count.
    if (workerCount == 0)
        workerCount = std::max(1u, std::thread::hardware_concurrency());

    workerCount_ = workerCount;

    running_.store(true, std::memory_order_release);

    workers_.reserve(workerCount_);
    for (uint32_t i = 0; i < workerCount_; ++i)
        workers_.emplace_back([this, i]() { WorkerMain_(i); });

    NOC_LOG_INFO("Jobs", "JobSystem initialized (%u workers)", workerCount_);
    return true;
}

void JobSystem::Shutdown()
{
    if (!running_.load(std::memory_order_acquire))
        return;

    WaitIdle();

    running_.store(false, std::memory_order_release);
    cvWork_.notify_all();

    for (auto& t : workers_)
    {
        if (t.joinable())
            t.join();
    }
    workers_.clear();

    // Drain anything that might remain (should be empty after WaitIdle).
    {
        std::lock_guard<std::mutex> lock(mtx_);
        while (!queue_.empty())
            queue_.pop();
    }

    NOC_LOG_INFO("Jobs", "JobSystem shutdown complete");
}

void JobSystem::Enqueue(JobFn fn)
{
    if (!fn)
        return;

    if (!running_.load(std::memory_order_acquire))
    {
        // If jobs are not running, execute inline (safe fallback).
        fn();
        return;
    }

    inflight_.fetch_add(1, std::memory_order_acq_rel);

    {
        std::lock_guard<std::mutex> lock(mtx_);
        queue_.push(std::move(fn));
    }

    cvWork_.notify_one();
}

void JobSystem::WaitIdle()
{
    // Wait until there are no inflight jobs and queue is empty.
    std::unique_lock<std::mutex> lock(mtx_);
    cvIdle_.wait(lock, [&]() {
        return queue_.empty() && inflight_.load(std::memory_order_acquire) == 0;
    });
}

void JobSystem::WorkerMain_(uint32_t workerIndex)
{
    (void)workerIndex;

    for (;;)
    {
        JobFn job;

        {
            std::unique_lock<std::mutex> lock(mtx_);
            cvWork_.wait(lock, [&]() {
                return !running_.load(std::memory_order_acquire) || !queue_.empty();
            });

            if (!running_.load(std::memory_order_acquire) && queue_.empty())
                break;

            if (!queue_.empty())
            {
                job = std::move(queue_.front());
                queue_.pop();
            }
        }

        if (job)
            job();

        const uint32_t left = inflight_.fetch_sub(1, std::memory_order_acq_rel) - 1;

        // Notify anyone waiting for idle when we *might* have reached it.
        if (left == 0)
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (queue_.empty())
                cvIdle_.notify_all();
        }
    }
}

} // namespace noc
```

## `Engine/Runtime/Engine.h`

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

    JobSystem& Jobs() { return jobs_; }
    const JobSystem& Jobs() const { return jobs_; }

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
    JobSystem jobs_;
    ResourceManager resources_;

    EngineConfig cfg_{};
    bool initialized_ = false;
};

} // namespace noc
```

## `Engine/Runtime/Engine.cpp`

```cpp
#include "Engine.h"

#include <span>

#include "Core/Assert.h"
#include "Core/Log.h"
#include "Core/Time.h"

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
    cfg_.contentRoot = path;
    return true;
}

bool Engine::SetOverrideRoot(const char* path)
{
    if (!IsConfigMutable())
    {
        NOC_LOG_ERROR("Runtime", "SetOverrideRoot() called after Init(); ignored.");
        return false;
    }
    cfg_.overrideRoot = path;
    return true;
}

bool Engine::SetArchivePath(const char* path)
{
    if (!IsConfigMutable())
    {
        NOC_LOG_ERROR("Runtime", "SetArchivePath() called after Init(); ignored.");
        return false;
    }
    cfg_.archivePath = path;
    return true;
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

static bool StartupWindow(void* ctx)
{
    auto* e = static_cast<Engine*>(ctx);
    (void)e;
    NOC_LOG_INFO("Win32", "Window subsystem ready");
    return true;
}

static void ShutdownWindow(void*)
{
    NOC_LOG_INFO("Win32", "Window subsystem shutdown");
}

static bool StartupJobs(void* ctx)
{
    auto* e = static_cast<noc::Engine*>(ctx);
    // Design choice: worker count = HW threads - 1 (leave room for main thread), clamped.
    const uint32_t hw = std::max(1u, std::thread::hardware_concurrency());
    const uint32_t workers = (hw > 1) ? (hw - 1) : 1;
    return e->Jobs().Init(workers);
}

static void ShutdownJobs(void* ctx)
{
    auto* e = static_cast<noc::Engine*>(ctx);
    e->Jobs().Shutdown();
}

bool Engine::Init()
{
    std::span<const char* const> depsLog{};

    static const char* kDepsNeedLog[] = { "Log" };
    std::span<const char* const> depsNeedLog{ kDepsNeedLog, 1 };

    static const char* kDepsJobs[] = { "Log", "Memory" };
    std::span<const char* const> depsJobs{ kDepsJobs, 2 };

    registry_.Register(SubsystemDesc{ "Log",    depsLog,     &StartupLog,    &ShutdownLog });
    registry_.Register(SubsystemDesc{ "Time",   depsNeedLog, &StartupTime,   &ShutdownTime });
    registry_.Register(SubsystemDesc{ "Memory", depsNeedLog, &StartupMemory, &ShutdownMemory });
    registry_.Register(SubsystemDesc{ "Assert", depsNeedLog, &StartupAssert, &ShutdownAssert });
    registry_.Register(SubsystemDesc{ "Window", depsNeedLog, &StartupWindow, &ShutdownWindow });
    registry_.Register(SubsystemDesc{ "Jobs",   depsJobs,    &StartupJobs,   &ShutdownJobs });

    if (!registry_.StartupAll(this))
        return false;

    // Phase 3 policy: later mounts override earlier mounts.
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

    // Phase 6: ResourceManager uses JobSystem (must be after jobs + VFS mounts).
    if (!resources_.Init(*this, vfs_))
        return false;

    initialized_ = true;
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
    // Resource manager must shutdown while jobs + memory + log still exist.
    resources_.Shutdown();

    registry_.ShutdownAll(this);
}

} // namespace noc
```

## `Engine/Resources/ResourceManager.h`

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

        // ---- Binary blob API ----
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

        // ---- Typed API (Phase 5) ----
        ResourceHandleT<TextResource> RequestText(const char* vpath);
        const TextResource* GetText(ResourceHandleT<TextResource> h) const;

        ResourceLoaderRegistry& Loaders() { return loaders_; }
        const ResourceLoaderRegistry& Loaders() const { return loaders_; }

    private:
        bool ValidateHandle_(ResourceHandle h, uint32_t* outIndex) const;
        void EnqueueLoadJob_(uint32_t index);
        void WaitAllJobs_();

    private:
        Engine* engine_ = nullptr;
        VirtualFileSystem* vfs_ = nullptr;

        // Internal opaque state (allocated in .cpp)
        void* state_ = nullptr;

        bool running_ = false;

        ResourceLoaderRegistry loaders_;
    };

} // namespace noc
```

## `Engine/Resources/ResourceManager.cpp`

```cpp
#include "ResourceManager.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "Core/Assert.h"
#include "Core/Log.h"
#include "Core/Jobs/JobSystem.h"
#include "Core/Memory/Allocator.h"
#include "Resources/ResourceID.h"
#include "Resources/VirtualFileSystem.h"
#include "Resources/VPath.h"
#include "Resources/FileHandle.h"
#include "Runtime/Engine.h"

#include "Engine/Resources/Typed/TextResourceLoader.h"

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

        // Binary blob (owned by ResourceManager allocator)
        uint8_t* data = nullptr;
        size_t size = 0;

        // Typed object (type-erased)
        ResourceType type = ResourceType::Unknown;
        void* typedObject = nullptr;

        std::string error;

        Record() = default;
        Record(const Record&) = delete;
        Record& operator=(const Record&) = delete;
        Record(Record&&) = delete;
        Record& operator=(Record&&) = delete;
    };

    struct CompletedReq
    {
        uint32_t index = 0;
        ResourceType type = ResourceType::Unknown;

        bool ok = false;
        std::vector<uint8_t> bytes; // only used for Binary (or intermediate before decode)
        void* typedObject = nullptr;

        std::string error;
    };

    struct InternalState
    {
        std::mutex mtx;

        // Completion queue is produced by worker jobs, consumed by main thread Update().
        std::deque<CompletedReq> completed;

        std::unordered_map<uint64_t, uint32_t> idToIndex;
        std::vector<std::unique_ptr<Record>> records;

        std::atomic<uint32_t> jobsInflight{ 0 };
        std::condition_variable cvJobsIdle;
    };

    static TextResourceLoader g_textLoader;

    static std::string NormalizeVPath_(std::string_view vpath)
    {
        char buf[512]{};
        if (!noc::vpath::NormalizeToRelative(buf, sizeof(buf), vpath))
            return {};
        return std::string(buf);
    }

    static uint64_t MakeCacheKey_(uint64_t idValue, ResourceType type)
    {
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

        // Register built-in loaders (Phase 5)
        loaders_.RegisterLoader(&g_textLoader);

        running_ = true;

        NOC_LOG_INFO("Res", "ResourceManager initialized (job-backed)");
        return true;
    }

    void ResourceManager::WaitAllJobs_()
    {
        auto* st = static_cast<InternalState*>(state_);
        if (!st)
            return;

        std::unique_lock<std::mutex> lock(st->mtx);
        st->cvJobsIdle.wait(lock, [&]() {
            return st->jobsInflight.load(std::memory_order_acquire) == 0;
        });
    }

    void ResourceManager::Shutdown()
    {
        if (!running_)
            return;

        running_ = false;

        auto* st = static_cast<InternalState*>(state_);
        if (!st)
            return;

        // Wait for outstanding load jobs to finish producing completions.
        WaitAllJobs_();

        // Drain completions once (safe: no producers remain).
        Update();

        // Free blobs/typed objects
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

    void ResourceManager::EnqueueLoadJob_(uint32_t index)
    {
        auto* st = static_cast<InternalState*>(state_);
        if (!st || !engine_ || !vfs_)
            return;

        st->jobsInflight.fetch_add(1, std::memory_order_acq_rel);

        engine_->Jobs().Enqueue([this, index]() {
            auto* stLocal = static_cast<InternalState*>(state_);
            if (!stLocal || !vfs_)
                return;

            CompletedReq done{};
            done.index = index;

            // Snapshot record pointer safely.
            Record* r = nullptr;
            {
                std::lock_guard<std::mutex> lock(stLocal->mtx);
                if (index < stLocal->records.size())
                    r = stLocal->records[index].get();
            }

            if (!running_ || !r)
            {
                done.ok = false;
                done.error = "Invalid record index or manager stopped";
            }
            else
            {
                done.type = r->type;

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

                            if (done.type != ResourceType::Binary)
                            {
                                IResourceLoader* loader = loaders_.FindLoader(done.type);
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
                                        done.bytes.clear(); // decoded object owns data now
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        // empty file is still OK
                        done.ok = true;

                        if (done.type != ResourceType::Binary)
                        {
                            IResourceLoader* loader = loaders_.FindLoader(done.type);
                            if (!loader)
                            {
                                done.ok = false;
                                done.error = "No loader registered for requested type";
                            }
                            else
                            {
                                const ResourceLoadResult rr = loader->Decode(nullptr, 0);
                                if (!rr.ok)
                                {
                                    done.ok = false;
                                    done.error = rr.error;
                                }
                                else
                                {
                                    done.typedObject = rr.object;
                                }
                            }
                        }
                    }

                    vfs_->Close(fh);
                }
            }

            {
                std::lock_guard<std::mutex> lock(stLocal->mtx);
                stLocal->completed.push_back(std::move(done));
            }

            const uint32_t left = stLocal->jobsInflight.fetch_sub(1, std::memory_order_acq_rel) - 1;
            if (left == 0)
            {
                std::lock_guard<std::mutex> lock(stLocal->mtx);
                stLocal->cvJobsIdle.notify_all();
            }
        });
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

        // Create new record
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

        // Enqueue job
        {
            std::lock_guard<std::mutex> lock(st->mtx);
            st->records[index]->state.store(ResourceState::Loading, std::memory_order_release);
        }
        EnqueueLoadJob_(index);

        return ResourceHandle{ index, st->records[index]->generation };
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

        // Enqueue job
        {
            std::lock_guard<std::mutex> lock(st->mtx);
            st->records[index]->state.store(ResourceState::Loading, std::memory_order_release);
        }
        EnqueueLoadJob_(index);

        Record* r = st->records[index].get();
        return ResourceHandleT<TextResource>(ResourceHandle{ index, r->generation });
    }

    void ResourceManager::Update()
    {
        if (!state_)
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
                st->completed.pop_front();
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

            // Success path:
            r.error.clear();

            if (r.type == ResourceType::Binary)
            {
                // Replace blob
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
            else
            {
                // Replace typed object
                if (r.typedObject)
                {
                    if (r.type == ResourceType::Text)
                        delete static_cast<TextResource*>(r.typedObject);
                    r.typedObject = nullptr;
                }

                r.typedObject = c.typedObject;
                r.state.store(ResourceState::Ready, std::memory_order_release);
                NOC_LOG_INFO("Res", "READY: %s (typed=%u)", r.vpathNormalized.c_str(), (unsigned)r.type);
            }
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
        if (r.type != ResourceType::Binary)
            return nullptr;

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
        if (r.type != ResourceType::Binary)
            return 0;

        if (r.state.load(std::memory_order_acquire) != ResourceState::Ready)
            return 0;

        return r.size;
    }

    const char* ResourceManager::GetError(ResourceHandle h) const
    {
        uint32_t idx = 0;
        if (!ValidateHandle_(h, &idx))
            return "Invalid handle";

        auto* st = static_cast<InternalState*>(state_);
        const Record& r = *st->records[idx];
        if (r.state.load(std::memory_order_acquire) != ResourceState::Failed)
            return nullptr;

        return r.error.c_str();
    }

    bool ResourceManager::WaitUntilReady(ResourceHandle h, uint32_t timeoutMs)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

        while (std::chrono::steady_clock::now() < deadline)
        {
            Update();

            if (IsReady(h))
                return true;

            if (HasFailed(h))
                return false;

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        return IsReady(h);
    }

    const TextResource* ResourceManager::GetText(ResourceHandleT<TextResource> h) const
    {
        if (!state_)
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

} // namespace noc
```
