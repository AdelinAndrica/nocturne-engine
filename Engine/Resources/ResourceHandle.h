#pragma once
#include <cstdint>

namespace noc
{
    // Design choice (not directly from the book):
    // index + generation to detect stale handles.
    struct ResourceHandle
    {
        uint32_t index = 0xFFFFFFFFu;
        uint32_t generation = 0;

        constexpr bool IsValid() const { return index != 0xFFFFFFFFu; }

        friend constexpr bool operator==(ResourceHandle a, ResourceHandle b)
        {
            return a.index == b.index && a.generation == b.generation;
        }

        friend constexpr bool operator!=(ResourceHandle a, ResourceHandle b)
        {
            return !(a == b);
        }
    };

} // namespace noc
