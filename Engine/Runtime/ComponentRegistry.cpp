#include "Runtime/ComponentRegistry.h"

#include "Core/Memory/Allocator.h"
#include "Runtime/Reflection/ReflectionRegistry.h"

#include <cstddef>
#include <cstring>
#include <limits>
#include <new>

namespace noc
{
    struct ComponentRegistry::Impl
    {
        IAllocator* allocator = nullptr;
        const ReflectionRegistry* reflection = nullptr;
        ComponentTypeMetadata* entries = nullptr;
        uint32_t count = 0;

        [[nodiscard]] uint32_t LowerBound(ComponentTypeId typeId) const
        {
            uint32_t first = 0;
            uint32_t length = count;

            while (length > 0)
            {
                const uint32_t half = length / 2u;
                const uint32_t middle = first + half;

                if (entries[middle].typeId < typeId)
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

    bool ComponentRegistry::Init(
        const ReflectionRegistry& reflection,
        IAllocator& allocator)
    {
        if (impl_)
            return false;

        if (!reflection.IsFrozen())
            return false;

        const uint32_t count = reflection.ComponentTypeCount();

        void* implMemory =
            allocator.Allocate(sizeof(Impl), alignof(Impl));
        if (!implMemory)
            return false;

        impl_ = new (implMemory) Impl{};
        impl_->allocator = &allocator;
        impl_->reflection = &reflection;
        impl_->count = count;

        if (count == 0)
            return true;

        impl_->entries = static_cast<ComponentTypeMetadata*>(
            allocator.Allocate(
                sizeof(ComponentTypeMetadata) * count,
                alignof(ComponentTypeMetadata)));
        if (!impl_->entries)
        {
            impl_->~Impl();
            allocator.Deallocate(impl_);
            impl_ = nullptr;
            return false;
        }

        for (uint32_t i = 0; i < count; ++i)
            new (impl_->entries + i) ComponentTypeMetadata{};

        for (uint32_t i = 0; i < count; ++i)
        {
            const TypeMetadata* reflected =
                reflection.ComponentTypeAt(i);

            if (!reflected
                || reflected->typeId.value
                    > std::numeric_limits<uint32_t>::max())
            {
                Shutdown();
                return false;
            }

            ComponentTypeFlags flags = ComponentTypeFlags::None;
            if (HasFlag(reflected->flags, TypeFlags::EditorVisible))
            {
                flags = flags | ComponentTypeFlags::EditorVisible;
            }
            if (HasFlag(reflected->flags, TypeFlags::Serializable))
            {
                flags = flags | ComponentTypeFlags::Serializable;
            }

            impl_->entries[i] = ComponentTypeMetadata{
                ComponentTypeId{
                    static_cast<uint32_t>(reflected->typeId.value) },
                reflected->canonicalName,
                reflected->version,
                reflected->size,
                reflected->alignment,
                flags
            };
        }

        return true;
    }

    void ComponentRegistry::Shutdown()
    {
        if (!impl_)
            return;

        IAllocator* allocator = impl_->allocator;

        if (impl_->entries)
        {
            for (uint32_t i = 0; i < impl_->count; ++i)
                impl_->entries[i].~ComponentTypeMetadata();

            allocator->Deallocate(impl_->entries);
        }

        impl_->~Impl();
        allocator->Deallocate(impl_);
        impl_ = nullptr;
    }

    const ComponentTypeMetadata* ComponentRegistry::Find(
        ComponentTypeId typeId) const
    {
        if (!impl_ || !typeId.IsValid() || impl_->count == 0)
            return nullptr;

        const uint32_t index = impl_->LowerBound(typeId);
        if (index >= impl_->count
            || impl_->entries[index].typeId != typeId)
        {
            return nullptr;
        }

        return &impl_->entries[index];
    }

    const ComponentTypeMetadata* ComponentRegistry::FindByName(
        const char* canonicalName) const
    {
        if (!impl_ || !canonicalName || canonicalName[0] == '\0')
            return nullptr;

        for (uint32_t i = 0; i < impl_->count; ++i)
        {
            if (std::strcmp(
                    impl_->entries[i].canonicalName,
                    canonicalName) == 0)
            {
                return &impl_->entries[i];
            }
        }

        return nullptr;
    }

    uint32_t ComponentRegistry::Count() const
    {
        return impl_ ? impl_->count : 0u;
    }

    const ComponentTypeMetadata* ComponentRegistry::MetadataAt(
        uint32_t index) const
    {
        if (!impl_ || index >= impl_->count)
            return nullptr;

        return &impl_->entries[index];
    }
}
