#pragma once
#include "Allocator.h"
#include <atomic>
#include <cstddef>

namespace noc {

    class DebugAlloc final : public IAllocator
    {
    public:
        explicit DebugAlloc(IAllocator& backing);

        void* Allocate(std::size_t size, std::size_t alignment) override;
        void  Deallocate(void* p) override;

        std::size_t TotalAllocatedBytes() const { return totalAllocated_.load(); }
        std::size_t OutstandingBytes() const { return outstanding_.load(); }
        std::size_t AllocationCount() const { return allocCount_.load(); }

    private:
        IAllocator& backing_;

        std::atomic<std::size_t> totalAllocated_{ 0 };
        std::atomic<std::size_t> outstanding_{ 0 };
        std::atomic<std::size_t> allocCount_{ 0 };
    };

} // namespace noc
