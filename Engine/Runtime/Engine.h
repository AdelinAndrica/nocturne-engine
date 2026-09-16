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

#include "Runtime/World.h"
#include <Platform/Win32/WinWindow.h>

#include "Assets/AssetImportPipeline.h"


namespace noc {

	class WinWindow;
	class MainLoop;

	class Engine
	{
	public:
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

		AssetImportPipeline& Assets() { return assets_; }
		const AssetImportPipeline& Assets() const { return assets_; }

		InputSystem& Input() { return input_; }
		const InputSystem& Input() const { return input_; }

		JobSystem& Jobs() { return jobs_; }
		const JobSystem& Jobs() const { return jobs_; }

		World& GetWorld() { return world_; }
		const World& GetWorld() const { return world_; }

		bool InitMemory();
		void KillMemory();

		bool AttachWindow(WinWindow& window);
		bool CreateAndAttachMainWindow(WinWindowDesc desc, WinWindow& outWindow);


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
		AssetImportPipeline assets_;
		InputSystem input_;

		RenderSystem render_;
		World world_;

		EngineConfig cfg_{};
		bool initialized_ = false;
	};

} // namespace noc
