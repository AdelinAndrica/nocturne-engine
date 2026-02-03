#include "Allocator.h"
#include "../Assert.h"

#include <new>
#include <cstdlib>

namespace noc {

    void* MallocAllocator::Allocate(std::size_t size, std::size_t alignment)
    {
        NOC_ASSERT(size > 0);
        NOC_ASSERT((alignment & (alignment - 1)) == 0); // power of two

#if defined(_MSC_VER)
        void* p = _aligned_malloc(size, alignment);
        NOC_ASSERT(p != nullptr);
        return p;
#else
        // Fallback (shouldn't be hit on Windows/MSVC)
        void* p = ::operator new(size, std::align_val_t(alignment));
        return p;
#endif
    }

    void MallocAllocator::Deallocate(void* p)
    {
        if (!p) return;

#if defined(_MSC_VER)
        _aligned_free(p);
#else
        ::operator delete(p);
#endif
    }

} // namespace noc
