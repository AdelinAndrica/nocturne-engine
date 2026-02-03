#include "Engine.h"
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
		if (initialized_)
		{
			NOC_LOG_ERROR("Runtime", "EngineConfig is frozen after Init(). Modify config before calling Init().");
			return cfg_; // returns current config for inspection; do not mutate.
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

		// Phase 3: VFS mount policy from engine config.
		//
		// Mount priority rule: later mounts override earlier mounts.
		// Desired priority (highest last):
		//   overrideRoot > contentRoot > archive
		//
		// So mount in this order:
		//   1) archivePath
		//   2) contentRoot
		//   3) overrideRoot

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


		return true;
	}

	int Engine::Run()
	{
		WinWindow window;
		WinWindowDesc wd{};
		wd.title = L"NocturneHost"; // Phase 2 behavior unchanged (window config is separate from EngineConfig)
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
		// Phase 3: still intentionally empty.
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
		registry_.ShutdownAll(this);
	}

} // namespace noc
