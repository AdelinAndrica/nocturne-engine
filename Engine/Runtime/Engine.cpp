#include "Engine.h"

#include <algorithm>
#include <span>
#include <filesystem>

#include "Core/Assert.h"
#include "Core/Log.h"
#include "Core/Clock.h"

#include "Platform/Win32/WinWindow.h"
#include "Runtime/MainLoop.h"

#include "Render/RenderQueue.h"

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
        const uint32_t hw = (std::max)(1u, std::thread::hardware_concurrency());
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

        // Ensure DDC exists and mount it if you want runtime blobs visible via VFS.
        // Design choice: mount loose dir "DerivedDataCache" at the same priority as content.
        std::filesystem::create_directories("DerivedDataCache");
        vfs_.MountLooseDirectory("DerivedDataCache");

        // Start asset pipeline (host-side imports)
        assets_.Init(*this);


		// Phase 7: InputSystem init (HWND comes later in AttachWindow).
		if (!input_.Init(*this))
			return false;

        // Phase 8: RenderSystem init (no HWND yet)
#if NOC_ENABLE_ASSERTS
		const bool enableDebugLayer = true;
#else
		const bool enableDebugLayer = false;
#endif

		if (!render_.Init(enableDebugLayer))
			return false;

        render_.SetResourceManager(&resources_);

        // World is runtime-owned
        if (!world_.Init(Allocator()))
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

    bool Engine::CreateAndAttachMainWindow(WinWindowDesc desc, WinWindow& outWindow)
    {
        if (!outWindow.Create(desc))
            return false;

        if (!AttachWindow(outWindow))
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
            NOC_LOG_FATAL("Runtime", "Failed to attach InputSystem to window");
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
        world_.Update();
    }

    void Engine::EndFrame()
    {
        // Phase 10: runtime builds render submission in FrameArena and hands it to Render.
        // This keeps Render from touching Runtime state.
        // Viewport size is owned by the swapchain; in this phase we mirror host window size.
        // (Design choice) If you already expose swapchain size, wire it here instead.
        const uint32_t viewportW = 1280;
        const uint32_t viewportH = 720;

        RenderQueue rq = world_.BuildRenderQueue(FrameArena(), viewportW, viewportH);
        render_.SetFrameRenderQueue(&rq);
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
		render_.Shutdown();
        world_.Shutdown();
        // Resource manager must shutdown while jobs + memory + log still exist.
        assets_.Shutdown();
        resources_.Shutdown();
		input_.Shutdown();
        registry_.ShutdownAll(this);
    }

} // namespace noc
