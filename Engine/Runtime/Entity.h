#pragma once
#include <cstdint>

namespace noc
{
    inline constexpr uint32_t kInvalidEntityIndex = 0xFFFFFFFFu;

    /**
     * @brief Transient runtime identity for one entity slot.
     *
     * EntityHandle uses index + generation so a slot can be recycled without
     * silently making an old handle point at a different live entity.
     *
     * @par Beginner rule: valid is not alive
     * IsValid() checks only whether this value is structurally non-sentinel.
     * It does NOT prove that the referenced entity is still alive.
     * Use World::IsAlive() or EntityRegistry::IsAlive() before relying on it.
     *
     * @par Persistence
     * This is process/runtime identity. Do not serialize EntityHandle as persistent
     * scene identity; saved-scene identity/fixup belongs to the later persistence
     * layer.
     *
     * @par Ownership
     * The handle owns nothing. EntityRegistry owns the slot/generation state.
     *
     * @see EntityRegistry
     * @see World
     * @ingroup world_ecs
     */
    struct EntityHandle
    {
        uint32_t index = kInvalidEntityIndex;
        uint32_t generation = 0;

        /**
         * @brief Performs the local non-sentinel check.
         * @return true when index is not the invalid sentinel and generation != 0.
         *
         * @warning A stale/destroyed handle may still return true here.
         */
        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return index != kInvalidEntityIndex && generation != 0;
        }

        /** @brief Returns the canonical invalid entity handle. */
        [[nodiscard]] static constexpr EntityHandle Invalid() noexcept
        {
            return {};
        }
    };

    [[nodiscard]] constexpr bool operator==(EntityHandle a, EntityHandle b) noexcept
    {
        return a.index == b.index && a.generation == b.generation;
    }

    [[nodiscard]] constexpr bool operator!=(EntityHandle a, EntityHandle b) noexcept
    {
        return !(a == b);
    }
}
