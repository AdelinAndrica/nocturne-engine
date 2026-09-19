#include "Runtime/Reflection/ReflectionRegistry.h"

#include "Core/Memory/Allocator.h"

#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <new>

namespace noc
{
    namespace
    {
        [[nodiscard]] const char* TypeKindName(
            TypeKind kind) noexcept
        {
            switch (kind)
            {
            case TypeKind::Invalid: return "Invalid";
            case TypeKind::Bool: return "Bool";
            case TypeKind::SignedInteger: return "SignedInteger";
            case TypeKind::UnsignedInteger: return "UnsignedInteger";
            case TypeKind::FloatingPoint: return "FloatingPoint";
            case TypeKind::String: return "String";
            case TypeKind::Enum: return "Enum";
            case TypeKind::Struct: return "Struct";
            case TypeKind::Component: return "Component";
            case TypeKind::EntityReference: return "EntityReference";
            case TypeKind::ResourceReference: return "ResourceReference";
            case TypeKind::FixedArray: return "FixedArray";
            case TypeKind::DynamicSequence: return "DynamicSequence";
            case TypeKind::Function: return "Function";
            case TypeKind::Opaque: return "Opaque";
            }
            return "Unknown";
        }

        [[nodiscard]] const char* AttributeKindName(
            AttributeKind kind) noexcept
        {
            switch (kind)
            {
            case AttributeKind::Invalid: return "Invalid";
            case AttributeKind::DisplayName: return "DisplayName";
            case AttributeKind::Category: return "Category";
            case AttributeKind::Tooltip: return "Tooltip";
            case AttributeKind::NumericRange: return "NumericRange";
            case AttributeKind::NumericStep: return "NumericStep";
            case AttributeKind::Units: return "Units";
            case AttributeKind::Angle: return "Angle";
            case AttributeKind::Color: return "Color";
            case AttributeKind::Multiline: return "Multiline";
            case AttributeKind::ResourceTypeConstraint: return "ResourceTypeConstraint";
            case AttributeKind::EditorWidgetHint: return "EditorWidgetHint";
            case AttributeKind::SerializationAlias: return "SerializationAlias";
            case AttributeKind::ScriptingAlias: return "ScriptingAlias";
            case AttributeKind::ReadOnlyReason: return "ReadOnlyReason";
            }
            return "Unknown";
        }

        [[nodiscard]] const char* AttributeValueKindName(
            AttributeValueKind kind) noexcept
        {
            switch (kind)
            {
            case AttributeValueKind::None: return "None";
            case AttributeValueKind::String: return "String";
            case AttributeValueKind::Number: return "Number";
            case AttributeValueKind::Range: return "Range";
            case AttributeValueKind::TypeId: return "TypeId";
            case AttributeValueKind::Boolean: return "Boolean";
            }
            return "Unknown";
        }

        [[nodiscard]] bool EmitSchemaDumpLine(
            ReflectionSchemaDumpWriteFn writer,
            void* userData,
            const char* format,
            ...) noexcept
        {
            if (!writer || !format)
                return false;

            char line[2048]{};

            va_list arguments;
            va_start(arguments, format);
            const int written =
                std::vsnprintf(
                    line,
                    sizeof(line),
                    format,
                    arguments);
            va_end(arguments);

            if (written < 0
                || static_cast<std::size_t>(written)
                    >= sizeof(line))
            {
                return false;
            }

            return writer(userData, line);
        }

