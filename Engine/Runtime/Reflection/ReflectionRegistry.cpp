#include "Runtime/Reflection/ReflectionRegistry.h"

#include "Core/Memory/Allocator.h"

#include <cmath>
#include <cstddef>
#include <cstring>
#include <new>

namespace noc
{
    namespace
    {
        [[nodiscard]] bool IsPowerOfTwo(uint32_t value) noexcept
        {
            return value != 0 && (value & (value - 1u)) == 0;
        }

        [[nodiscard]] AttributeValueKind ExpectedAttributeValueKind(
            AttributeKind kind) noexcept
        {
            switch (kind)
            {
            case AttributeKind::DisplayName:
            case AttributeKind::Category:
            case AttributeKind::Tooltip:
            case AttributeKind::Units:
            case AttributeKind::EditorWidgetHint:
            case AttributeKind::SerializationAlias:
            case AttributeKind::ScriptingAlias:
            case AttributeKind::ReadOnlyReason:
                return AttributeValueKind::String;
            case AttributeKind::NumericRange:
                return AttributeValueKind::Range;
            case AttributeKind::NumericStep:
                return AttributeValueKind::Number;
            case AttributeKind::ResourceTypeConstraint:
                return AttributeValueKind::TypeId;
            case AttributeKind::Angle:
            case AttributeKind::Color:
            case AttributeKind::Multiline:
                return AttributeValueKind::Boolean;
            case AttributeKind::Invalid:
                break;
            }
            return AttributeValueKind::None;
        }

        [[nodiscard]] ReflectionRegistryError ValidateAttributes(
            const AttributeMetadata* attributes,
            uint32_t count) noexcept
        {
            if (count == 0)
                return attributes == nullptr
                    ? ReflectionRegistryError::None
                    : ReflectionRegistryError::InvalidAttributeMetadata;

            if (!attributes)
                return ReflectionRegistryError::InvalidAttributeMetadata;

            for (uint32_t i = 0; i < count; ++i)
            {
                const AttributeMetadata& attribute = attributes[i];
                const AttributeValueKind expected =
                    ExpectedAttributeValueKind(attribute.kind);

                if (attribute.kind == AttributeKind::Invalid
                    || expected == AttributeValueKind::None
                    || attribute.valueKind != expected)
                {
                    return ReflectionRegistryError::InvalidAttributeMetadata;
                }

                if (attribute.valueKind == AttributeValueKind::String)
                {
                    if (!attribute.stringValue
                        || attribute.stringValue[0] == '\0')
                    {
                        return ReflectionRegistryError::InvalidAttributeMetadata;
                    }
                }
                else if (attribute.valueKind == AttributeValueKind::Number)
                {
                    if (!std::isfinite(attribute.numberA))
                        return ReflectionRegistryError::InvalidAttributeMetadata;
                }
                else if (attribute.valueKind == AttributeValueKind::Range)
                {
                    if (!std::isfinite(attribute.numberA)
                        || !std::isfinite(attribute.numberB)
                        || attribute.numberA > attribute.numberB)
                    {
                        return ReflectionRegistryError::InvalidAttributeMetadata;
                    }
                }
                else if (attribute.valueKind == AttributeValueKind::TypeId)
                {
                    if (!attribute.typeIdValue.IsValid())
                        return ReflectionRegistryError::InvalidAttributeMetadata;
                }

                for (uint32_t j = 0; j < i; ++j)
                {
                    if (attributes[j].kind == attribute.kind)
                        return ReflectionRegistryError::DuplicateAttributeKind;
                }
            }

            return ReflectionRegistryError::None;
        }

        [[nodiscard]] bool IsPropertyBaseValid(
            const PropertyMetadata& property,
            TypeId expectedOwner) noexcept
        {
            if (!property.propertyId.IsValid()
                || !property.canonicalName
                || property.canonicalName[0] == '\0'
                || property.ownerTypeId != expectedOwner
                || !property.valueTypeId.IsValid()
                || !property.read)
            {
                return false;
            }

            const bool readOnly =
                HasFlag(property.flags, PropertyFlags::ReadOnly);
            if (readOnly)
            {
                return property.write == nullptr
                    && property.mutableAddress == nullptr;
            }

            return property.write != nullptr;
        }

