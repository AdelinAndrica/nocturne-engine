#include "LinearArena.h"
#include "../Assert.h"

#include <algorithm>

namespace noc {

    static std::size_t AlignUp(std::size_t value, std::size_t alignment)
    {
        const std::size_t mask = alignment - 1;
        return (value + mask) & ~mask;
    }

    void LinearArena::Init(void* backingMemory, std::size_t bytes)
    {
        NOC_ASSERT(backingMemory != nullptr);
        NOC_ASSERT(bytes > 0);

        base_ = static_cast<std::byte*>(backingMemory);
        capacity_ = bytes;
        offset_ = 0;
    }

    void LinearArena::Reset()
    {
        offset_ = 0;
    }

    void* LinearArena::Allocate(std::size_t size, std::size_t alignment)
    {
        NOC_ASSERT(base_ != nullptr);
        NOC_ASSERT(size > 0);
        NOC_ASSERT((alignment & (alignment - 1)) == 0);

        const std::size_t alignedOffset = AlignUp(offset_, alignment);
        const std::size_t end = alignedOffset + size;

        NOC_ASSERT(end <= capacity_); // Phase 1: hard fail if out of memory

        void* p = base_ + alignedOffset;
        offset_ = end;
        return p;
    }

} // namespace noc
