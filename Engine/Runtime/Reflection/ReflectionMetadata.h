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
            MakeTypeLifecycleOperations<T>()
        };
    }
}
