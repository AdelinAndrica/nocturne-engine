#pragma once
#include <cstdint>

namespace noc
{
    inline constexpr uint32_t kInvalidEntityIndex = 0xFFFFFFFFu;

    // Runtime-only entity identity.
    //
    // Design choice (not directly from the book): Nocturne uses an index +
    // generation handle so storage may relocate while stale references remain
    // detectable. This handle is transient runtime identity and must not be
    // serialized as the persistent scene identity introduced in Phase 17.
    struct EntityHandle
    {
        uint32_t index = kInvalidEntityIndex;
        uint32_t generation = 0;

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return index != kInvalidEntityIndex && generation != 0;
        }

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
