#pragma once
#include <cstdint>

#include "Runtime/Entity.h"

namespace noc
{
    class IAllocator;

    // Owns transient runtime entity slots and validates generational handles.
    //
    // Structural mutation is main-thread-only in Phase 15.
    // Design choice (not directly from the book): the registry uses a slot
    // table plus a LIFO free-list. Handles never contain pointers and therefore
    // survive internal storage growth.
    class EntityRegistry
    {
    public:
        EntityRegistry() = default;
        ~EntityRegistry();

        EntityRegistry(const EntityRegistry&) = delete;
        EntityRegistry& operator=(const EntityRegistry&) = delete;
        EntityRegistry(EntityRegistry&&) = delete;
        EntityRegistry& operator=(EntityRegistry&&) = delete;

        bool Init(IAllocator& allocator, uint32_t initialCapacity = 64);
        void Shutdown();

        [[nodiscard]] EntityHandle Create();
        [[nodiscard]] bool Destroy(EntityHandle entity);

        [[nodiscard]] bool IsAlive(EntityHandle entity) const;
        [[nodiscard]] uint32_t AliveCount() const;
        [[nodiscard]] uint32_t Capacity() const;

        // Deterministic index-order inspection seam for World queries/tests.
        // Returns EntityHandle::Invalid() for unused, dead, retired or
        // out-of-range slots.
        [[nodiscard]] EntityHandle EntityAtIndex(uint32_t index) const;

    private:
        struct Impl;
        Impl* impl_ = nullptr;
    };
}