        [[nodiscard]] ReflectionRegistryError ValidateEnumIntrinsic(
            const TypeMetadata& metadata) noexcept
        {
            if (metadata.kind != TypeKind::Enum)
            {
                return metadata.enumMetadata == nullptr
                    ? ReflectionRegistryError::None
                    : ReflectionRegistryError::InvalidMetadata;
            }

            if (!metadata.enumMetadata
                || !metadata.enumMetadata->underlyingTypeId.IsValid()
                || !metadata.enumMetadata->values
                || metadata.enumMetadata->valueCount == 0)
            {
                return ReflectionRegistryError::InvalidMetadata;
            }

            for (uint32_t i = 0; i < metadata.enumMetadata->valueCount; ++i)
            {
                const EnumValueMetadata& value = metadata.enumMetadata->values[i];
                if (!value.valueId.IsValid()
                    || !value.canonicalName
                    || value.canonicalName[0] == '\0')
                {
                    return ReflectionRegistryError::InvalidMetadata;
                }

                for (uint32_t j = 0; j < i; ++j)
                {
                    const EnumValueMetadata& previous =
                        metadata.enumMetadata->values[j];
                    if (previous.valueId == value.valueId)
                        return ReflectionRegistryError::DuplicateEnumValueId;
                    if (std::strcmp(
                            previous.canonicalName,
                            value.canonicalName) == 0)
                    {
                        return ReflectionRegistryError::
                            DuplicateEnumValueCanonicalName;
                    }
                    if (previous.rawValue == value.rawValue)
                        return ReflectionRegistryError::DuplicateEnumNumericValue;
                }
            }

            return ReflectionRegistryError::None;
        }

        [[nodiscard]] ReflectionRegistryError ValidateContainerIntrinsic(
            const TypeMetadata& metadata) noexcept
        {
            const bool isContainer =
                metadata.kind == TypeKind::FixedArray
                || metadata.kind == TypeKind::DynamicSequence;

            if (!isContainer)
            {
                return metadata.containerMetadata == nullptr
                    ? ReflectionRegistryError::None
                    : ReflectionRegistryError::InvalidContainerMetadata;
            }

            const ContainerMetadata* container = metadata.containerMetadata;
            if (!container
                || !container->elementTypeId.IsValid()
                || !container->count
                || !container->constElement
                || !container->capacity)
            {
                return ReflectionRegistryError::InvalidContainerMetadata;
            }

            if (metadata.kind == TypeKind::FixedArray)
            {
                if (container->fixedCount == 0
                    || container->resize
                    || container->insertDefault
                    || container->remove)
                {
                    return ReflectionRegistryError::InvalidContainerMetadata;
                }
            }
            else if (container->fixedCount != 0)
            {
                return ReflectionRegistryError::InvalidContainerMetadata;
            }

            if (container->readOnly)
            {
                if (container->mutableElement
                    || container->resize
                    || container->insertDefault
                    || container->remove)
                {
                    return ReflectionRegistryError::InvalidContainerMetadata;
                }
            }
            else if (!container->mutableElement)
            {
                return ReflectionRegistryError::InvalidContainerMetadata;
            }

            return ReflectionRegistryError::None;
        }

