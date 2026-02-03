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
