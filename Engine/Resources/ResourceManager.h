#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "Resources/ResourceHandle.h"
#include "Resources/Typed/ResourceLoaderRegistry.h"
#include "Resources/Typed/ResourceHandleT.h"
#include "Resources/Typed/TextResource.h"
#include "Resources/Typed/ResourceType.h"

#include "Resources/Typed/MeshResource.h"
#include "Resources/Typed/TextureResource.h"
#include "Resources/Typed/MaterialResource.h"

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

        void Update();

        ResourceHandle RequestBinary(std::string_view vpath);

        bool IsReady(ResourceHandle h) const;
        bool HasFailed(ResourceHandle h) const;

        const uint8_t* GetBytes(ResourceHandle h) const;
        size_t GetSize(ResourceHandle h) const;

        const char* GetError(ResourceHandle h) const;

        bool WaitUntilReady(ResourceHandle h, uint32_t timeoutMs);

        // ---- Typed API ----
        ResourceHandleT<TextResource> RequestText(const char* vpath);
        const TextResource* GetText(ResourceHandleT<TextResource> h) const;

        ResourceHandleT<MeshResource> RequestMesh(const char* vpath);
        const MeshResource* GetMesh(ResourceHandleT<MeshResource> h) const;

        ResourceHandleT<TextureResource> RequestTexture(const char* vpath);
        const TextureResource* GetTexture(ResourceHandleT<TextureResource> h) const;

        ResourceHandleT<MaterialResource> RequestMaterial(const char* vpath);
        const MaterialResource* GetMaterial(ResourceHandleT<MaterialResource> h) const;

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