        [[nodiscard]] ReflectionRegistryError ValidateTypeIntrinsic(
            const TypeMetadata& metadata) noexcept
        {
            if (!metadata.typeId.IsValid()
                || !metadata.canonicalName
                || metadata.canonicalName[0] == '\0'
                || metadata.kind == TypeKind::Invalid
                || metadata.version == 0
                || metadata.size == 0
                || !IsPowerOfTwo(metadata.alignment)
                || !metadata.lifecycle.destruct)
            {
                return ReflectionRegistryError::InvalidMetadata;
            }

            ReflectionRegistryError attributeError =
                ValidateAttributes(metadata.attributes, metadata.attributeCount);
            if (attributeError != ReflectionRegistryError::None)
                return attributeError;

            if (metadata.propertyCount == 0)
            {
                if (metadata.properties != nullptr)
                    return ReflectionRegistryError::InvalidMetadata;
            }
            else
            {
                if (!metadata.properties)
                    return ReflectionRegistryError::InvalidMetadata;

                for (uint32_t i = 0; i < metadata.propertyCount; ++i)
                {
                    const PropertyMetadata& property = metadata.properties[i];
                    if (!IsPropertyBaseValid(property, metadata.typeId))
                        return ReflectionRegistryError::InvalidMetadata;

                    attributeError = ValidateAttributes(
                        property.attributes,
                        property.attributeCount);
                    if (attributeError != ReflectionRegistryError::None)
                        return attributeError;

                    for (uint32_t j = 0; j < i; ++j)
                    {
                        const PropertyMetadata& previous = metadata.properties[j];
                        if (previous.propertyId == property.propertyId)
                            return ReflectionRegistryError::DuplicatePropertyId;
                        if (std::strcmp(
                                previous.canonicalName,
                                property.canonicalName) == 0)
                        {
                            return ReflectionRegistryError::
                                DuplicatePropertyCanonicalName;
                        }
                    }
                }
            }

            const ReflectionRegistryError enumError =
                ValidateEnumIntrinsic(metadata);
            if (enumError != ReflectionRegistryError::None)
                return enumError;

            return ValidateContainerIntrinsic(metadata);
        }

        [[nodiscard]] char* CopyString(
            IAllocator& allocator,
            const char* source)
        {
            const std::size_t length = std::strlen(source);
            auto* copy = static_cast<char*>(
                allocator.Allocate(length + 1u, alignof(char)));
            if (!copy)
                return nullptr;
            std::memcpy(copy, source, length + 1u);
            return copy;
        }

        void DestroyAttributes(
            IAllocator& allocator,
            const AttributeMetadata*& attributes,
            uint32_t& count)
        {
            if (!attributes)
            {
                count = 0;
                return;
            }

            auto* owned = const_cast<AttributeMetadata*>(attributes);
            for (uint32_t i = 0; i < count; ++i)
            {
                if (owned[i].valueKind == AttributeValueKind::String
                    && owned[i].stringValue)
                {
                    allocator.Deallocate(
                        const_cast<char*>(owned[i].stringValue));
                }
            }

            allocator.Deallocate(owned);
            attributes = nullptr;
            count = 0;
        }

        [[nodiscard]] bool CopyAttributes(
            IAllocator& allocator,
            const AttributeMetadata* source,
            uint32_t count,
            const AttributeMetadata*& destination,
            uint32_t& destinationCount)
        {
            destination = nullptr;
            destinationCount = 0;
            if (count == 0)
                return true;

            auto* copy = static_cast<AttributeMetadata*>(
                allocator.Allocate(
                    sizeof(AttributeMetadata) * count,
                    alignof(AttributeMetadata)));
            if (!copy)
                return false;

            for (uint32_t i = 0; i < count; ++i)
                new (copy + i) AttributeMetadata(source[i]);

            destination = copy;
            destinationCount = count;

            for (uint32_t i = 0; i < count; ++i)
            {
                if (copy[i].valueKind == AttributeValueKind::String)
                {
                    copy[i].stringValue = nullptr;
                    copy[i].stringValue =
                        CopyString(allocator, source[i].stringValue);
                    if (!copy[i].stringValue)
                    {
                        DestroyAttributes(
                            allocator,
                            destination,
                            destinationCount);
                        return false;
                    }
                }
            }

            return true;
        }

