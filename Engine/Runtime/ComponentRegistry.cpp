#include "Runtime/ComponentRegistry.h"

#include "Core/Memory/Allocator.h"

#include <cstddef>
#include <cstring>
#include <new>

namespace noc
{
    namespace
    {
        [[nodiscard]] bool IsPowerOfTwo(uint32_t value)
        {
            return value != 0 && (value & (value - 1u)) == 0;
        }

        [[nodiscard]] bool IsMetadataValid(const ComponentTypeMetadata& metadata)
        {
            return metadata.typeId.IsValid()
                && metadata.canonicalName != nullptr
                && metadata.canonicalName[0] != '\0'
                && metadata.version != 0
                && metadata.size != 0
                && IsPowerOfTwo(metadata.alignment);
        }
    }

    struct ComponentRegistry::Impl
    {
        struct Entry
        {
            ComponentTypeMetadata metadata{};
            char* ownedName = nullptr;
        };

        IAllocator* allocator = nullptr;
        Entry* entries = nullptr;
        uint32_t count = 0;
        uint32_t capacity = 0;

        bool EnsureCapacity(uint32_t required)
        {
            if (required <= capacity)
                return true;

            uint32_t target = capacity == 0 ? 16u : capacity;
            while (target < required)
            {
                if (target > 0x7FFFFFFFu)
                {
                    target = required;
                    break;
                }
                target *= 2u;
            }

            auto* newEntries = static_cast<Entry*>(
                allocator->Allocate(
                    sizeof(Entry) * target,
                    alignof(std::max_align_t)));
            if (!newEntries)
                return false;

            for (uint32_t i = 0; i < target; ++i)
                new (newEntries + i) Entry{};

            for (uint32_t i = 0; i < count; ++i)
                newEntries[i] = entries[i];

            if (entries)
            {
                for (uint32_t i = 0; i < capacity; ++i)
                    entries[i].~Entry();
                allocator->Deallocate(entries);
            }

            entries = newEntries;
            capacity = target;
            return true;
        }

        uint32_t LowerBound(ComponentTypeId typeId) const
        {
            uint32_t first = 0;
            uint32_t length = count;

            while (length > 0)
            {
                const uint32_t half = length / 2u;
                const uint32_t middle = first + half;

                if (entries[middle].metadata.typeId < typeId)
                {
                    first = middle + 1u;
                    length -= half + 1u;
                }
                else
                {
                    length = half;
                }
            }

            return first;
        }
    };

    ComponentRegistry::~ComponentRegistry()
    {
        Shutdown();
    }

    bool ComponentRegistry::Init(IAllocator& allocator, uint32_t initialCapacity)
    {
        if (impl_)
            return true;

        void* memory = allocator.Allocate(sizeof(Impl), alignof(Impl));
        if (!memory)
            return false;

        impl_ = new (memory) Impl{};
        impl_->allocator = &allocator;

        if (initialCapacity > 0 && !impl_->EnsureCapacity(initialCapacity))
        {
            impl_->~Impl();
            allocator.Deallocate(impl_);
            impl_ = nullptr;
            return false;
        }

        return true;
    }

    void ComponentRegistry::Shutdown()
    {
        if (!impl_)
            return;

        IAllocator* allocator = impl_->allocator;

        for (uint32_t i = 0; i < impl_->count; ++i)
        {
            if (impl_->entries[i].ownedName)
                allocator->Deallocate(impl_->entries[i].ownedName);
        }

        if (impl_->entries)
        {
            for (uint32_t i = 0; i < impl_->capacity; ++i)
                impl_->entries[i].~Entry();
            allocator->Deallocate(impl_->entries);
        }

        impl_->~Impl();
        allocator->Deallocate(impl_);
        impl_ = nullptr;
    }

    bool ComponentRegistry::Register(const ComponentTypeMetadata& metadata)
    {
        if (!impl_ || !IsMetadataValid(metadata))
            return false;

        const uint32_t insertionIndex = impl_->LowerBound(metadata.typeId);
        if (insertionIndex < impl_->count
            && impl_->entries[insertionIndex].metadata.typeId == metadata.typeId)
        {
            return false;
        }

        for (uint32_t i = 0; i < impl_->count; ++i)
        {
            if (std::strcmp(
                    impl_->entries[i].metadata.canonicalName,
                    metadata.canonicalName) == 0)
            {
                return false;
            }
        }

        if (!impl_->EnsureCapacity(impl_->count + 1u))
            return false;

        const std::size_t nameLength = std::strlen(metadata.canonicalName);
        auto* ownedName = static_cast<char*>(
            impl_->allocator->Allocate(
                nameLength + 1u,
                alignof(std::max_align_t)));
        if (!ownedName)
            return false;

        std::memcpy(ownedName, metadata.canonicalName, nameLength + 1u);

        for (uint32_t i = impl_->count; i > insertionIndex; --i)
            impl_->entries[i] = impl_->entries[i - 1u];

        auto& entry = impl_->entries[insertionIndex];
        entry.metadata = metadata;
        entry.ownedName = ownedName;
        entry.metadata.canonicalName = ownedName;

        ++impl_->count;
        return true;
    }

    const ComponentTypeMetadata* ComponentRegistry::Find(ComponentTypeId typeId) const
    {
        if (!impl_ || !typeId.IsValid() || impl_->count == 0)
            return nullptr;

        const uint32_t index = impl_->LowerBound(typeId);
        if (index >= impl_->count
            || impl_->entries[index].metadata.typeId != typeId)
        {
            return nullptr;
        }

        return &impl_->entries[index].metadata;
    }

    const ComponentTypeMetadata* ComponentRegistry::FindByName(
        const char* canonicalName) const
    {
        if (!impl_ || !canonicalName || canonicalName[0] == '\0')
            return nullptr;

        for (uint32_t i = 0; i < impl_->count; ++i)
        {
            if (std::strcmp(
                    impl_->entries[i].metadata.canonicalName,
                    canonicalName) == 0)
            {
                return &impl_->entries[i].metadata;
            }
        }

        return nullptr;
    }

    uint32_t ComponentRegistry::Count() const
    {
        return impl_ ? impl_->count : 0u;
    }

    const ComponentTypeMetadata* ComponentRegistry::MetadataAt(uint32_t index) const
    {
        if (!impl_ || index >= impl_->count)
            return nullptr;

        return &impl_->entries[index].metadata;
    }
}
