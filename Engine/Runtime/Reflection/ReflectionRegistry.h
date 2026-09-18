#pragma once

#include "Runtime/Reflection/ReflectionMetadata.h"

#include <cstdint>

namespace noc
{
    class IAllocator;

    enum class ReflectionRegistryState : uint8_t
    {
        Uninitialized = 0,
        Building,
        Frozen
    };

    enum class ReflectionRegistryError : uint8_t
    {
        None = 0,
        NotInitialized,
        WrongState,
        InvalidMetadata,
        DuplicateTypeId,
        DuplicateCanonicalName,
        DuplicatePropertyId,
        DuplicatePropertyCanonicalName,
        UnknownPropertyValueType,
        DuplicateEnumValueId,
        DuplicateEnumValueCanonicalName,
        DuplicateEnumNumericValue,
        UnknownEnumUnderlyingType,
        InvalidEnumUnderlyingType,
        InvalidAttributeMetadata,
        DuplicateAttributeKind,
        InvalidContainerMetadata,
        UnknownContainerElementType,
        InvalidFunctionMetadata,
        DuplicateFunctionId,
        DuplicateFunctionCanonicalName,
        DuplicateFunctionParameterName,
        UnknownFunctionParameterType,
        UnknownFunctionReturnType,
        AllocationFailure,
        ValidationFailure
    };

    [[nodiscard]] constexpr const char* ReflectionRegistryErrorName(
        ReflectionRegistryError error) noexcept
    {
        switch (error)
        {
        case ReflectionRegistryError::None: return "None";
        case ReflectionRegistryError::NotInitialized: return "NotInitialized";
        case ReflectionRegistryError::WrongState: return "WrongState";
        case ReflectionRegistryError::InvalidMetadata: return "InvalidMetadata";
        case ReflectionRegistryError::DuplicateTypeId: return "DuplicateTypeId";
        case ReflectionRegistryError::DuplicateCanonicalName: return "DuplicateCanonicalName";
        case ReflectionRegistryError::DuplicatePropertyId: return "DuplicatePropertyId";
        case ReflectionRegistryError::DuplicatePropertyCanonicalName: return "DuplicatePropertyCanonicalName";
        case ReflectionRegistryError::UnknownPropertyValueType: return "UnknownPropertyValueType";
        case ReflectionRegistryError::DuplicateEnumValueId: return "DuplicateEnumValueId";
        case ReflectionRegistryError::DuplicateEnumValueCanonicalName: return "DuplicateEnumValueCanonicalName";
        case ReflectionRegistryError::DuplicateEnumNumericValue: return "DuplicateEnumNumericValue";
        case ReflectionRegistryError::UnknownEnumUnderlyingType: return "UnknownEnumUnderlyingType";
        case ReflectionRegistryError::InvalidEnumUnderlyingType: return "InvalidEnumUnderlyingType";
        case ReflectionRegistryError::InvalidAttributeMetadata: return "InvalidAttributeMetadata";
        case ReflectionRegistryError::DuplicateAttributeKind: return "DuplicateAttributeKind";
        case ReflectionRegistryError::InvalidContainerMetadata: return "InvalidContainerMetadata";
        case ReflectionRegistryError::UnknownContainerElementType: return "UnknownContainerElementType";
        case ReflectionRegistryError::InvalidFunctionMetadata: return "InvalidFunctionMetadata";
        case ReflectionRegistryError::DuplicateFunctionId: return "DuplicateFunctionId";
        case ReflectionRegistryError::DuplicateFunctionCanonicalName: return "DuplicateFunctionCanonicalName";
        case ReflectionRegistryError::DuplicateFunctionParameterName: return "DuplicateFunctionParameterName";
        case ReflectionRegistryError::UnknownFunctionParameterType: return "UnknownFunctionParameterType";
        case ReflectionRegistryError::UnknownFunctionReturnType: return "UnknownFunctionReturnType";
        case ReflectionRegistryError::AllocationFailure: return "AllocationFailure";
        case ReflectionRegistryError::ValidationFailure: return "ValidationFailure";
        }
        return "Unknown";
    }

    class ReflectionRegistry
    {
    public:
        ReflectionRegistry() = default;
        ~ReflectionRegistry();

        ReflectionRegistry(const ReflectionRegistry&) = delete;
        ReflectionRegistry& operator=(const ReflectionRegistry&) = delete;
        ReflectionRegistry(ReflectionRegistry&&) = delete;
        ReflectionRegistry& operator=(ReflectionRegistry&&) = delete;

        [[nodiscard]] bool Init(
            IAllocator& allocator,
            uint32_t initialTypeCapacity = 32);
        void Shutdown();

        // Building-only. Registry owns copied type/property descriptors and
        // strings, so temporary registration arrays are safe.
        [[nodiscard]] bool RegisterType(const TypeMetadata& metadata);

        // Cross-type references are validated here so registration order does
        // not define schema correctness.
        [[nodiscard]] bool Freeze();

        [[nodiscard]] ReflectionRegistryState State() const noexcept;
        [[nodiscard]] ReflectionRegistryError LastError() const noexcept;
        [[nodiscard]] bool IsFrozen() const noexcept;
        [[nodiscard]] uint32_t TypeCount() const noexcept;

        [[nodiscard]] const TypeMetadata* FindType(TypeId typeId) const noexcept;
        [[nodiscard]] const TypeMetadata* FindTypeByName(
            const char* canonicalName) const noexcept;
        [[nodiscard]] const TypeMetadata* TypeAt(uint32_t index) const noexcept;

        [[nodiscard]] const PropertyMetadata* FindProperty(
            TypeId ownerTypeId,
            PropertyId propertyId) const noexcept;
        [[nodiscard]] const PropertyMetadata* FindPropertyByName(
            TypeId ownerTypeId,
            const char* canonicalName) const noexcept;

        [[nodiscard]] const EnumValueMetadata* FindEnumValue(
            TypeId enumTypeId,
            EnumValueId valueId) const noexcept;
        [[nodiscard]] const EnumValueMetadata* FindEnumValueByName(
            TypeId enumTypeId,
            const char* canonicalName) const noexcept;
        [[nodiscard]] const EnumValueMetadata* FindEnumValueByRawValue(
            TypeId enumTypeId,
            uint64_t rawValue) const noexcept;

        [[nodiscard]] const AttributeMetadata* FindTypeAttribute(
            TypeId typeId,
            AttributeKind kind) const noexcept;
        [[nodiscard]] const AttributeMetadata* FindPropertyAttribute(
            TypeId ownerTypeId,
            PropertyId propertyId,
            AttributeKind kind) const noexcept;

        [[nodiscard]] const ContainerMetadata* FindContainer(
            TypeId typeId) const noexcept;

        [[nodiscard]] const FunctionMetadata* FindFunction(
            TypeId ownerTypeId,
            FunctionId functionId) const noexcept;
        [[nodiscard]] const FunctionMetadata* FindFunctionByName(
            TypeId ownerTypeId,
            const char* canonicalName) const noexcept;

    private:
        struct Impl;
        Impl* impl_ = nullptr;
        ReflectionRegistryError lastError_ =
            ReflectionRegistryError::None;
    };
}
