#pragma once
#include <cstdint>

namespace noc
{
    class ResourceManager;
    struct RenderQueue;

    /**
     * @brief High-level rendering facade used by Engine/runtime code.
     *
     * RenderSystem owns the concrete DirectX 12 backend behind an opaque Impl and
     * exposes only the operations the runtime needs: initialize, attach/resize a
     * render target, provide Resources, submit one frame queue, and present.
     *
     * @par When to use
     * Engine owns and calls RenderSystem. Higher-level runtime/editor code should use
     * Engine's render-window seams rather than reaching directly into Dx12Renderer.
     *
     * @par Do not use for
     * RenderSystem is not authoritative world storage and does not own gameplay/editor
     * entities or components.
     *
     * @par Ownership
     * RenderSystem owns its backend. ResourceManager and RenderQueue pointers passed
     * into it are borrowed; ownership is never transferred.
     *
     * @par Frame order
     * @code
     * render.BeginFrame();
     * render.SetFrameRenderQueue(&queue);
     * render.EndFramePresent();
     * @endcode
     *
     * Engine normally performs this sequence for you.
     *
     * @par Threading
     * Treat this as render/runtime-thread API. No general concurrent-call contract is
     * exposed.
     *
     * @see RenderQueue
     * @see Dx12Renderer
     * @ingroup rendering
     */
    class RenderSystem
    {
    public:
        RenderSystem() = default;

        /**
         * @brief Initializes the concrete renderer backend.
         *
         * @param enableDebugLayer Whether the DX12 debug layer should be enabled.
         * @return true on success. Repeated Init() after success is treated as already
         * initialized and returns true.
         */
        bool Init(bool enableDebugLayer);

        /** @brief Shuts down the backend and releases renderer-owned state. */
        void Shutdown();

        /**
         * @brief Attaches rendering to a native window.
         *
         * @param nativeHwnd Native Win32 HWND passed opaquely.
         * @param clientWidth Non-zero client width.
         * @param clientHeight Non-zero client height.
         * @return false when RenderSystem is uninitialized or backend attachment fails.
         */
        bool AttachToWindow(void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight);

        /**
         * @brief Resizes the already-attached render target.
         * @return false when RenderSystem is uninitialized or the backend rejects resize.
         */
        bool ResizeAttachedWindow(uint32_t clientWidth, uint32_t clientHeight);

        /**
         * @brief Supplies the ResourceManager used by render passes.
         *
         * @param rm Borrowed pointer. RenderSystem does not delete it.
         *
         * @warning The pointed manager must remain alive while rendering uses it.
         */
        void SetResourceManager(ResourceManager* rm);

        /**
         * @brief Begins backend recording/state for one render frame.
         *
         * Normally called by Engine::BeginFrame() only when a valid render target exists.
         */
        void BeginFrame();

        /**
         * @brief Finishes the frame and presents the attached swap chain.
         *
         * The current frame queue must remain alive through this call.
         */
        void EndFramePresent();

        /**
         * @brief Sets the renderer-facing submission for the current frame.
         *
         * @param q Borrowed pointer; no ownership is taken.
         *
         * @warning The queue and its instance array must remain valid until
         * EndFramePresent() returns. In the standard runtime this is guaranteed because
         * Engine builds and submits the queue synchronously inside EndFrame().
         */
        void SetFrameRenderQueue(const RenderQueue* q);

    private:
        struct Impl;
        Impl* impl_ = nullptr;
    };
}
