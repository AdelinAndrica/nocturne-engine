#pragma once

#include "Runtime/Reflection/ReflectionIds.h"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

namespace noc
{
    enum class TypeKind : uint8_t
    {
        Invalid = 0,
        Bool,
        SignedInteger,
        UnsignedInteger,
        FloatingPoint,
        String,
        Enum,
        Struct,
        Component,
        EntityReference,
        ResourceReference,
        FixedArray,
        DynamicSequence,
        Function,
        Opaque
    };

    enum class TypeFlags : uint32_t
    {
        None = 0,
        EditorVisible = 1u << 0,
        Serializable = 1u << 1,
        ScriptVisible = 1u << 2,
        Deprecated = 1u << 3,
        Transient = 1u << 4
    };

    enum class PropertyFlags : uint32_t
    {
        None = 0,
        ReadOnly = 1u << 0,
        Transient = 1u << 1,
        Serializable = 1u << 2,
        EditorVisible = 1u << 3,
        ScriptVisible = 1u << 4,
        Deprecated = 1u << 5,
        Required = 1u << 6,
        Hidden = 1u << 7,
        ResourceReference = 1u << 8,
        EntityReference = 1u << 9
    };

    enum class FunctionFlags : uint32_t
    {
        None = 0,
        Const = 1u << 0,
        Static = 1u << 1,
        Member = 1u << 2,
        EditorCallable = 1u << 3,
        ScriptVisible = 1u << 4,
        Deprecated = 1u << 5
    };

