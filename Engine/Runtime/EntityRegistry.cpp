#include "Runtime/EntityRegistry.h"

#include "Core/Memory/Allocator.h"

#include <cstddef>
#include <cstring>
#include <limits>
#include <new>

namespace noc
{
    namespace
    {
        constexpr uint8_t kSlotUnused = 0;
        constexpr uint8_t kSlotAlive = 1;
        constexpr uint8_t kSlotRetired = 2;
        constexpr uint32_t kInitialGeneration = 1;
    }

    struct EntityRegistry::Impl
    {
        IAllocator* allocator = nullptr;

        uint32_t* generations = nullptr;
        uint8_t* states = nullptr;
        uint32_t* freeList = nullptr;

        uint32_t capacity = 0;
        uint32_t nextUnused = 0;
        uint32_t freeCount = 0;
        uint32_t aliveCount = 0;

        bool EnsureCapacity(uint32_t required)
        {
            if (required <= capacity)
                return true;

            // kInvalidEntityIndex is reserved as the invalid handle sentinel.
            if (required == 0 || required >= kInvalidEntityIndex)
                return false;

            uint32_t target = capacity == 0 ? 64u : capacity;
            while (target < required)
            {
                if (target > (kInvalidEntityIndex / 2u))
                {
                    target = required;
                    break;
                }
                target *= 2u;
            }

            constexpr std::size_t kStorageAlignment = alignof(std::max_align_t);
            auto* newGenerations = static_cast<uint32_t*>(
                allocator->Allocate(sizeof(uint32_t) * target, kStorageAlignment));
            auto* newStates = static_cast<uint8_t*>(
                allocator->Allocate(sizeof(uint8_t) * target, kStorageAlignment));
            auto* newFreeList = static_cast<uint32_t*>(
                allocator->Allocate(sizeof(uint32_t) * target, kStorageAlignment));

            if (!newGenerations || !newStates || !newFreeList)
            {
                if (newGenerations) allocator->Deallocate(newGenerations);
                if (newStates) allocator->Deallocate(newStates);
                if (newFreeList) allocator->Deallocate(newFreeList);
                return false;
            }

            if (capacity > 0)
            {
                std::memcpy(newGenerations, generations, sizeof(uint32_t) * capacity);
                std::memcpy(newStates, states, sizeof(uint8_t) * capacity);
            }
            if (freeCount > 0)
                std::memcpy(newFreeList, freeList, sizeof(uint32_t) * freeCount);

            for (uint32_t i = capacity; i < target; ++i)
            {
                newGenerations[i] = kInitialGeneration;
                newStates[i] = kSlotUnused;
            }

            if (generations) allocator->Deallocate(generations);
            if (states) allocator->Deallocate(states);
            if (freeList) allocator->Deallocate(freeList);

            generations = newGenerations;
            states = newStates;
            freeList = newFreeList;
            capacity = target;
            return true;
        }

        bool IsAlive(EntityHandle entity) const
        {
            if (!entity.IsValid() || entity.index >= nextUnused)
                return false;

            return states[entity.index] == kSlotAlive
                && generations[entity.index] == entity.generation;
        }
    };

    EntityRegistry::~EntityRegistry()
    {
        Shutdown();
    }

    bool EntityRegistry::Init(IAllocator& allocator, uint32_t initialCapacity)
    {
        if (impl_)
            return true;

        void* memory = allocator.Allocate(sizeof(Impl), alignof(Impl));
        if (!memory)
            return false;

        impl_ = new (memory) Impl{};
        impl_->allocator = &allocator;

        const uint32_t requestedCapacity = initialCapacity == 0 ? 64u : initialCapacity;
        if (!impl_->EnsureCapacity(requestedCapacity))
        {
            impl_->~Impl();
            allocator.Deallocate(impl_);
            impl_ = nullptr;
            return false;
        }

        return true;
    }

    void EntityRegistry::Shutdown()
    {
        if (!impl_)
            return;

        IAllocator* allocator = impl_->allocator;

        if (impl_->generations) allocator->Deallocate(impl_->generations);
        if (impl_->states) allocator->Deallocate(impl_->states);
        if (impl_->freeList) allocator->Deallocate(impl_->freeList);

        impl_->~Impl();
        allocator->Deallocate(impl_);
        impl_ = nullptr;
    }

    EntityHandle EntityRegistry::Create()
    {
        if (!impl_)
            return EntityHandle::Invalid();

        uint32_t index = kInvalidEntityIndex;

        if (impl_->freeCount > 0)
        {
            index = impl_->freeList[--impl_->freeCount];
        }
        else
        {
            if (impl_->nextUnused >= kInvalidEntityIndex)
                return EntityHandle::Invalid();

            if (impl_->nextUnused == impl_->capacity
                && !impl_->EnsureCapacity(impl_->nextUnused + 1u))
            {
                return EntityHandle::Invalid();
            }

            index = impl_->nextUnused++;
        }

        if (index >= impl_->capacity || impl_->states[index] == kSlotRetired)
            return EntityHandle::Invalid();

        impl_->states[index] = kSlotAlive;
        ++impl_->aliveCount;

        return EntityHandle{ index, impl_->generations[index] };
    }

    bool EntityRegistry::Destroy(EntityHandle entity)
    {
        if (!impl_ || !impl_->IsAlive(entity))
            return false;

        const uint32_t index = entity.index;
        impl_->states[index] = kSlotUnused;
        --impl_->aliveCount;

        // Generation zero is reserved for invalid handles. If a slot reaches the
        // maximum generation, retire it permanently instead of wrapping and
        // potentially resurrecting an ancient stale handle.
        if (impl_->generations[index] == std::numeric_limits<uint32_t>::max())
        {
            impl_->states[index] = kSlotRetired;
            return true;
        }

        ++impl_->generations[index];

        // The free-list is allocated to entity capacity, therefore every dead
        // non-retired slot fits without a mutation-time allocation.
        impl_->freeList[impl_->freeCount++] = index;
        return true;
    }

    bool EntityRegistry::IsAlive(EntityHandle entity) const
    {
        return impl_ && impl_->IsAlive(entity);
    }

    uint32_t EntityRegistry::AliveCount() const
    {
        return impl_ ? impl_->aliveCount : 0u;
    }

    uint32_t EntityRegistry::Capacity() const
    {
        return impl_ ? impl_->capacity : 0u;
    }

    EntityHandle EntityRegistry::EntityAtIndex(uint32_t index) const
    {
        if (!impl_ || index >= impl_->nextUnused || impl_->states[index] != kSlotAlive)
            return EntityHandle::Invalid();

        return EntityHandle{ index, impl_->generations[index] };
    }
}
