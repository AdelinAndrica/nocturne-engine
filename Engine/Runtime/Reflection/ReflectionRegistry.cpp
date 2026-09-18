#include "Runtime/Reflection/ReflectionRegistry.h"

#include "Core/Memory/Allocator.h"

#include <cstddef>
#include <cstring>
#include <new>

namespace noc
{
    namespace
    {
        [[nodiscard]] bool IsPowerOfTwo(uint32_t value) noexcept
        {
            return value != 0
                && (value & (value - 1u)) == 0;
        }

        [[nodiscard]] bool IsTypeMetadataIntrinsicallyValid(
            const TypeMetadata& metadata) noexcept
        {
            return metadata.typeId.IsValid()
                && metadata.canonicalName != nullptr
                && metadata.canonicalName[0] != '\0'
                && metadata.kind != TypeKind::Invalid
                && metadata.version != 0
                && metadata.size != 0
                && IsPowerOfTwo(metadata.alignment)
                && metadata.lifecycle.destruct != nullptr;
        }
    }

    struct ReflectionRegistry::Impl
    {
        struct Entry
        {
            TypeMetadata metadata{};
            char* ownedName = nullptr;
        };

        IAllocator* allocator = nullptr;
        Entry* entries = nullptr;
        uint32_t count = 0;
        uint32_t capacity = 0;
        ReflectionRegistryState state =
            ReflectionRegistryState::Uninitialized;

        [[nodiscard]] bool EnsureCapacity(uint32_t required)
        {
            if (required <= capacity)
                return true;

            uint32_t target = capacity == 0 ? 32u : capacity;
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
                    alignof(Entry)));
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

        [[nodiscard]] uint32_t LowerBound(TypeId typeId) const noexcept
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

    ReflectionRegistry::~ReflectionRegistry()
    {
        Shutdown();
    }

    bool ReflectionRegistry::Init(
        IAllocator& allocator,
        uint32_t initialTypeCapacity)
    {
        if (impl_)
        {
            lastError_ = ReflectionRegistryError::WrongState;
            return false;
        }

        void* memory =
            allocator.Allocate(sizeof(Impl), alignof(Impl));
        if (!memory)
        {
            lastError_ =
                ReflectionRegistryError::AllocationFailure;
            return false;
        }

        impl_ = new (memory) Impl{};
        impl_->allocator = &allocator;
        impl_->state = ReflectionRegistryState::Building;

        if (initialTypeCapacity > 0
            && !impl_->EnsureCapacity(initialTypeCapacity))
        {
            impl_->~Impl();
            allocator.Deallocate(impl_);
            impl_ = nullptr;
            lastError_ =
                ReflectionRegistryError::AllocationFailure;
            return false;
        }

        lastError_ = ReflectionRegistryError::None;
        return true;
    }

    void ReflectionRegistry::Shutdown()
    {
        if (!impl_)
        {
            lastError_ = ReflectionRegistryError::None;
            return;
        }

        IAllocator* allocator = impl_->allocator;

        for (uint32_t i = 0; i < impl_->count; ++i)
        {
            if (impl_->entries[i].ownedName)
                allocator->Deallocate(
                    impl_->entries[i].ownedName);
        }

        if (impl_->entries)
        {
            for (uint32_t i = 0;
                 i < impl_->capacity;
                 ++i)
            {
                impl_->entries[i].~Entry();
            }

            allocator->Deallocate(impl_->entries);
        }

        impl_->state =
            ReflectionRegistryState::Uninitialized;
        impl_->~Impl();
        allocator->Deallocate(impl_);
        impl_ = nullptr;
        lastError_ = ReflectionRegistryError::None;
    }

