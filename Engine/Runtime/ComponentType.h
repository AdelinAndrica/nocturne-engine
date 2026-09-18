#pragma once

#include <cstddef>
#include <cstdint>

namespace noc
{
    // Stable runtime component type identity.
    //
    // Design choice (not directly from the book): type IDs are explicit numeric
    // constants declared by component types. They are never assigned from
    // registration order, RTTI addresses, pointers, or storage addresses.
    struct ComponentTypeId
    {
        uint32_t value = 0;

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return value != 0;
        }

        [[nodiscard]] static constexpr ComponentTypeId Invalid() noexcept
        {
            return {};
        }
    };

    [[nodiscard]] constexpr bool operator==(ComponentTypeId a, ComponentTypeId b) noexcept
    {
        return a.value == b.value;
    }

    [[nodiscard]] constexpr bool operator!=(ComponentTypeId a, ComponentTypeId b) noexcept
    {
        return !(a == b);
    }

    [[nodiscard]] constexpr bool operator<(ComponentTypeId a, ComponentTypeId b) noexcept
    {
        return a.value < b.value;
    }

    enum class ComponentTypeFlags : uint32_t
    {
        None = 0,
        EditorVisible = 1u << 0,
        Serializable = 1u << 1
    };

    [[nodiscard]] constexpr ComponentTypeFlags operator|(
        ComponentTypeFlags a,
        ComponentTypeFlags b) noexcept
    {
        return static_cast<ComponentTypeFlags>(
            static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    [[nodiscard]] constexpr bool HasFlag(
        ComponentTypeFlags value,
        ComponentTypeFlags flag) noexcept
    {
        return (static_cast<uint32_t>(value) & static_cast<uint32_t>(flag)) != 0;
    }

    // Legacy Phase 15 component metadata view.
    //
    // Phase 16 design choice (not directly from the book): ComponentRegistry
    // no longer owns schema or canonical-name storage. These values are derived
    // from the engine-wide frozen ReflectionRegistry and canonicalName is valid
    // for ReflectionRegistry lifetime.
    struct ComponentTypeMetadata
    {
        ComponentTypeId typeId{};
        const char* canonicalName = nullptr;
        uint32_t version = 0;
        uint32_t size = 0;
        uint32_t alignment = 0;
        ComponentTypeFlags flags = ComponentTypeFlags::None;
    };

    template <typename T>
    [[nodiscard]] constexpr ComponentTypeMetadata MakeComponentTypeMetadata(
        ComponentTypeId typeId,
        const char* canonicalName,
        uint32_t version,
        ComponentTypeFlags flags = ComponentTypeFlags::None) noexcept
    {
        static_assert(sizeof(T) <= 0xFFFFFFFFull,
            "Component size exceeds ComponentTypeMetadata::size.");
        static_assert(alignof(T) <= 0xFFFFFFFFull,
            "Component alignment exceeds ComponentTypeMetadata::alignment.");

        return ComponentTypeMetadata{
            typeId,
            canonicalName,
            version,
            static_cast<uint32_t>(sizeof(T)),
            static_cast<uint32_t>(alignof(T)),
            flags
        };
    }
}
