#include "Engine.h"

#include <algorithm>
#include <span>
#include <filesystem>

#include "Core/Assert.h"
#include "Core/Log.h"
#include "Core/Clock.h"

#include "Platform/Win32/WinWindow.h"
#include "Runtime/MainLoop.h"
#include "Runtime/Reflection/BuiltinTypes.h"
#include "Runtime/Reflection/FoundationComponents.h"

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

        // Phase 16: build the one engine-wide reflected schema explicitly
        // after persistent memory is available and before World startup.
        if (!reflection_.Init(Allocator(), 32))
            return false;

        if (!RegisterBuiltinReflectionTypes(reflection_)
            || !RegisterFoundationComponentReflectionTypes(reflection_)
            || !reflection_.Freeze())
        {
            NOC_LOG_ERROR(
                "Reflection",
                "Reflection startup failed: %s",
                ReflectionRegistryErrorName(reflection_.LastError()));
            reflection_.Shutdown();
            return false;
        }

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

        if (!resources_.Init(*this, vfs_))
            return false;

        std::filesystem::create_directories("DerivedDataCache");
        vfs_.MountLooseDirectory("DerivedDataCache");
        assets_.Init(*this);

        if (!input_.Init(*this))
            return false;

#if NOC_ENABLE_ASSERTS
        const bool enableDebugLayer = true;
#else
        const bool enableDebugLayer = false;
#endif

        if (!render_.Init(enableDebugLayer))
            return false;

        render_.SetResourceManager(&resources_);

        if (!world_.Init(Allocator(), reflection_))
            return false;

        initialized_ = true;
        return true;
    }

    bool Engine::AttachRenderWindow(void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight)
    {
        if (!initialized_ || !nativeHwnd || clientWidth == 0 || clientHeight == 0)
            return false;
        if (renderAttached_)
            return false;

        if (!render_.AttachToWindow(nativeHwnd, clientWidth, clientHeight))
            return false;

        renderAttached_ = true;
        renderWidth_ = clientWidth;
        renderHeight_ = clientHeight;
        return true;
    }

    bool Engine::ResizeRenderWindow(uint32_t clientWidth, uint32_t clientHeight)
    {
        if (!renderAttached_)
            return false;

        renderWidth_ = clientWidth;
        renderHeight_ = clientHeight;
        if (clientWidth == 0 || clientHeight == 0)
            return true;

        return render_.ResizeAttachedWindow(clientWidth, clientHeight);
    }

    bool Engine::AttachWindow(WinWindow& window)
    {
        window.SetRawInputSink(input_.RawSink());

        if (!input_.AttachToWindow(window.Handle()))
            return false;

        return AttachRenderWindow(window.Handle(), window.ClientWidth(), window.ClientHeight());
    }

    bool Engine::CreateAndAttachMainWindow(WinWindowDesc desc, WinWindow& outWindow)
    {
        if (!outWindow.Create(desc))
            return false;

        if (!AttachWindow(outWindow))
            return false;

        return true;
    }

    void Engine::SetDebugSelection(const AABB& localBounds, const Mat4& world)
    {
        debugSelectionLocalBounds_ = localBounds;
        debugSelectionWorld_ = world;
        debugSelectionEnabled_ = true;
    }

    void Engine::ClearDebugSelectionBounds()
    {
        debugSelectionEnabled_ = false;
        debugSelectionWorld_ = Mat4::Identity();
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
            NOC_LOG_FATAL("Runtime", "Failed to attach engine to window");
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
        if (renderAttached_ && renderWidth_ > 0 && renderHeight_ > 0)
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
        if (renderAttached_ && renderWidth_ > 0 && renderHeight_ > 0)
        {
            RenderQueue rq = world_.BuildRenderQueue(FrameArena(), renderWidth_, renderHeight_);
            if (debugSelectionEnabled_)
            {
                rq.debugSelection.enabled = 1;
                rq.debugSelection.localBoundsMin = debugSelectionLocalBounds_.min;
                rq.debugSelection.localBoundsMax = debugSelectionLocalBounds_.max;
                rq.debugSelection.world = debugSelectionWorld_;
            }
            render_.SetFrameRenderQueue(&rq);
            render_.EndFramePresent();
        }
        GetTime().EndFrame();
    }

    void Engine::TickOnce()
    {
        GetTime().BeginFrame();
        FrameArena().Reset();

        void* a = FrameArena().Allocate(256, 16);
        void* b = FrameArena().Allocate(1024, 64);
        (void)a;
        (void)b;

        GetTime().EndFrame();

        NOC_LOG_INFO("Core", "TickOnce() dt=%.6f sec arenaUsed=%zu bytes",
            GetTime().DeltaSeconds(),
            FrameArena().Used());
    }

    void Engine::Shutdown()
    {
        ClearDebugSelectionBounds();
        render_.Shutdown();
        renderAttached_ = false;
        renderWidth_ = renderHeight_ = 0;
        world_.Shutdown();
        reflection_.Shutdown();
        assets_.Shutdown();
        resources_.Shutdown();
        input_.Shutdown();
        registry_.ShutdownAll(this);
        initialized_ = false;
    }

} // namespace noc