        void DestroyOwnedMetadata(
            IAllocator& allocator,
            TypeMetadata& metadata)
        {
            if (metadata.containerMetadata)
            {
                auto* containerMetadata =
                    const_cast<ContainerMetadata*>(metadata.containerMetadata);
                containerMetadata->~ContainerMetadata();
                allocator.Deallocate(containerMetadata);
                metadata.containerMetadata = nullptr;
            }

            if (metadata.enumMetadata)
            {
                auto* enumMetadata =
                    const_cast<EnumMetadata*>(metadata.enumMetadata);
                if (enumMetadata->values)
                {
                    auto* values =
                        const_cast<EnumValueMetadata*>(enumMetadata->values);
                    for (uint32_t i = 0; i < enumMetadata->valueCount; ++i)
                    {
                        if (values[i].canonicalName)
                        {
                            allocator.Deallocate(
                                const_cast<char*>(values[i].canonicalName));
                        }
                    }
                    allocator.Deallocate(values);
                }
                enumMetadata->~EnumMetadata();
                allocator.Deallocate(enumMetadata);
                metadata.enumMetadata = nullptr;
            }

            if (metadata.properties)
            {
                auto* properties =
                    const_cast<PropertyMetadata*>(metadata.properties);
                for (uint32_t i = 0; i < metadata.propertyCount; ++i)
                {
                    DestroyAttributes(
                        allocator,
                        properties[i].attributes,
                        properties[i].attributeCount);
                    if (properties[i].canonicalName)
                    {
                        allocator.Deallocate(
                            const_cast<char*>(properties[i].canonicalName));
                    }
                }
                allocator.Deallocate(properties);
                metadata.properties = nullptr;
                metadata.propertyCount = 0;
            }

            DestroyAttributes(
                allocator,
                metadata.attributes,
                metadata.attributeCount);

            if (metadata.canonicalName)
            {
                allocator.Deallocate(
                    const_cast<char*>(metadata.canonicalName));
                metadata.canonicalName = nullptr;
            }
        }

        [[nodiscard]] bool DeepCopyMetadata(
            IAllocator& allocator,
            const TypeMetadata& source,
            TypeMetadata& destination)
        {
            destination = source;
            destination.canonicalName = nullptr;
            destination.properties = nullptr;
            destination.propertyCount = 0;
            destination.attributes = nullptr;
            destination.attributeCount = 0;
            destination.enumMetadata = nullptr;
            destination.containerMetadata = nullptr;

            destination.canonicalName =
                CopyString(allocator, source.canonicalName);
            if (!destination.canonicalName)
                return false;

            if (!CopyAttributes(
                    allocator,
                    source.attributes,
                    source.attributeCount,
                    destination.attributes,
                    destination.attributeCount))
            {
                DestroyOwnedMetadata(allocator, destination);
                return false;
            }

            if (source.propertyCount > 0)
            {
                auto* properties = static_cast<PropertyMetadata*>(
                    allocator.Allocate(
                        sizeof(PropertyMetadata) * source.propertyCount,
                        alignof(PropertyMetadata)));
                if (!properties)
                {
                    DestroyOwnedMetadata(allocator, destination);
                    return false;
                }

                for (uint32_t i = 0; i < source.propertyCount; ++i)
                    new (properties + i) PropertyMetadata{};

                destination.properties = properties;
                destination.propertyCount = source.propertyCount;

                for (uint32_t i = 0; i < source.propertyCount; ++i)
                {
                    properties[i] = source.properties[i];
                    properties[i].canonicalName = nullptr;
                    properties[i].attributes = nullptr;
                    properties[i].attributeCount = 0;

                    properties[i].canonicalName =
                        CopyString(allocator, source.properties[i].canonicalName);
                    if (!properties[i].canonicalName
                        || !CopyAttributes(
                            allocator,
                            source.properties[i].attributes,
                            source.properties[i].attributeCount,
                            properties[i].attributes,
                            properties[i].attributeCount))
                    {
                        DestroyOwnedMetadata(allocator, destination);
                        return false;
                    }
                }
            }

            if (source.containerMetadata)
            {
                auto* containerMetadata = static_cast<ContainerMetadata*>(
                    allocator.Allocate(
                        sizeof(ContainerMetadata),
                        alignof(ContainerMetadata)));
                if (!containerMetadata)
                {
                    DestroyOwnedMetadata(allocator, destination);
                    return false;
                }

                new (containerMetadata) ContainerMetadata(
                    *source.containerMetadata);
                destination.containerMetadata = containerMetadata;
            }

            if (source.enumMetadata)
            {
                auto* enumMetadata = static_cast<EnumMetadata*>(
                    allocator.Allocate(sizeof(EnumMetadata), alignof(EnumMetadata)));
                if (!enumMetadata)
                {
                    DestroyOwnedMetadata(allocator, destination);
                    return false;
                }

                new (enumMetadata) EnumMetadata(*source.enumMetadata);
                enumMetadata->values = nullptr;
                destination.enumMetadata = enumMetadata;

                auto* values = static_cast<EnumValueMetadata*>(
                    allocator.Allocate(
                        sizeof(EnumValueMetadata) * source.enumMetadata->valueCount,
                        alignof(EnumValueMetadata)));
                if (!values)
                {
                    DestroyOwnedMetadata(allocator, destination);
                    return false;
                }

                for (uint32_t i = 0; i < source.enumMetadata->valueCount; ++i)
                    new (values + i) EnumValueMetadata{};

                enumMetadata->values = values;

                for (uint32_t i = 0; i < source.enumMetadata->valueCount; ++i)
                {
                    values[i] = source.enumMetadata->values[i];
                    values[i].canonicalName = nullptr;
                    values[i].canonicalName =
                        CopyString(
                            allocator,
                            source.enumMetadata->values[i].canonicalName);
                    if (!values[i].canonicalName)
                    {
                        DestroyOwnedMetadata(allocator, destination);
                        return false;
                    }
                }
            }

            return true;
        }
    }

