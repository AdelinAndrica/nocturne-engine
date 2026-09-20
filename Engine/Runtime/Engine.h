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

    /**
     * @brief Top-level runtime owner and frame orchestrator for Nocturne Engine.
     *
     * Engine owns the long-lived engine services used by the application:
     * memory, the frame arena, VFS, jobs, resources, asset import, input,
     * rendering and the runtime World.
     *
     * @par When to use
     * Create one Engine at application/editor startup, configure it, call Init(),
     * attach the required render window, then drive it either with Run() or with
     * the explicit BeginFrame() -> Tick() -> EndFrame() sequence.
     *
     * @par Do not use for
     * Engine is not a gameplay object, service locator for arbitrary game policy,
     * or a place to store editor presentation state.
     *
     * @par Ownership and lifetime
     * The application owns the Engine object. Engine owns its subsystem instances.
     * References returned by VFS(), Resources(), Jobs(), Input(), Assets(),
     * GetWorld() and FrameArena() are borrowed references and must not outlive Engine.
     * FrameArena() allocations are invalidated by the next BeginFrame().
     *
     * @par Threading
     * Treat lifecycle and frame-driving methods as the runtime/main-thread API.
     * Worker execution is delegated internally through JobSystem; this class does
     * not advertise a general thread-safe calling contract.
     *
     * @par Typical standalone flow
     * @code
     * noc::Engine engine;
     * engine.SetContentRoot("Data");
     *
     * if (!engine.Init())
     *     return -1;
     *
     * const int result = engine.Run(); // creates/owns the host loop, not Engine lifetime
     * engine.Shutdown();
     * return result;
     * @endcode
     *
     * @par Typical embedded/editor flow
     * @code
     * engine.BeginFrame();
     * // editor-only pre-Tick work may run here
     * engine.Tick();
     * engine.EndFrame();
     * @endcode
     *
     * @see MainLoop
     * @see World
     * @see ResourceManager
     * @see RenderSystem
     * @ingroup runtime
     */
    class Engine
    {
    public:
        /**
         * @brief Returns the mutable boot configuration.
         *
         * Use this before Init() to configure content/mount policy.
         *
         * @warning Configuration changes after Init() are unsupported. The current
         * implementation logs an error if this function is called after Init(), but
         * still returns the underlying reference; callers must treat the returned
         * configuration as frozen once initialization begins.
         *
         * @return Borrowed reference owned by Engine.
         */
        EngineConfig& ConfigMutable();

        /**
         * @brief Returns the current engine boot configuration as read-only data.
         * @return Borrowed read-only reference owned by Engine.
         */
        const EngineConfig& Config() const { return cfg_; }

        /**
         * @brief Sets the loose development content root consumed by Init().
         *
         * @param path Physical directory path, absolute or process-relative.
         * @return true when accepted before Init(); false after configuration is frozen.
         *
         * @note EngineConfig stores this value as a const char pointer. Keep the
         * referenced character storage alive at least until Init() has consumed it.
         */
        bool SetContentRoot(const char* path);

        /**
         * @brief Sets an optional loose override mount consumed by Init().
         *
         * Later VFS mounts have higher lookup priority, so this mount can override
         * files mounted earlier.
         *
         * @param path Physical directory path, or nullptr to leave it unused.
         * @return true when accepted before Init(); false after initialization.
         */
        bool SetOverrideRoot(const char* path);

        /**
         * @brief Sets an optional archive path consumed by Init().
         * @param path Archive path, or nullptr to leave archive mounting unused.
         * @return true when accepted before Init(); false after initialization.
         */
        bool SetArchivePath(const char* path);

        /**
         * @brief Initializes the engine-owned subsystems in dependency order.
         *
         * Init starts core subsystems, mounts configured VFS sources, initializes
         * Resources, Assets, Input, RenderSystem and World, and freezes the supported
         * boot-configuration phase.
         *
         * @return true when required initialization succeeds; false if a required
         * subsystem cannot initialize.
         *
         * @note Failure to mount an optional configured archive/content/override path
         * is currently logged as a warning and does not by itself make Init() fail.
         *
         * @pre Configure boot paths before this call.
         * @post On success, frame/render attachment APIs may be used.
         */
        bool Init();

        /**
         * @brief Runs the current diagnostic single-tick smoke helper.
         *
         * In the current implementation this is NOT a complete runtime frame. It
         * begins timing, resets/exercises the frame arena, ends timing and logs the
         * result. It does not call InputSystem::Update(), ResourceManager::Update(),
         * World::Update(), render extraction or presentation.
         *
         * @warning Do not use TickOnce() when you need one real game/editor frame.
         * Use BeginFrame() -> Tick() -> EndFrame() instead.
         */
        void TickOnce();

        /**
         * @brief Shuts down engine-owned systems and releases runtime state.
         *
         * Safe to call after normal engine use; individual subsystems implement
         * defensive no-op behavior for already-shutdown state where applicable.
         *
         * @post The Engine may no longer be frame-driven until initialized again.
         */
        void Shutdown();

        /**
         * @brief Runs the standalone Nocturne host window and main loop.
         *
         * Run() creates a 1280x720 resizable Win32 host window, attaches input and
         * rendering, then blocks in MainLoop until the window requests quit.
         *
         * @return 0 after a normal loop exit; -1 if window creation or engine/window
         * attachment fails.
         *
         * @warning Do not use Run() for the editor viewport. The editor owns its
         * outer window/tool UI and uses the embedded frame-driving seam instead.
         *
         * @pre Init() must have succeeded.
         */
        int Run();

        /**
         * @brief Starts one runtime frame.
         *
         * Begins the time system frame, resets the frame arena, starts the input
         * frame and begins rendering when a non-zero render target is attached.
         *
         * @warning Any pointer/reference into previous FrameArena() allocations is
         * invalid after this call.
         *
         * @see Tick
         * @see EndFrame
         */
        void BeginFrame();

        /**
         * @brief Services the simulation/runtime systems for the current frame.
         *
         * Current order is InputSystem::Update(), ResourceManager::Update(), then
         * World::Update(). Resource completions become visible here through the
         * ResourceManager update.
         *
         * @pre Call after BeginFrame().
         * @see BeginFrame
         * @see EndFrame
         */
        void Tick();

        /**
         * @brief Extracts the current World for rendering and completes the frame.
         *
         * When a valid render target is attached, EndFrame builds a RenderQueue in
         * the frame arena, applies the optional debug-selection payload, submits the
         * queue to RenderSystem and presents. It then ends the time-system frame.
         *
         * @pre Call after BeginFrame() and Tick() in the normal frame sequence.
         *
         * @warning The submitted RenderQueue points into frame-arena memory and is
         * valid only for this frame.
         */
        void EndFrame();

        /**
         * @brief Returns the persistent allocator used by engine-owned systems.
         * @return Borrowed allocator reference owned by Engine.
         */
        IAllocator& Allocator();

        /**
         * @brief Returns the transient per-frame linear arena.
         *
         * Allocate temporary frame data here when it must live until EndFrame().
         * Never retain arena pointers across BeginFrame(), which resets the arena.
         */
        LinearArena& FrameArena();

        /** @brief Returns the engine-owned virtual file system. */
        VirtualFileSystem& VFS() { return vfs_; }

        /** @brief Returns the engine-owned resource manager. */
        ResourceManager& Resources() { return resources_; }

        /** @brief Returns the engine-owned resource manager as read-only access. */
        const ResourceManager& Resources() const { return resources_; }

        /** @brief Returns the engine-owned asset import pipeline. */
        AssetImportPipeline& Assets() { return assets_; }

        /** @brief Returns the engine-owned asset import pipeline as read-only access. */
        const AssetImportPipeline& Assets() const { return assets_; }

        /** @brief Returns the engine-owned input system. */
        InputSystem& Input() { return input_; }

        /** @brief Returns the engine-owned input system as read-only access. */
        const InputSystem& Input() const { return input_; }

        /** @brief Returns the engine-owned job system. */
        JobSystem& Jobs() { return jobs_; }

        /** @brief Returns the engine-owned job system as read-only access. */
        const JobSystem& Jobs() const { return jobs_; }

        /** @brief Returns the authoritative runtime World. */
        World& GetWorld() { return world_; }

        /** @brief Returns the authoritative runtime World as read-only access. */
        const World& GetWorld() const { return world_; }

        /**
         * @brief Allocates and initializes the engine frame arena.
         *
         * Normally called by the subsystem registry during Init(); application code
         * should not call this as a separate boot path.
         */
        bool InitMemory();

        /**
         * @brief Releases memory-system storage owned by Engine.
         *
         * Normally called by the subsystem registry during Shutdown().
         */
        void KillMemory();

        /**
         * @brief Connects an existing WinWindow to both input and rendering.
         *
         * @param window Existing application-owned WinWindow.
         * @return true when input and render attachment both succeed.
         *
         * @pre Init() must have succeeded.
         */
        bool AttachWindow(WinWindow& window);

        /**
         * @brief Attaches rendering to one native Win32 HWND.
         *
         * This is the seam used by the editor's dedicated child viewport.
         *
         * @param nativeHwnd Native HWND passed as an opaque pointer.
         * @param clientWidth Current client width in pixels; must be non-zero.
         * @param clientHeight Current client height in pixels; must be non-zero.
         * @return false if Engine is not initialized, the arguments are invalid,
         * rendering is already attached, or RenderSystem attachment fails.
         *
         * @warning The current Engine supports one attached render target at a time.
         */
        bool AttachRenderWindow(void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight);

        /**
         * @brief Resizes the currently attached render target.
         *
         * @param clientWidth New client width.
         * @param clientHeight New client height.
         * @return false if no render target is attached or the renderer rejects a
         * non-zero resize. A zero dimension is accepted and temporarily suppresses
         * rendering without invoking the renderer resize path.
         */
        bool ResizeRenderWindow(uint32_t clientWidth, uint32_t clientHeight);

        /**
         * @brief Creates a WinWindow and immediately attaches Engine to it.
         *
         * @param desc Window creation description.
         * @param outWindow Caller-owned WinWindow that receives the created window.
         * @return true only when both creation and Engine attachment succeed.
         *
         * @note Caller remains responsible for the WinWindow object's lifetime.
         */
        bool CreateAndAttachMainWindow(WinWindowDesc desc, WinWindow& outWindow);

        /**
         * @brief Enables one renderer-facing debug selection box.
         *
         * This is development/editor scaffolding used to depth-test selection
         * visualization without exposing editor types inside rendering.
         *
         * @param localBounds Selected object's local-space bounds.
         * @param world Selected object's world transform.
         *
         * @note Data is copied into Engine; no ownership is transferred.
         */
        void SetDebugSelection(const AABB& localBounds, const Mat4& world);

        /** @brief Disables the renderer-facing debug selection payload. */
        void ClearDebugSelectionBounds();

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
        bool renderAttached_ = false;
        uint32_t renderWidth_ = 0;
        uint32_t renderHeight_ = 0;

        bool debugSelectionEnabled_ = false;
        AABB debugSelectionLocalBounds_{};
        Mat4 debugSelectionWorld_ = Mat4::Identity();
    };

} // namespace noc
