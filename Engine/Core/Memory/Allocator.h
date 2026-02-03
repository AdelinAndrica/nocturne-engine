#pragma once
#include <cstddef>

namespace noc {

    class IAllocator
    {
    public:
        virtual ~IAllocator() = default;

        virtual void* Allocate(std::size_t size, std::size_t alignment) = 0;
        virtual void  Deallocate(void* p) = 0;
    };

    class MallocAllocator final : public IAllocator
    {
    public:
        void* Allocate(std::size_t size, std::size_t alignment) override;
        void  Deallocate(void* p) override;
    };

    // Convenience helpers (Design choice: nice ergonomics)
    inline void* Alloc(IAllocator& a, std::size_t size, std::size_t alignment = alignof(std::max_align_t))
    {
        return a.Allocate(size, alignment);
    }

    inline void Free(IAllocator& a, void* p)
    {
        a.Deallocate(p);
    }

} // namespace noc