        [[nodiscard]] bool EmitAttributeDumpLine(
            ReflectionSchemaDumpWriteFn writer,
            void* userData,
            const char* scope,
            const char* ownerName,
            const char* propertyName,
            const AttributeMetadata& attribute) noexcept
        {
            const char* safeScope =
                scope ? scope : "unknown";
            const char* safeOwner =
                ownerName ? ownerName : "<null>";
            const char* safeProperty =
                propertyName ? propertyName : "-";

            switch (attribute.valueKind)
            {
            case AttributeValueKind::String:
                return EmitSchemaDumpLine(
                    writer,
                    userData,
                    "attribute scope=%s owner=%s property=%s kind=%s value_kind=%s value=%s",
                    safeScope,
                    safeOwner,
                    safeProperty,
                    AttributeKindName(attribute.kind),
                    AttributeValueKindName(attribute.valueKind),
                    attribute.stringValue
                        ? attribute.stringValue
                        : "<null>");

            case AttributeValueKind::Number:
                return EmitSchemaDumpLine(
                    writer,
                    userData,
                    "attribute scope=%s owner=%s property=%s kind=%s value_kind=%s value=%.17g",
                    safeScope,
                    safeOwner,
                    safeProperty,
                    AttributeKindName(attribute.kind),
                    AttributeValueKindName(attribute.valueKind),
                    attribute.numberA);

            case AttributeValueKind::Range:
                return EmitSchemaDumpLine(
                    writer,
                    userData,
                    "attribute scope=%s owner=%s property=%s kind=%s value_kind=%s min=%.17g max=%.17g",
                    safeScope,
                    safeOwner,
                    safeProperty,
                    AttributeKindName(attribute.kind),
                    AttributeValueKindName(attribute.valueKind),
                    attribute.numberA,
                    attribute.numberB);

            case AttributeValueKind::TypeId:
                return EmitSchemaDumpLine(
                    writer,
                    userData,
                    "attribute scope=%s owner=%s property=%s kind=%s value_kind=%s type_id=%llu",
                    safeScope,
                    safeOwner,
                    safeProperty,
                    AttributeKindName(attribute.kind),
                    AttributeValueKindName(attribute.valueKind),
                    static_cast<unsigned long long>(
                        attribute.typeIdValue.value));

            case AttributeValueKind::Boolean:
                return EmitSchemaDumpLine(
                    writer,
                    userData,
                    "attribute scope=%s owner=%s property=%s kind=%s value_kind=%s value=%s",
                    safeScope,
                    safeOwner,
                    safeProperty,
                    AttributeKindName(attribute.kind),
                    AttributeValueKindName(attribute.valueKind),
                    attribute.boolValue ? "true" : "false");

            case AttributeValueKind::None:
                break;
            }

            return EmitSchemaDumpLine(
                writer,
                userData,
                "attribute scope=%s owner=%s property=%s kind=%s value_kind=%s",
                safeScope,
                safeOwner,
                safeProperty,
                AttributeKindName(attribute.kind),
                AttributeValueKindName(attribute.valueKind));
        }

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

        [[nodiscard]] ReflectionRegistryError ValidateFunctions(
            const TypeMetadata& metadata) noexcept
        {
            if (metadata.functionCount == 0)
            {
                return metadata.functions == nullptr
                    ? ReflectionRegistryError::None
                    : ReflectionRegistryError::InvalidFunctionMetadata;
            }

            if (!metadata.functions)
                return ReflectionRegistryError::InvalidFunctionMetadata;

            for (uint32_t i = 0; i < metadata.functionCount; ++i)
            {
                const FunctionMetadata& function = metadata.functions[i];
                const bool isStatic =
                    HasFlag(function.flags, FunctionFlags::Static);
                const bool isMember =
                    HasFlag(function.flags, FunctionFlags::Member);

                if (!function.functionId.IsValid()
                    || !function.canonicalName
                    || function.canonicalName[0] == '\0'
                    || function.ownerTypeId != metadata.typeId
                    || !function.invoke
                    || isStatic == isMember
                    || (isStatic
                        && HasFlag(function.flags, FunctionFlags::Const)))
                {
                    return ReflectionRegistryError::InvalidFunctionMetadata;
                }

                if (function.parameterCount == 0)
                {
                    if (function.parameters)
                        return ReflectionRegistryError::InvalidFunctionMetadata;
                }
                else
                {
                    if (!function.parameters)
                        return ReflectionRegistryError::InvalidFunctionMetadata;

                    for (uint32_t p = 0; p < function.parameterCount; ++p)
                    {
                        const FunctionParameterMetadata& parameter =
                            function.parameters[p];
                        if (!parameter.canonicalName
                            || parameter.canonicalName[0] == '\0'
                            || !parameter.typeId.IsValid())
                        {
                            return ReflectionRegistryError::InvalidFunctionMetadata;
                        }

                        for (uint32_t q = 0; q < p; ++q)
                        {
                            if (std::strcmp(
                                    function.parameters[q].canonicalName,
                                    parameter.canonicalName) == 0)
                            {
                                return ReflectionRegistryError::
                                    DuplicateFunctionParameterName;
                            }
                        }
                    }
                }

                for (uint32_t j = 0; j < i; ++j)
                {
                    if (metadata.functions[j].functionId == function.functionId)
                        return ReflectionRegistryError::DuplicateFunctionId;
                    if (std::strcmp(
                            metadata.functions[j].canonicalName,
                            function.canonicalName) == 0)
                    {
                        return ReflectionRegistryError::
                            DuplicateFunctionCanonicalName;
                    }
                }
            }

            return ReflectionRegistryError::None;
        }

