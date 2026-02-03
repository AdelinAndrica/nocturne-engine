#include "DebugAlloc.h"
#include "../Assert.h"

#include <cstdint>

namespace noc {

    // Header placed immediately before the user pointer (at userPtr - sizeof(Header)).
    struct AllocHeader
    {
        void* raw;          // pointer returned by backing allocator
        std::size_t size;   // requested user size
    };

    DebugAlloc::DebugAlloc(IAllocator& backing)
        : backing_(backing)
    {
    }

    void* DebugAlloc::Allocate(std::size_t size, std::size_t alignment)
    {
        NOC_ASSERT(size > 0);
        NOC_ASSERT((alignment & (alignment - 1)) == 0);

        const std::size_t headerSize = sizeof(AllocHeader);

        // Reserve enough space so we can:
        // 1) keep 'raw' somewhere
        // 2) align the returned user pointer to 'alignment'
        const std::size_t total = size + headerSize + alignment;

        void* raw = backing_.Allocate(total, alignof(std::max_align_t));
        NOC_ASSERT(raw != nullptr);

        std::uintptr_t rawAddr = reinterpret_cast<std::uintptr_t>(raw);
        std::uintptr_t userAddr = rawAddr + headerSize;

        const std::uintptr_t mask = static_cast<std::uintptr_t>(alignment - 1);
        userAddr = (userAddr + mask) & ~mask;

        auto* header = reinterpret_cast<AllocHeader*>(userAddr - headerSize);
        header->raw = raw;
        header->size = size;

        totalAllocated_.fetch_add(size);
        outstanding_.fetch_add(size);
        allocCount_.fetch_add(1);

        return reinterpret_cast<void*>(userAddr);
    }

    void DebugAlloc::Deallocate(void* p)
    {
        if (!p) return;

        const std::size_t headerSize = sizeof(AllocHeader);
        std::uintptr_t userAddr = reinterpret_cast<std::uintptr_t>(p);

        auto* header = reinterpret_cast<AllocHeader*>(userAddr - headerSize);
        outstanding_.fetch_sub(header->size);

        backing_.Deallocate(header->raw);
    }

} // namespace noc