    [[nodiscard]] constexpr TypeFlags operator|(TypeFlags a, TypeFlags b) noexcept
    {
        return static_cast<TypeFlags>(
            static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    [[nodiscard]] constexpr PropertyFlags operator|(
        PropertyFlags a,
        PropertyFlags b) noexcept
    {
        return static_cast<PropertyFlags>(
            static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    [[nodiscard]] constexpr FunctionFlags operator|(
        FunctionFlags a,
        FunctionFlags b) noexcept
    {
        return static_cast<FunctionFlags>(
            static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    [[nodiscard]] constexpr bool HasFlag(TypeFlags value, TypeFlags flag) noexcept
    {
        return (static_cast<uint32_t>(value) & static_cast<uint32_t>(flag)) != 0;
    }

    [[nodiscard]] constexpr bool HasFlag(
        PropertyFlags value,
        PropertyFlags flag) noexcept
    {
        return (static_cast<uint32_t>(value) & static_cast<uint32_t>(flag)) != 0;
    }

    [[nodiscard]] constexpr bool HasFlag(
        FunctionFlags value,
        FunctionFlags flag) noexcept
    {
        return (static_cast<uint32_t>(value) & static_cast<uint32_t>(flag)) != 0;
    }

    enum class AttributeKind : uint8_t
    {
        Invalid = 0,
        DisplayName,
        Category,
        Tooltip,
        NumericRange,
        NumericStep,
        Units,
        Angle,
        Color,
        Multiline,
        ResourceTypeConstraint,
        EditorWidgetHint,
        SerializationAlias,
        ScriptingAlias,
        ReadOnlyReason
    };

    enum class AttributeValueKind : uint8_t
    {
        None = 0,
        String,
        Number,
        Range,
        TypeId,
        Boolean
    };

    struct AttributeMetadata
    {
        AttributeKind kind = AttributeKind::Invalid;
        AttributeValueKind valueKind = AttributeValueKind::None;

        const char* stringValue = nullptr;
        double numberA = 0.0;
        double numberB = 0.0;
        TypeId typeIdValue{};
        bool boolValue = false;
    };

    [[nodiscard]] constexpr AttributeMetadata MakeStringAttribute(
        AttributeKind kind,
        const char* value) noexcept
    {
        AttributeMetadata result{};
        result.kind = kind;
        result.valueKind = AttributeValueKind::String;
        result.stringValue = value;
        return result;
    }

    [[nodiscard]] constexpr AttributeMetadata MakeNumberAttribute(
        AttributeKind kind,
        double value) noexcept
    {
        AttributeMetadata result{};
        result.kind = kind;
        result.valueKind = AttributeValueKind::Number;
        result.numberA = value;
        return result;
    }

    [[nodiscard]] constexpr AttributeMetadata MakeRangeAttribute(
        AttributeKind kind,
        double minimum,
        double maximum) noexcept
    {
        AttributeMetadata result{};
        result.kind = kind;
        result.valueKind = AttributeValueKind::Range;
        result.numberA = minimum;
        result.numberB = maximum;
        return result;
    }

    [[nodiscard]] constexpr AttributeMetadata MakeTypeIdAttribute(
        AttributeKind kind,
        TypeId typeId) noexcept
    {
        AttributeMetadata result{};
        result.kind = kind;
        result.valueKind = AttributeValueKind::TypeId;
        result.typeIdValue = typeId;
        return result;
    }

    [[nodiscard]] constexpr AttributeMetadata MakeBooleanAttribute(
        AttributeKind kind,
        bool value = true) noexcept
    {
        AttributeMetadata result{};
        result.kind = kind;
        result.valueKind = AttributeValueKind::Boolean;
        result.boolValue = value;
        return result;
    }

    struct TypeLifecycleOperations
    {
        void (*defaultConstruct)(void* destination) = nullptr;
        void (*destruct)(void* object) = nullptr;
        void (*copyConstruct)(void* destination, const void* source) = nullptr;
        void (*moveConstruct)(void* destination, void* source) = nullptr;
        void (*copyAssign)(void* destination, const void* source) = nullptr;
        void (*moveAssign)(void* destination, void* source) = nullptr;
        bool (*equals)(const void* a, const void* b) = nullptr;
        void (*reset)(void* object) = nullptr;
    };

    struct PropertyAccessContext
    {
        const void* object = nullptr;
        void* mutableObject = nullptr;

        // Design choice (not directly from the book): semantic property
        // adapters may carry an opaque runtime context (for example World +
        // Entity) without making reflection depend on ECS/editor types.
        void* userContext = nullptr;
    };

    using PropertyReadFn =
        bool (*)(const PropertyAccessContext& context, void* destination);
    using PropertyWriteFn =
        bool (*)(PropertyAccessContext& context, const void* source);
    using PropertyConstAddressFn =
        const void* (*)(const PropertyAccessContext& context);
    using PropertyMutableAddressFn =
        void* (*)(PropertyAccessContext& context);
    using PropertyValidateFn =
        bool (*)(const PropertyAccessContext& context, const void* candidate);
    using PropertyDefaultValueFn =
        bool (*)(void* destination);

    struct PropertyMetadata
    {
        PropertyId propertyId{};
        const char* canonicalName = nullptr;
        TypeId ownerTypeId{};
        TypeId valueTypeId{};
        PropertyFlags flags = PropertyFlags::None;

        // read/write use already-constructed value storage.
        PropertyReadFn read = nullptr;
        PropertyWriteFn write = nullptr;

        // Optional safe fast path for plain data only.
        PropertyConstAddressFn constAddress = nullptr;
        PropertyMutableAddressFn mutableAddress = nullptr;

        PropertyValidateFn validate = nullptr;
        PropertyDefaultValueFn defaultValue = nullptr;

        const AttributeMetadata* attributes = nullptr;
        uint32_t attributeCount = 0;
    };

    struct EnumValueMetadata
    {
        EnumValueId valueId{};
        const char* canonicalName = nullptr;

        // Raw underlying bits. Interpretation is determined by the enum's
        // reflected signed/unsigned underlying type.
        uint64_t rawValue = 0;
    };

    struct EnumMetadata
    {
        TypeId underlyingTypeId{};
        const EnumValueMetadata* values = nullptr;
        uint32_t valueCount = 0;
        bool isFlags = false;
    };

    template <typename Enum>
    [[nodiscard]] constexpr EnumValueMetadata MakeEnumValueMetadata(
        EnumValueId valueId,
        const char* canonicalName,
        Enum value) noexcept
    {
        static_assert(std::is_enum_v<Enum>, "Enum reflection requires an enum type.");

        using Underlying = std::underlying_type_t<Enum>;
        using Unsigned = std::make_unsigned_t<Underlying>;

        return EnumValueMetadata{
            valueId,
            canonicalName,
            static_cast<uint64_t>(
                static_cast<Unsigned>(
                    static_cast<Underlying>(value)))
        };
    }

    namespace reflection_detail
    {
        template <typename T>
        concept EqualityComparable =
            requires(const T& a, const T& b)
            {
                { a == b } -> std::convertible_to<bool>;
            };

        template <typename T>
        void DefaultConstruct(void* destination)
        {
            new (destination) T();
        }

        template <typename T>
        void Destruct(void* object)
        {
            static_cast<T*>(object)->~T();
        }

        template <typename T>
        void CopyConstruct(void* destination, const void* source)
        {
            new (destination) T(*static_cast<const T*>(source));
        }

        template <typename T>
        void MoveConstruct(void* destination, void* source)
        {
            new (destination) T(std::move(*static_cast<T*>(source)));
        }

        template <typename T>
        void CopyAssign(void* destination, const void* source)
        {
            *static_cast<T*>(destination) = *static_cast<const T*>(source);
        }

        template <typename T>
        void MoveAssign(void* destination, void* source)
        {
            *static_cast<T*>(destination) = std::move(*static_cast<T*>(source));
        }

        template <typename T>
        bool Equals(const void* a, const void* b)
        {
            return *static_cast<const T*>(a) == *static_cast<const T*>(b);
        }

        template <typename T>
        void Reset(void* object)
        {
            *static_cast<T*>(object) = T{};
        }

        template <typename Owner, typename Value, Value Owner::*Member>
        bool ReadMember(
            const PropertyAccessContext& context,
            void* destination)
        {
            if (!destination)
                return false;

            const void* sourceObject =
                context.object ? context.object : context.mutableObject;
            if (!sourceObject)
                return false;

            *static_cast<Value*>(destination) =
                static_cast<const Owner*>(sourceObject)->*Member;
            return true;
        }

        template <typename Owner, typename Value, Value Owner::*Member>
        bool WriteMember(
            PropertyAccessContext& context,
            const void* source)
        {
            if (!context.mutableObject || !source)
                return false;

            static_cast<Owner*>(context.mutableObject)->*Member =
                *static_cast<const Value*>(source);
            return true;
        }

        template <typename Owner, typename Value, Value Owner::*Member>
        const void* ConstAddressMember(
            const PropertyAccessContext& context)
        {
            const void* sourceObject =
                context.object ? context.object : context.mutableObject;
            if (!sourceObject)
                return nullptr;

            return &(static_cast<const Owner*>(sourceObject)->*Member);
        }

        template <typename Owner, typename Value, Value Owner::*Member>
        void* MutableAddressMember(PropertyAccessContext& context)
        {
            if (!context.mutableObject)
                return nullptr;

            return &(static_cast<Owner*>(context.mutableObject)->*Member);
        }
    }

    // Design choice (not directly from the book): lifecycle adapters are
    // explicit function pointers stored in immutable metadata. This keeps
    // non-trivial values safe without exposing STL ownership or std::any.
    template <typename T>
    [[nodiscard]] constexpr TypeLifecycleOperations
    MakeTypeLifecycleOperations() noexcept
    {
        static_assert(std::is_destructible_v<T>,
            "Reflected types must be destructible.");

        TypeLifecycleOperations operations{};

        if constexpr (std::is_default_constructible_v<T>)
            operations.defaultConstruct =
                &reflection_detail::DefaultConstruct<T>;

        operations.destruct = &reflection_detail::Destruct<T>;

        if constexpr (std::is_copy_constructible_v<T>)
            operations.copyConstruct =
                &reflection_detail::CopyConstruct<T>;

        if constexpr (std::is_move_constructible_v<T>)
            operations.moveConstruct =
                &reflection_detail::MoveConstruct<T>;

        if constexpr (std::is_copy_assignable_v<T>)
            operations.copyAssign =
                &reflection_detail::CopyAssign<T>;

        if constexpr (std::is_move_assignable_v<T>)
            operations.moveAssign =
                &reflection_detail::MoveAssign<T>;

        if constexpr (reflection_detail::EqualityComparable<T>)
            operations.equals =
                &reflection_detail::Equals<T>;

        if constexpr (
            std::is_default_constructible_v<T>
            && std::is_move_assignable_v<T>)
        {
            operations.reset =
                &reflection_detail::Reset<T>;
        }

        return operations;
    }

    struct TypeMetadata
    {
        TypeId typeId{};
        const char* canonicalName = nullptr;
        TypeKind kind = TypeKind::Invalid;
        uint32_t version = 0;
        uint32_t size = 0;
        uint32_t alignment = 0;
        TypeFlags flags = TypeFlags::None;
        TypeLifecycleOperations lifecycle{};

        const PropertyMetadata* properties = nullptr;
        uint32_t propertyCount = 0;

        const AttributeMetadata* attributes = nullptr;
        uint32_t attributeCount = 0;

        const EnumMetadata* enumMetadata = nullptr;
    };

    template <typename T>
    [[nodiscard]] constexpr TypeMetadata MakeTypeMetadata(
        TypeId typeId,
        const char* canonicalName,
        TypeKind kind,
        uint32_t version,
        TypeFlags flags = TypeFlags::None) noexcept
    {
        return TypeMetadata{
            typeId,
            canonicalName,
            kind,
            version,
            static_cast<uint32_t>(sizeof(T)),
            static_cast<uint32_t>(alignof(T)),
            flags,
            MakeTypeLifecycleOperations<T>(),
            nullptr,
            0,
            nullptr,
            0,
            nullptr
        };
    }

    // Plain member-property helper. Semantic properties use explicit callbacks
    // rather than this direct-address adapter.
    template <typename Owner, typename Value, Value Owner::*Member>
    [[nodiscard]] constexpr PropertyMetadata MakeMemberPropertyMetadata(
        PropertyId propertyId,
        const char* canonicalName,
        TypeId ownerTypeId,
        TypeId valueTypeId,
        PropertyFlags flags = PropertyFlags::None) noexcept
    {
        static_assert(
            std::is_copy_assignable_v<Value>,
            "Plain reflected member values must be copy assignable.");

        const bool readOnly = HasFlag(flags, PropertyFlags::ReadOnly);

        return PropertyMetadata{
            propertyId,
            canonicalName,
            ownerTypeId,
            valueTypeId,
            flags,
            &reflection_detail::ReadMember<Owner, Value, Member>,
            readOnly
                ? nullptr
                : &reflection_detail::WriteMember<Owner, Value, Member>,
            &reflection_detail::ConstAddressMember<Owner, Value, Member>,
            readOnly
                ? nullptr
                : &reflection_detail::MutableAddressMember<Owner, Value, Member>,
            nullptr,
            nullptr,
            nullptr,
            0
        };
    }
}