        [[nodiscard]] ReflectionRegistryError ValidateComponentIntrinsic(
            const TypeMetadata& metadata) noexcept
        {
            if (metadata.kind != TypeKind::Component)
            {
                return metadata.componentMetadata == nullptr
                    ? ReflectionRegistryError::None
                    : ReflectionRegistryError::InvalidComponentMetadata;
            }

            const ComponentMetadata* component = metadata.componentMetadata;
            if (!component
                || !component->has
                || !component->add
                || !component->remove
                || !component->getConst)
            {
                return ReflectionRegistryError::InvalidComponentMetadata;
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

            const ReflectionRegistryError functionError =
                ValidateFunctions(metadata);
            if (functionError != ReflectionRegistryError::None)
                return functionError;

            const ReflectionRegistryError enumError =
                ValidateEnumIntrinsic(metadata);
            if (enumError != ReflectionRegistryError::None)
                return enumError;

            const ReflectionRegistryError containerError =
                ValidateContainerIntrinsic(metadata);
            if (containerError != ReflectionRegistryError::None)
                return containerError;

            return ValidateComponentIntrinsic(metadata);
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
            if (metadata.componentMetadata)
            {
                auto* component =
                    const_cast<ComponentMetadata*>(metadata.componentMetadata);
                component->~ComponentMetadata();
                allocator.Deallocate(component);
                metadata.componentMetadata = nullptr;
            }
            if (metadata.functions)
            {
                auto* functions =
                    const_cast<FunctionMetadata*>(metadata.functions);
                for (uint32_t i = 0; i < metadata.functionCount; ++i)
                {
                    if (functions[i].parameters)
                    {
                        auto* parameters =
                            const_cast<FunctionParameterMetadata*>(
                                functions[i].parameters);
                        for (uint32_t p = 0;
                             p < functions[i].parameterCount;
                             ++p)
                        {
                            if (parameters[p].canonicalName)
                            {
                                allocator.Deallocate(
                                    const_cast<char*>(
                                        parameters[p].canonicalName));
                            }
                        }
                        allocator.Deallocate(parameters);
                    }

                    if (functions[i].canonicalName)
                    {
                        allocator.Deallocate(
                            const_cast<char*>(functions[i].canonicalName));
                    }
                }
                allocator.Deallocate(functions);
                metadata.functions = nullptr;
                metadata.functionCount = 0;
            }

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
            destination.functions = nullptr;
            destination.functionCount = 0;
            destination.enumMetadata = nullptr;
            destination.containerMetadata = nullptr;
            destination.componentMetadata = nullptr;

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

            if (source.componentMetadata)
            {
                auto* component = static_cast<ComponentMetadata*>(
                    allocator.Allocate(
                        sizeof(ComponentMetadata),
                        alignof(ComponentMetadata)));
                if (!component)
                {
                    DestroyOwnedMetadata(allocator, destination);
                    return false;
                }

                new (component) ComponentMetadata(*source.componentMetadata);
                destination.componentMetadata = component;
            }

            if (source.functionCount > 0)
            {
                auto* functions = static_cast<FunctionMetadata*>(
                    allocator.Allocate(
                        sizeof(FunctionMetadata) * source.functionCount,
                        alignof(FunctionMetadata)));
                if (!functions)
                {
                    DestroyOwnedMetadata(allocator, destination);
                    return false;
                }

                for (uint32_t i = 0; i < source.functionCount; ++i)
                    new (functions + i) FunctionMetadata{};

                destination.functions = functions;
                destination.functionCount = source.functionCount;

                for (uint32_t i = 0; i < source.functionCount; ++i)
                {
                    functions[i] = source.functions[i];
                    functions[i].canonicalName = nullptr;
                    functions[i].parameters = nullptr;

                    functions[i].canonicalName =
                        CopyString(allocator, source.functions[i].canonicalName);
                    if (!functions[i].canonicalName)
                    {
                        DestroyOwnedMetadata(allocator, destination);
                        return false;
                    }

                    if (source.functions[i].parameterCount > 0)
                    {
                        auto* parameters =
                            static_cast<FunctionParameterMetadata*>(
                                allocator.Allocate(
                                    sizeof(FunctionParameterMetadata)
                                        * source.functions[i].parameterCount,
                                    alignof(FunctionParameterMetadata)));
                        if (!parameters)
                        {
                            DestroyOwnedMetadata(allocator, destination);
                            return false;
                        }

                        for (uint32_t p = 0;
                             p < source.functions[i].parameterCount;
                             ++p)
                        {
                            new (parameters + p)
                                FunctionParameterMetadata(
                                    source.functions[i].parameters[p]);
                            parameters[p].canonicalName = nullptr;
                            parameters[p].canonicalName =
                                CopyString(
                                    allocator,
                                    source.functions[i]
                                        .parameters[p]
                                        .canonicalName);
                            if (!parameters[p].canonicalName)
                            {
                                functions[i].parameters = parameters;
                                DestroyOwnedMetadata(allocator, destination);
                                return false;
                            }
                        }

                        functions[i].parameters = parameters;
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

            for (uint32_t f = 0; f < metadata.functionCount; ++f)
            {
                const FunctionMetadata& function = metadata.functions[f];

                if (function.returnTypeId.IsValid()
                    && !FindType(function.returnTypeId))
                {
                    lastError_ =
                        ReflectionRegistryError::UnknownFunctionReturnType;
                    return false;
                }

                for (uint32_t p = 0; p < function.parameterCount; ++p)
                {
                    if (!FindType(function.parameters[p].typeId))
                    {
                        lastError_ =
                            ReflectionRegistryError::UnknownFunctionParameterType;
                        return false;
                    }
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

    const FunctionMetadata* ReflectionRegistry::FindFunction(
        TypeId ownerTypeId,
        FunctionId functionId) const noexcept
    {
        if (!functionId.IsValid())
            return nullptr;

        const TypeMetadata* type = FindType(ownerTypeId);
        if (!type)
            return nullptr;

        for (uint32_t i = 0; i < type->functionCount; ++i)
        {
            if (type->functions[i].functionId == functionId)
                return &type->functions[i];
        }
        return nullptr;
    }

    uint32_t ReflectionRegistry::ComponentTypeCount() const noexcept
    {
        if (!impl_)
            return 0;

        uint32_t count = 0;
        for (uint32_t i = 0; i < impl_->count; ++i)
        {
            if (impl_->entries[i].metadata.kind == TypeKind::Component)
                ++count;
        }
        return count;
    }

    const TypeMetadata* ReflectionRegistry::ComponentTypeAt(
        uint32_t componentIndex) const noexcept
    {
        if (!impl_)
            return nullptr;

        uint32_t seen = 0;
        for (uint32_t i = 0; i < impl_->count; ++i)
        {
            const TypeMetadata& metadata = impl_->entries[i].metadata;
            if (metadata.kind != TypeKind::Component)
                continue;

            if (seen == componentIndex)
                return &metadata;
            ++seen;
        }

        return nullptr;
    }

    bool ReflectionRegistry::DumpSchema(
        ReflectionSchemaDumpWriteFn writer,
        void* userData) const noexcept
    {
        if (!writer
            || !impl_
            || impl_->state != ReflectionRegistryState::Frozen)
        {
            return false;
        }

        if (!EmitSchemaDumpLine(
                writer,
                userData,
                "registry state=Frozen types=%u components=%u",
                impl_->count,
                ComponentTypeCount()))
        {
            return false;
        }

        for (uint32_t typeIndex = 0;
             typeIndex < impl_->count;
             ++typeIndex)
        {
            const TypeMetadata& type =
                impl_->entries[typeIndex].metadata;

            if (!EmitSchemaDumpLine(
                    writer,
                    userData,
                    "type id=%llu name=%s kind=%s version=%u size=%u alignment=%u flags=0x%08X properties=%u attributes=%u functions=%u",
                    static_cast<unsigned long long>(
                        type.typeId.value),
                    type.canonicalName
                        ? type.canonicalName
                        : "<null>",
                    TypeKindName(type.kind),
                    type.version,
                    type.size,
                    type.alignment,
                    static_cast<unsigned int>(type.flags),
                    type.propertyCount,
                    type.attributeCount,
                    type.functionCount))
            {
                return false;
            }

            for (uint32_t attributeIndex = 0;
                 attributeIndex < type.attributeCount;
                 ++attributeIndex)
            {
                if (!EmitAttributeDumpLine(
                        writer,
                        userData,
                        "type",
                        type.canonicalName,
                        nullptr,
                        type.attributes[attributeIndex]))
                {
                    return false;
                }
            }

            for (uint32_t propertyIndex = 0;
                 propertyIndex < type.propertyCount;
                 ++propertyIndex)
            {
                const PropertyMetadata& property =
                    type.properties[propertyIndex];

                if (!EmitSchemaDumpLine(
                        writer,
                        userData,
                        "property owner=%s id=%llu name=%s value_type=%llu flags=0x%08X attributes=%u read=%u write=%u const_address=%u mutable_address=%u validate=%u default=%u",
                        type.canonicalName,
                        static_cast<unsigned long long>(
                            property.propertyId.value),
                        property.canonicalName,
                        static_cast<unsigned long long>(
                            property.valueTypeId.value),
                        static_cast<unsigned int>(
                            property.flags),
                        property.attributeCount,
                        property.read ? 1u : 0u,
                        property.write ? 1u : 0u,
                        property.constAddress ? 1u : 0u,
                        property.mutableAddress ? 1u : 0u,
                        property.validate ? 1u : 0u,
                        property.defaultValue ? 1u : 0u))
                {
                    return false;
                }

                for (uint32_t attributeIndex = 0;
                     attributeIndex
                        < property.attributeCount;
                     ++attributeIndex)
                {
                    if (!EmitAttributeDumpLine(
                            writer,
                            userData,
                            "property",
                            type.canonicalName,
                            property.canonicalName,
                            property.attributes[
                                attributeIndex]))
                    {
                        return false;
                    }
                }
            }

            if (type.enumMetadata)
            {
                const EnumMetadata& enumMetadata =
                    *type.enumMetadata;

                if (!EmitSchemaDumpLine(
                        writer,
                        userData,
                        "enum owner=%s underlying_type=%llu is_flags=%u values=%u",
                        type.canonicalName,
                        static_cast<unsigned long long>(
                            enumMetadata
                                .underlyingTypeId.value),
                        enumMetadata.isFlags ? 1u : 0u,
                        enumMetadata.valueCount))
                {
                    return false;
                }

                for (uint32_t valueIndex = 0;
                     valueIndex < enumMetadata.valueCount;
                     ++valueIndex)
                {
                    const EnumValueMetadata& value =
                        enumMetadata.values[valueIndex];

                    if (!EmitSchemaDumpLine(
                            writer,
                            userData,
                            "enum_value owner=%s id=%llu name=%s raw=%llu",
                            type.canonicalName,
                            static_cast<unsigned long long>(
                                value.valueId.value),
                            value.canonicalName,
                            static_cast<unsigned long long>(
                                value.rawValue)))
                    {
                        return false;
                    }
                }
            }

            if (type.containerMetadata)
            {
                const ContainerMetadata& container =
                    *type.containerMetadata;

                if (!EmitSchemaDumpLine(
                        writer,
                        userData,
                        "container owner=%s element_type=%llu fixed_count=%u read_only=%u count=%u capacity=%u const_element=%u mutable_element=%u resize=%u insert_default=%u remove=%u",
                        type.canonicalName,
                        static_cast<unsigned long long>(
                            container.elementTypeId.value),
                        container.fixedCount,
                        container.readOnly ? 1u : 0u,
                        container.count ? 1u : 0u,
                        container.capacity ? 1u : 0u,
                        container.constElement ? 1u : 0u,
                        container.mutableElement ? 1u : 0u,
                        container.resize ? 1u : 0u,
                        container.insertDefault ? 1u : 0u,
                        container.remove ? 1u : 0u))
                {
                    return false;
                }
            }

            if (type.componentMetadata)
            {
                const ComponentMetadata& component =
                    *type.componentMetadata;

                if (!EmitSchemaDumpLine(
                        writer,
                        userData,
                        "component owner=%s flags=0x%08X has=%u add=%u remove=%u get_const=%u get_mutable=%u",
                        type.canonicalName,
                        static_cast<unsigned int>(
                            component.flags),
                        component.has ? 1u : 0u,
                        component.add ? 1u : 0u,
                        component.remove ? 1u : 0u,
                        component.getConst ? 1u : 0u,
                        component.getMutable ? 1u : 0u))
                {
                    return false;
                }
            }

            for (uint32_t functionIndex = 0;
                 functionIndex < type.functionCount;
                 ++functionIndex)
            {
                const FunctionMetadata& function =
                    type.functions[functionIndex];

                if (!EmitSchemaDumpLine(
                        writer,
                        userData,
                        "function owner=%s id=%llu name=%s return_type=%llu flags=0x%08X parameters=%u invoke=%u",
                        type.canonicalName,
                        static_cast<unsigned long long>(
                            function.functionId.value),
                        function.canonicalName,
                        static_cast<unsigned long long>(
                            function.returnTypeId.value),
                        static_cast<unsigned int>(
                            function.flags),
                        function.parameterCount,
                        function.invoke ? 1u : 0u))
                {
                    return false;
                }

                for (uint32_t parameterIndex = 0;
                     parameterIndex
                        < function.parameterCount;
                     ++parameterIndex)
                {
                    const FunctionParameterMetadata&
                        parameter =
                            function.parameters[
                                parameterIndex];

                    if (!EmitSchemaDumpLine(
                            writer,
                            userData,
                            "function_parameter owner=%s function=%s index=%u name=%s type=%llu",
                            type.canonicalName,
                            function.canonicalName,
                            parameterIndex,
                            parameter.canonicalName,
                            static_cast<unsigned long long>(
                                parameter.typeId.value)))
                    {
                        return false;
                    }
                }
            }
        }

        return EmitSchemaDumpLine(
            writer,
            userData,
            "registry end");
    }

    const FunctionMetadata* ReflectionRegistry::FindFunctionByName(
        TypeId ownerTypeId,
        const char* canonicalName) const noexcept
    {
        if (!canonicalName || canonicalName[0] == '\0')
            return nullptr;

        const TypeMetadata* type = FindType(ownerTypeId);
        if (!type)
            return nullptr;

        for (uint32_t i = 0; i < type->functionCount; ++i)
        {
            if (std::strcmp(
                    type->functions[i].canonicalName,
                    canonicalName) == 0)
            {
                return &type->functions[i];
            }
        }
        return nullptr;
    }
}