    struct ReflectionRegistry::Impl
    {
        struct Entry { TypeMetadata metadata{}; };
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
                target *= 2u;

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

    ReflectionRegistry::~ReflectionRegistry() { Shutdown(); }

    bool ReflectionRegistry::Init(
        IAllocator& allocator,
        uint32_t initialTypeCapacity)
    {
        if (impl_)
        {
            lastError_ = ReflectionRegistryError::WrongState;
            return false;
        }

        void* memory = allocator.Allocate(sizeof(Impl), alignof(Impl));
        if (!memory)
        {
            lastError_ = ReflectionRegistryError::AllocationFailure;
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
            lastError_ = ReflectionRegistryError::AllocationFailure;
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
            DestroyOwnedMetadata(*allocator, impl_->entries[i].metadata);

        if (impl_->entries)
        {
            for (uint32_t i = 0; i < impl_->capacity; ++i)
                impl_->entries[i].~Entry();
            allocator->Deallocate(impl_->entries);
        }

        impl_->~Impl();
        allocator->Deallocate(impl_);
        impl_ = nullptr;
        lastError_ = ReflectionRegistryError::None;
    }

    bool ReflectionRegistry::RegisterType(const TypeMetadata& metadata)
    {
        if (!impl_)
        {
            lastError_ = ReflectionRegistryError::NotInitialized;
            return false;
        }
        if (impl_->state != ReflectionRegistryState::Building)
        {
            lastError_ = ReflectionRegistryError::WrongState;
            return false;
        }

        lastError_ = ValidateTypeIntrinsic(metadata);
        if (lastError_ != ReflectionRegistryError::None)
            return false;

        const uint32_t insertionIndex = impl_->LowerBound(metadata.typeId);
        if (insertionIndex < impl_->count
            && impl_->entries[insertionIndex].metadata.typeId == metadata.typeId)
        {
            lastError_ = ReflectionRegistryError::DuplicateTypeId;
            return false;
        }

        for (uint32_t i = 0; i < impl_->count; ++i)
        {
            if (std::strcmp(
                    impl_->entries[i].metadata.canonicalName,
                    metadata.canonicalName) == 0)
            {
                lastError_ = ReflectionRegistryError::DuplicateCanonicalName;
                return false;
            }
        }

        if (!impl_->EnsureCapacity(impl_->count + 1u))
        {
            lastError_ = ReflectionRegistryError::AllocationFailure;
            return false;
        }

        TypeMetadata owned{};
        if (!DeepCopyMetadata(*impl_->allocator, metadata, owned))
        {
            lastError_ = ReflectionRegistryError::AllocationFailure;
            return false;
        }

        for (uint32_t i = impl_->count; i > insertionIndex; --i)
            impl_->entries[i] = impl_->entries[i - 1u];

        impl_->entries[insertionIndex].metadata = owned;
        ++impl_->count;
        lastError_ = ReflectionRegistryError::None;
        return true;
    }

    bool ReflectionRegistry::Freeze()
    {
        if (!impl_)
        {
            lastError_ = ReflectionRegistryError::NotInitialized;
            return false;
        }
        if (impl_->state != ReflectionRegistryState::Building)
        {
            lastError_ = ReflectionRegistryError::WrongState;
            return false;
        }

        for (uint32_t i = 0; i < impl_->count; ++i)
        {
            const TypeMetadata& metadata = impl_->entries[i].metadata;
            if (ValidateTypeIntrinsic(metadata) != ReflectionRegistryError::None)
            {
                lastError_ = ReflectionRegistryError::ValidationFailure;
                return false;
            }

            if (i > 0
                && !(impl_->entries[i - 1u].metadata.typeId < metadata.typeId))
            {
                lastError_ = ReflectionRegistryError::ValidationFailure;
                return false;
            }

            for (uint32_t p = 0; p < metadata.propertyCount; ++p)
            {
                if (!FindType(metadata.properties[p].valueTypeId))
                {
                    lastError_ =
                        ReflectionRegistryError::UnknownPropertyValueType;
                    return false;
                }
            }

            if (metadata.containerMetadata)
            {
                if (!FindType(metadata.containerMetadata->elementTypeId))
                {
                    lastError_ =
                        ReflectionRegistryError::UnknownContainerElementType;
                    return false;
                }
            }

            if (metadata.kind == TypeKind::Enum)
            {
                const TypeMetadata* underlying =
                    FindType(metadata.enumMetadata->underlyingTypeId);
                if (!underlying)
                {
                    lastError_ =
                        ReflectionRegistryError::UnknownEnumUnderlyingType;
                    return false;
                }
                if (underlying->kind != TypeKind::SignedInteger
                    && underlying->kind != TypeKind::UnsignedInteger)
                {
                    lastError_ =
                        ReflectionRegistryError::InvalidEnumUnderlyingType;
                    return false;
                }
            }
        }

        impl_->state = ReflectionRegistryState::Frozen;
        lastError_ = ReflectionRegistryError::None;
        return true;
    }

    ReflectionRegistryState ReflectionRegistry::State() const noexcept
    {
        return impl_ ? impl_->state : ReflectionRegistryState::Uninitialized;
    }

    ReflectionRegistryError ReflectionRegistry::LastError() const noexcept
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

    const TypeMetadata* ReflectionRegistry::FindType(TypeId typeId) const noexcept
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

    const TypeMetadata* ReflectionRegistry::FindTypeByName(
        const char* canonicalName) const noexcept
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

    const TypeMetadata* ReflectionRegistry::TypeAt(uint32_t index) const noexcept
    {
        if (!impl_ || index >= impl_->count)
            return nullptr;
        return &impl_->entries[index].metadata;
    }

    const PropertyMetadata* ReflectionRegistry::FindProperty(
        TypeId ownerTypeId,
        PropertyId propertyId) const noexcept
    {
        if (!propertyId.IsValid())
            return nullptr;
        const TypeMetadata* type = FindType(ownerTypeId);
        if (!type)
            return nullptr;
        for (uint32_t i = 0; i < type->propertyCount; ++i)
        {
            if (type->properties[i].propertyId == propertyId)
                return &type->properties[i];
        }
        return nullptr;
    }

    const PropertyMetadata* ReflectionRegistry::FindPropertyByName(
        TypeId ownerTypeId,
        const char* canonicalName) const noexcept
    {
        if (!canonicalName || canonicalName[0] == '\0')
            return nullptr;
        const TypeMetadata* type = FindType(ownerTypeId);
        if (!type)
            return nullptr;
        for (uint32_t i = 0; i < type->propertyCount; ++i)
        {
            if (std::strcmp(
                    type->properties[i].canonicalName,
                    canonicalName) == 0)
            {
                return &type->properties[i];
            }
        }
        return nullptr;
    }

    const EnumValueMetadata* ReflectionRegistry::FindEnumValue(
        TypeId enumTypeId,
        EnumValueId valueId) const noexcept
    {
        if (!valueId.IsValid())
            return nullptr;
        const TypeMetadata* type = FindType(enumTypeId);
        if (!type || type->kind != TypeKind::Enum || !type->enumMetadata)
            return nullptr;
        for (uint32_t i = 0; i < type->enumMetadata->valueCount; ++i)
        {
            if (type->enumMetadata->values[i].valueId == valueId)
                return &type->enumMetadata->values[i];
        }
        return nullptr;
    }

    const EnumValueMetadata* ReflectionRegistry::FindEnumValueByName(
        TypeId enumTypeId,
        const char* canonicalName) const noexcept
    {
        if (!canonicalName || canonicalName[0] == '\0')
            return nullptr;
        const TypeMetadata* type = FindType(enumTypeId);
        if (!type || type->kind != TypeKind::Enum || !type->enumMetadata)
            return nullptr;
        for (uint32_t i = 0; i < type->enumMetadata->valueCount; ++i)
        {
            if (std::strcmp(
                    type->enumMetadata->values[i].canonicalName,
                    canonicalName) == 0)
            {
                return &type->enumMetadata->values[i];
            }
        }
        return nullptr;
    }

    const EnumValueMetadata* ReflectionRegistry::FindEnumValueByRawValue(
        TypeId enumTypeId,
        uint64_t rawValue) const noexcept
    {
        const TypeMetadata* type = FindType(enumTypeId);
        if (!type || type->kind != TypeKind::Enum || !type->enumMetadata)
            return nullptr;
        for (uint32_t i = 0; i < type->enumMetadata->valueCount; ++i)
        {
            if (type->enumMetadata->values[i].rawValue == rawValue)
                return &type->enumMetadata->values[i];
        }
        return nullptr;
    }

    const AttributeMetadata* ReflectionRegistry::FindTypeAttribute(
        TypeId typeId,
        AttributeKind kind) const noexcept
    {
        const TypeMetadata* type = FindType(typeId);
        if (!type)
            return nullptr;
        for (uint32_t i = 0; i < type->attributeCount; ++i)
        {
            if (type->attributes[i].kind == kind)
                return &type->attributes[i];
        }
        return nullptr;
    }

    const AttributeMetadata* ReflectionRegistry::FindPropertyAttribute(
        TypeId ownerTypeId,
        PropertyId propertyId,
        AttributeKind kind) const noexcept
    {
        const PropertyMetadata* property =
            FindProperty(ownerTypeId, propertyId);
        if (!property)
            return nullptr;
        for (uint32_t i = 0; i < property->attributeCount; ++i)
        {
            if (property->attributes[i].kind == kind)
                return &property->attributes[i];
        }
        return nullptr;
    }

    const ContainerMetadata* ReflectionRegistry::FindContainer(
        TypeId typeId) const noexcept
    {
        const TypeMetadata* type = FindType(typeId);
        return type ? type->containerMetadata : nullptr;
    }
}
