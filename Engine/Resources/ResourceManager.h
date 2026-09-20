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

    /**
     * @brief Asynchronous runtime resource cache and typed-resource access layer.
     *
     * ResourceManager converts virtual paths into cached resource records, submits
     * file/decode work to Engine's JobSystem and exposes handles whose state can be
     * polled by the runtime.
     *
     * @par When to use
     * Use ResourceManager when runtime/editor code needs engine content without
     * performing direct synchronous disk I/O itself.
     *
     * @par Typical asynchronous flow
     * @code
     * auto mesh = engine.Resources().RequestMesh("Meshes/crate.nmsh");
     *
     * // Engine::Tick() calls Resources().Update() each frame.
     * if (engine.Resources().IsReady(mesh.Untyped()))
     * {
     *     const noc::MeshResource* data = engine.Resources().GetMesh(mesh);
     *     // data is borrowed; ResourceManager owns it.
     * }
     * @endcode
     *
     * @par Ownership
     * Engine owns ResourceManager. ResourceManager owns loaded binary allocations
     * and decoded typed objects. ResourceHandle/ResourceHandleT values are non-owning.
     * Pointers returned by GetBytes()/GetText()/GetMesh()/GetTexture()/GetMaterial()
     * are borrowed and must not be deleted by callers.
     *
     * @par Completion model
     * Worker jobs perform file/decode work, then push completion records.
     * Update() consumes those completions and commits Ready/Failed state. If Update()
     * is not serviced, completed worker work is not promoted into the public record.
     *
     * @par Threading
     * Internal worker/main-thread synchronization exists for the load pipeline, but
     * the public manager API does not promise arbitrary thread-safe use. Treat
     * requests, Update(), getters and loader-registry mutation as runtime-thread APIs
     * unless a narrower contract is documented later.
     *
     * @par Cache identity
     * Requests are cached by normalized virtual path/resource identity plus type.
     * Asking for the same resource/type returns the existing record handle rather
     * than intentionally creating a second loaded copy.
     *
     * @see VirtualFileSystem
     * @see ResourceHandle
     * @ingroup resources
     */
    class ResourceManager
    {
    public:
        /** @brief Constructs an uninitialized resource manager. Call Init() before use. */
        ResourceManager() = default;

        /**
         * @brief Destroys the manager after defensively calling Shutdown().
         *
         * Normal Engine shutdown should still call Shutdown() explicitly to preserve
         * deterministic subsystem ordering.
         */
        ~ResourceManager();

        /**
         * @brief Copy construction is disabled because the manager uniquely owns
         * internal resource records, worker-completion state and Engine/VFS links.
         */
        ResourceManager(const ResourceManager&) = delete;

        /**
         * @brief Copy assignment is disabled for the same unique-ownership reason.
         */
        ResourceManager& operator=(const ResourceManager&) = delete;

        /**
         * @brief Initializes the resource manager against Engine and VFS.
         *
         * Registers the built-in text, mesh, texture and material loaders.
         *
         * @param engine Owning Engine. Must outlive ResourceManager.
         * @param vfs Engine VFS used by load jobs. Must outlive ResourceManager.
         * @return true. Repeated calls while running are treated as already initialized.
         */
        bool Init(Engine& engine, VirtualFileSystem& vfs);

        /**
         * @brief Stops new operation, waits for in-flight load jobs and frees owned data.
         *
         * Shutdown drains final completions after workers become idle, releases owned
         * binary/typed resource storage and clears the Engine/VFS links.
         */
        void Shutdown();

        /**
         * @brief Applies completed worker requests to public resource records.
         *
         * Engine::Tick() calls this once per normal frame.
         *
         * @par Why this matters
         * Worker completion does not directly mutate all main-thread-facing resource
         * state. Update() is the handoff point that makes success/failure visible.
         */
        void Update();

        /**
         * @brief Requests an asynchronously loaded untyped binary resource.
         *
         * @param vpath Virtual path inside the VFS namespace.
         * @return Non-owning handle. Invalid if the manager is stopped or the path
         * cannot be normalized.
         *
         * @note A valid returned handle does not mean the bytes are ready yet.
         * Poll IsReady()/HasFailed() or use WaitUntilReady() for deliberate blocking.
         */
        ResourceHandle RequestBinary(std::string_view vpath);

        /**
         * @brief Tests whether a resource record reached Ready state.
         * @return false for invalid/stale handles and for resources still loading/failed.
         */
        bool IsReady(ResourceHandle h) const;

        /**
         * @brief Tests whether a resource should be treated as failed.
         *
         * @return true for an invalid/stale handle or a record in Failed state;
         * false otherwise.
         *
         * @note Invalid handles deliberately count as failure in the current API.
         */
        bool HasFailed(ResourceHandle h) const;

        /**
         * @brief Returns binary bytes for a ready Binary resource.
         *
         * @return Borrowed byte pointer, or nullptr if the handle is invalid, the
         * resource is not Binary, or it is not Ready.
         *
         * @warning The pointer is owned by ResourceManager; never free it directly.
         */
        const uint8_t* GetBytes(ResourceHandle h) const;

        /**
         * @brief Returns the byte size of a ready Binary resource.
         * @return Byte count, or 0 if invalid/wrong type/not ready.
         *
         * @note A ready empty binary file also has size 0.
         */
        size_t GetSize(ResourceHandle h) const;

        /**
         * @brief Returns the diagnostic string for a failed resource.
         *
         * @return "Invalid handle" for invalid/stale handles; a borrowed error string
         * for Failed resources; nullptr when the record has not failed.
         */
        const char* GetError(ResourceHandle h) const;

        /**
         * @brief Blocks until a resource becomes Ready, fails, or times out.
         *
         * WaitUntilReady repeatedly calls Update() and sleeps for 1 ms between polls.
         *
         * @param h Resource to wait for.
         * @param timeoutMs Maximum wait duration in milliseconds.
         * @return true only when the resource is Ready at return time.
         *
         * @warning Do not put this in normal per-frame gameplay/render code unless
         * you intentionally accept a blocking stall.
         */
        bool WaitUntilReady(ResourceHandle h, uint32_t timeoutMs);

        /**
         * @brief Requests a typed UTF-8 text resource.
         * @return Non-owning typed handle; readiness is asynchronous.
         */
        ResourceHandleT<TextResource> RequestText(const char* vpath);

        /**
         * @brief Returns a ready TextResource.
         * @return Borrowed pointer, or nullptr for invalid/wrong-type/not-ready state.
         */
        const TextResource* GetText(ResourceHandleT<TextResource> h) const;

        /**
         * @brief Requests a typed mesh resource.
         * @return Non-owning typed handle; readiness is asynchronous.
         */
        ResourceHandleT<MeshResource> RequestMesh(const char* vpath);

        /**
         * @brief Returns a ready MeshResource.
         * @return Borrowed pointer, or nullptr for invalid/wrong-type/not-ready state.
         */
        const MeshResource* GetMesh(ResourceHandleT<MeshResource> h) const;

        /**
         * @brief Requests a typed texture resource.
         * @return Non-owning typed handle; readiness is asynchronous.
         */
        ResourceHandleT<TextureResource> RequestTexture(const char* vpath);

        /**
         * @brief Returns a ready TextureResource.
         * @return Borrowed pointer, or nullptr for invalid/wrong-type/not-ready state.
         */
        const TextureResource* GetTexture(ResourceHandleT<TextureResource> h) const;

        /**
         * @brief Requests a typed material resource.
         * @return Non-owning typed handle; readiness is asynchronous.
         */
        ResourceHandleT<MaterialResource> RequestMaterial(const char* vpath);

        /**
         * @brief Returns a ready MaterialResource.
         * @return Borrowed pointer, or nullptr for invalid/wrong-type/not-ready state.
         */
        const MaterialResource* GetMaterial(ResourceHandleT<MaterialResource> h) const;

        /**
         * @brief Returns the typed-loader registry for registration/introspection.
         *
         * @warning Loader-registry mutation is boot/tooling policy; do not register
         * loaders concurrently with active requests.
         */
        ResourceLoaderRegistry& Loaders() { return loaders_; }

        /** @brief Returns the typed-loader registry as read-only access. */
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
