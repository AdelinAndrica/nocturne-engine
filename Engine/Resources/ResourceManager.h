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
