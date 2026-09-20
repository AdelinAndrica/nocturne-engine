#pragma once
#include "Dx12Common.h"

#include "Dx12Device.h"
#include "Dx12SwapChain.h"
#include "Dx12FrameSync.h"

#include "Dx12DescriptorAllocator.h"
#include "Dx12DeferredReleaseQueue.h"
#include "Dx12PsoCache.h"
#include "MeshPass.h"

namespace noc
{
    class ResourceManager;
    struct RenderQueue;

    /**
     * @brief Concrete Direct3D 12 backend behind RenderSystem.
     *
     * Dx12Renderer owns device/swap-chain/frame synchronization, descriptor heaps,
     * command allocators/lists, deferred releases, PSO cache and the current MeshPass.
     *
     * @par Beginner guidance
     * Most engine/application/editor code should NOT call Dx12Renderer directly.
     * Use RenderSystem (or Engine's render-window/frame APIs). This class is backend
     * implementation detail for work that specifically belongs to DirectX 12.
     *
     * @par Ownership
     * DX12 objects stored here are owned by the renderer. ResourceManager and
     * RenderQueue pointers are borrowed.
     *
     * @par Frame queue lifetime
     * frameQueue_ points to same-frame data, normally backed by Engine::FrameArena().
     * It must not be retained beyond the frame/present that consumes it.
     *
     * @par Threading
     * This backend is driven as a single render/runtime-thread object in the current
     * architecture. No general concurrent calling contract is provided.
     *
     * @see RenderSystem
     * @see RenderQueue
     * @ingroup rendering
     */
    class Dx12Renderer
    {
    public:
        /**
         * @brief Creates renderer/device-side initialization state.
         * @param enableDebugLayer Whether to request DX12 debug validation.
         * @return true on success.
         */
        bool Init(bool enableDebugLayer);

        /** @brief Releases backend/GPU-facing state. */
        void Shutdown();

        /**
         * @brief Creates/attaches swap-chain rendering for a native HWND.
         * @return true when attachment succeeds.
         */
        bool AttachToWindow(void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight);

        /**
         * @brief Recreates/resizes attached-window render targets.
         * @return true when resize succeeds.
         */
        bool ResizeAttachedWindow(uint32_t clientWidth, uint32_t clientHeight);

        /**
         * @brief Supplies ResourceManager to backend render passes.
         * @param rm Borrowed pointer; ownership remains outside the renderer.
         */
        void SetResourceManager(ResourceManager* rm) { rm_ = rm; }

        /**
         * @brief Supplies the current frame's extracted RenderQueue.
         * @param q Borrowed same-frame pointer; ownership is not transferred.
         */
        void SetFrameRenderQueue(const RenderQueue* q) { frameQueue_ = q; }

        /** @brief Opens/records backend state for one frame. */
        void BeginFrame();

        /** @brief Finishes command submission and presents the current frame. */
        void EndFramePresent();

    private:
        void LogDeviceRemoved_(const char* where);

    private:
        bool inited_ = false;
        bool attached_ = false;

        ResourceManager* rm_ = nullptr;
        const RenderQueue* frameQueue_ = nullptr; // points to FrameArena memory

        Dx12Device device_;
        Dx12SwapChain swap_;
        Dx12FrameSync sync_;

        dx12::ComPtr<ID3D12CommandAllocator> cmdAlloc_[dx12::kFrameCount];
        dx12::ComPtr<ID3D12GraphicsCommandList> cmdList_;

        uint32_t frameIndex_ = 0;
        bool frameOpen_ = false;

        Dx12DescriptorAllocator cbvSrvUavHeap_;
        Dx12DescriptorAllocator samplerHeap_;
        Dx12DeferredReleaseQueue deferred_;
        Dx12PsoCache psoCache_;

        MeshPass meshPass_;
    };
}
