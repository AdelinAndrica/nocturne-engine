#pragma once
#include <cstdint>

#include "Runtime/Entity.h"

namespace noc
{
    class IAllocator;

    /**
     * @brief Owns transient entity slots and validates generational EntityHandle values.
     *
     * EntityRegistry is the identity layer beneath World. It does not own components;
     * it answers which entity slots are alive and which generation currently owns a slot.
     *
     * @par When to use
     * Most engine/application code should use World rather than manipulating this
     * registry directly. EntityRegistry is primarily the lower-level identity mechanism
     * used by ECS systems and tests.
     *
     * @par Structural mutation
     * Create()/Destroy() change the registry structure and are main-thread-only in the
     * current Phase 15 contract.
     *
     * @par Stale-handle protection
     * Destroy() increments the slot generation before reuse. If a generation reaches
     * uint32_t max, the slot is retired permanently rather than wrapping to a value that
     * could resurrect an ancient stale handle.
     *
     * @par Storage
     * Handles contain no raw pointers and remain meaningful while internal arrays grow.
     *
     * @see EntityHandle
     * @see World
     * @ingroup world_ecs
     */
    class EntityRegistry
    {
    public:
        /** @brief Constructs an uninitialized registry with no slot storage. */
        EntityRegistry() = default;

        /** @brief Destroys the registry after defensively calling Shutdown(). */
        ~EntityRegistry();

        EntityRegistry(const EntityRegistry&) = delete;
        EntityRegistry& operator=(const EntityRegistry&) = delete;
        EntityRegistry(EntityRegistry&&) = delete;
        EntityRegistry& operator=(EntityRegistry&&) = delete;

        /**
         * @brief Allocates the slot/generation/free-list storage.
         *
         * @param allocator Persistent allocator that must remain alive until Shutdown().
         * @param initialCapacity Initial slot capacity. 0 selects the implementation
         * default of 64.
         * @return true on success. Calling Init() again while initialized is treated as
         * already initialized and returns true.
         */
        bool Init(IAllocator& allocator, uint32_t initialCapacity = 64);

        /** @brief Releases all registry storage and invalidates the registry state. */
        void Shutdown();

        /**
         * @brief Creates one live entity identity.
         *
         * Reuses the most recently freed non-retired slot when available; otherwise
         * grows/uses the next unused slot.
         *
         * @return New live handle, or EntityHandle::Invalid() if the registry is not
         * initialized or cannot grow.
         *
         * @note No components are created by this operation.
         */
        [[nodiscard]] EntityHandle Create();

        /**
         * @brief Destroys one currently live identity.
         *
         * @param entity Handle expected to match the current slot generation.
         * @return true when a live matching entity was destroyed; false for stale,
         * invalid, dead or uninitialized input.
         *
         * @warning EntityRegistry itself does not remove ECS components. World performs
         * component teardown before calling this lower-level operation.
         */
        [[nodiscard]] bool Destroy(EntityHandle entity);

        /**
         * @brief Validates both slot state and generation.
         * @return true only when @p entity currently names a live registry entity.
         */
        [[nodiscard]] bool IsAlive(EntityHandle entity) const;

        /** @brief Returns the number of currently live entity slots. */
        [[nodiscard]] uint32_t AliveCount() const;

        /**
         * @brief Returns allocated slot capacity, not the live-entity count.
         * @see AliveCount
         */
        [[nodiscard]] uint32_t Capacity() const;

        /**
         * @brief Inspects a slot by deterministic entity index.
         *
         * Used by World queries/render extraction/tests when deterministic index order
         * matters.
         *
         * @param index Registry slot index.
         * @return Current live handle for that slot, or EntityHandle::Invalid() when
         * unused, dead, retired or out of range.
         *
         * @warning Do not assume indices [0, AliveCount()) are all live; holes are legal.
         */
        [[nodiscard]] EntityHandle EntityAtIndex(uint32_t index) const;

    private:
        struct Impl;
        Impl* impl_ = nullptr;
    };
}
