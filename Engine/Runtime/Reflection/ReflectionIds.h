#pragma once

#include <cstdint>

namespace noc
{
    // Design choice (not directly from the book):
    // Reflection identity is explicit, stable and independent of registration
    // order, RTTI objects, pointer addresses and byte offsets.
    struct TypeId
    {
        uint64_t value = 0;

        [[nodiscard]] constexpr bool IsValid() const noexcept { return value != 0; }
        [[nodiscard]] static constexpr TypeId Invalid() noexcept { return {}; }
    };

    struct PropertyId
    {
        uint64_t value = 0;

        [[nodiscard]] constexpr bool IsValid() const noexcept { return value != 0; }
        [[nodiscard]] static constexpr PropertyId Invalid() noexcept { return {}; }
    };

    struct FunctionId
    {
        uint64_t value = 0;

        [[nodiscard]] constexpr bool IsValid() const noexcept { return value != 0; }
        [[nodiscard]] static constexpr FunctionId Invalid() noexcept { return {}; }
    };

    [[nodiscard]] constexpr bool operator==(TypeId a, TypeId b) noexcept { return a.value == b.value; }
    [[nodiscard]] constexpr bool operator!=(TypeId a, TypeId b) noexcept { return !(a == b); }
    [[nodiscard]] constexpr bool operator<(TypeId a, TypeId b) noexcept { return a.value < b.value; }

    [[nodiscard]] constexpr bool operator==(PropertyId a, PropertyId b) noexcept { return a.value == b.value; }
    [[nodiscard]] constexpr bool operator!=(PropertyId a, PropertyId b) noexcept { return !(a == b); }
    [[nodiscard]] constexpr bool operator<(PropertyId a, PropertyId b) noexcept { return a.value < b.value; }

    [[nodiscard]] constexpr bool operator==(FunctionId a, FunctionId b) noexcept { return a.value == b.value; }
    [[nodiscard]] constexpr bool operator!=(FunctionId a, FunctionId b) noexcept { return !(a == b); }
    [[nodiscard]] constexpr bool operator<(FunctionId a, FunctionId b) noexcept { return a.value < b.value; }

    // Deterministic FNV-1a helper for schema declarations/tests. Canonical engine
    // types still use named constants so identity changes remain reviewable.
    // Freeze-time duplicate detection remains mandatory because hashing can
    // theoretically collide.
    [[nodiscard]] constexpr uint64_t StableReflectionHash64(const char* text) noexcept
    {
        if (!text || text[0] == '\0')
            return 0;

        uint64_t hash = 14695981039346656037ull;
        for (const unsigned char* p =
                 reinterpret_cast<const unsigned char*>(text);
             *p != 0;
             ++p)
        {
            hash ^= static_cast<uint64_t>(*p);
            hash *= 1099511628211ull;
        }

        return hash == 0 ? 1ull : hash;
    }

    [[nodiscard]] constexpr TypeId MakeTypeId(const char* canonicalName) noexcept
    {
        return TypeId{ StableReflectionHash64(canonicalName) };
    }

    [[nodiscard]] constexpr PropertyId MakePropertyId(const char* canonicalName) noexcept
    {
        return PropertyId{ StableReflectionHash64(canonicalName) };
    }

    [[nodiscard]] constexpr FunctionId MakeFunctionId(const char* canonicalName) noexcept
    {
        return FunctionId{ StableReflectionHash64(canonicalName) };
    }
}