    bool ReflectionRegistry::RegisterType(
        const TypeMetadata& metadata)
    {
        if (!impl_)
        {
            lastError_ =
                ReflectionRegistryError::NotInitialized;
            return false;
        }

        if (impl_->state != ReflectionRegistryState::Building)
        {
            lastError_ =
                ReflectionRegistryError::WrongState;
            return false;
        }

        if (!IsTypeMetadataIntrinsicallyValid(metadata))
        {
            lastError_ =
                ReflectionRegistryError::InvalidMetadata;
            return false;
        }

        const uint32_t insertionIndex =
            impl_->LowerBound(metadata.typeId);

        if (insertionIndex < impl_->count
            && impl_->entries[insertionIndex]
                    .metadata.typeId == metadata.typeId)
        {
            lastError_ =
                ReflectionRegistryError::DuplicateTypeId;
            return false;
        }

        for (uint32_t i = 0; i < impl_->count; ++i)
        {
            if (std::strcmp(
                    impl_->entries[i]
                        .metadata.canonicalName,
                    metadata.canonicalName) == 0)
            {
                lastError_ =
                    ReflectionRegistryError::
                        DuplicateCanonicalName;
                return false;
            }
        }

        if (!impl_->EnsureCapacity(impl_->count + 1u))
        {
            lastError_ =
                ReflectionRegistryError::AllocationFailure;
            return false;
        }

        const std::size_t nameLength =
            std::strlen(metadata.canonicalName);

        auto* ownedName = static_cast<char*>(
            impl_->allocator->Allocate(
                nameLength + 1u,
                alignof(char)));
        if (!ownedName)
        {
            lastError_ =
                ReflectionRegistryError::AllocationFailure;
            return false;
        }

        std::memcpy(
            ownedName,
            metadata.canonicalName,
            nameLength + 1u);

        for (uint32_t i = impl_->count;
             i > insertionIndex;
             --i)
        {
            impl_->entries[i] =
                impl_->entries[i - 1u];
        }

        auto& entry =
            impl_->entries[insertionIndex];

        entry.metadata = metadata;
        entry.ownedName = ownedName;
        entry.metadata.canonicalName = ownedName;

        ++impl_->count;
        lastError_ = ReflectionRegistryError::None;
        return true;
    }

    bool ReflectionRegistry::Freeze()
    {
        if (!impl_)
        {
            lastError_ =
                ReflectionRegistryError::NotInitialized;
            return false;
        }

        if (impl_->state != ReflectionRegistryState::Building)
        {
            lastError_ =
                ReflectionRegistryError::WrongState;
            return false;
        }

        for (uint32_t i = 0; i < impl_->count; ++i)
        {
            const TypeMetadata& metadata =
                impl_->entries[i].metadata;

            if (!IsTypeMetadataIntrinsicallyValid(metadata))
            {
                lastError_ =
                    ReflectionRegistryError::ValidationFailure;
                return false;
            }

            if (i > 0
                && !(impl_->entries[i - 1u]
                         .metadata.typeId
                     < metadata.typeId))
            {
                lastError_ =
                    ReflectionRegistryError::ValidationFailure;
                return false;
            }
        }

        impl_->state = ReflectionRegistryState::Frozen;
        lastError_ = ReflectionRegistryError::None;
        return true;
    }

    ReflectionRegistryState
    ReflectionRegistry::State() const noexcept
    {
        return impl_
            ? impl_->state
            : ReflectionRegistryState::Uninitialized;
    }

    ReflectionRegistryError
    ReflectionRegistry::LastError() const noexcept
    {
        return lastError_;
    }

    bool ReflectionRegistry::IsFrozen() const noexcept
    {
        return State() == ReflectionRegistryState::Frozen;
    }

    uint32_t ReflectionRegistry::TypeCount() const noexcept
    {
        return impl_ ? impl_->count : 0u;
    }

    const TypeMetadata* ReflectionRegistry::FindType(
        TypeId typeId) const noexcept
    {
        if (!impl_
            || !typeId.IsValid()
            || impl_->count == 0)
        {
            return nullptr;
        }

        const uint32_t index =
            impl_->LowerBound(typeId);

        if (index >= impl_->count
            || impl_->entries[index]
                   .metadata.typeId != typeId)
        {
            return nullptr;
        }

        return &impl_->entries[index].metadata;
    }

    const TypeMetadata*
    ReflectionRegistry::FindTypeByName(
        const char* canonicalName) const noexcept
    {
        if (!impl_
            || !canonicalName
            || canonicalName[0] == '\0')
        {
            return nullptr;
        }

        for (uint32_t i = 0; i < impl_->count; ++i)
        {
            if (std::strcmp(
                    impl_->entries[i]
                        .metadata.canonicalName,
                    canonicalName) == 0)
            {
                return &impl_->entries[i].metadata;
            }
        }

        return nullptr;
    }

    const TypeMetadata* ReflectionRegistry::TypeAt(
        uint32_t index) const noexcept
    {
        if (!impl_ || index >= impl_->count)
            return nullptr;

        return &impl_->entries[index].metadata;
    }
}
