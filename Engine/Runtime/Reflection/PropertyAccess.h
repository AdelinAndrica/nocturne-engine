#pragma once

#include "Runtime/Reflection/ReflectedValue.h"
#include "Runtime/Reflection/ReflectionRegistry.h"

namespace noc
{
    enum class PropertyAccessStatus : uint8_t
    {
        Success = 0,
        InvalidProperty,
        UnknownValueType,
        DestinationAlreadyInitialized,
        CannotConstructValue,
        ReadFailed,
        InvalidSource,
        TypeMismatch,
        ReadOnly,
        ValidationFailed,
        WriteFailed
    };

    [[nodiscard]] constexpr const char* PropertyAccessStatusName(
        PropertyAccessStatus status) noexcept
    {
        switch (status)
        {
        case PropertyAccessStatus::Success: return "Success";
        case PropertyAccessStatus::InvalidProperty: return "InvalidProperty";
        case PropertyAccessStatus::UnknownValueType: return "UnknownValueType";
        case PropertyAccessStatus::DestinationAlreadyInitialized: return "DestinationAlreadyInitialized";
        case PropertyAccessStatus::CannotConstructValue: return "CannotConstructValue";
        case PropertyAccessStatus::ReadFailed: return "ReadFailed";
        case PropertyAccessStatus::InvalidSource: return "InvalidSource";
        case PropertyAccessStatus::TypeMismatch: return "TypeMismatch";
        case PropertyAccessStatus::ReadOnly: return "ReadOnly";
        case PropertyAccessStatus::ValidationFailed: return "ValidationFailed";
        case PropertyAccessStatus::WriteFailed: return "WriteFailed";
        }
        return "Unknown";
    }

    // Generic read path used by future Inspector/commands/serialization staging.
    // Direct address is only used when a property explicitly exposes that safe
    // fast path; semantic properties remain callback-driven.
    [[nodiscard]] inline PropertyAccessStatus ReadPropertyValue(
        const ReflectionRegistry& registry,
        const PropertyMetadata& property,
        const PropertyAccessContext& context,
        IAllocator& allocator,
        OwnedReflectedValue& destination)
    {
        if (!property.propertyId.IsValid() || !property.read)
            return PropertyAccessStatus::InvalidProperty;

        if (destination.IsValid())
            return PropertyAccessStatus::DestinationAlreadyInitialized;

        const TypeMetadata* valueType =
            registry.FindType(property.valueTypeId);
        if (!valueType)
            return PropertyAccessStatus::UnknownValueType;

        if (property.constAddress)
        {
            const void* address = property.constAddress(context);
            if (!address)
                return PropertyAccessStatus::ReadFailed;

            return destination.InitCopy(
                    allocator,
                    *valueType,
                    address)
                ? PropertyAccessStatus::Success
                : PropertyAccessStatus::CannotConstructValue;
        }

        if (!destination.InitDefault(allocator, *valueType))
            return PropertyAccessStatus::CannotConstructValue;

        if (!property.read(context, destination.Data()))
        {
            destination.Clear();
            return PropertyAccessStatus::ReadFailed;
        }

        return PropertyAccessStatus::Success;
    }

    // Generic write path deliberately funnels through validation + semantic
    // setter callbacks. It never falls back to mutableAddress for authoring.
    [[nodiscard]] inline PropertyAccessStatus WritePropertyValue(
        const PropertyMetadata& property,
        PropertyAccessContext& context,
        ReflectedConstValueView source)
    {
        if (!property.propertyId.IsValid())
            return PropertyAccessStatus::InvalidProperty;

        if (!source.IsValid())
            return PropertyAccessStatus::InvalidSource;

        if (source.typeId != property.valueTypeId)
            return PropertyAccessStatus::TypeMismatch;

        if (HasFlag(property.flags, PropertyFlags::ReadOnly)
            || !property.write)
        {
            return PropertyAccessStatus::ReadOnly;
        }

        if (property.validate
            && !property.validate(context, source.data))
        {
            return PropertyAccessStatus::ValidationFailed;
        }

        return property.write(context, source.data)
            ? PropertyAccessStatus::Success
            : PropertyAccessStatus::WriteFailed;
    }

    [[nodiscard]] inline PropertyAccessStatus WritePropertyValue(
        const PropertyMetadata& property,
        PropertyAccessContext& context,
        const OwnedReflectedValue& source)
    {
        return WritePropertyValue(
            property,
            context,
            source.ConstView());
    }
}
