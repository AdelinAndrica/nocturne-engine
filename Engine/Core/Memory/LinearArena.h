#pragma once
#include <cstddef>
#include <cstdint>

namespace noc {

    class LinearArena
    {
    public:
        LinearArena() = default;

        void Init(void* backingMemory, std::size_t bytes);
        void Reset();

        void* Allocate(std::size_t size, std::size_t alignment);

        std::size_t Capacity() const { return capacity_; }
        std::size_t Used() const { return offset_; }

    private:
        std::byte* base_ = nullptr;
        std::size_t capacity_ = 0;
        std::size_t offset_ = 0;
    };

} // namespace noc
