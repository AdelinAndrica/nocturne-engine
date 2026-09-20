#pragma once

#include "Runtime/ComponentType.h"

#include <cstdint>

namespace noc
{
    inline constexpr ComponentTypeId kNameComponentTypeId{ 4 };
    inline constexpr uint32_t kNameComponentVersion = 1;
    inline constexpr const char* kNameComponentCanonicalName =
        "Nocturne.Name";

    /** @brief Fixed byte capacity for NameComponent, including the null terminator. */
    inline constexpr uint32_t kNameComponentCapacity = 64;

    /** @brief Maximum UTF-8 payload bytes available before the null terminator. */
    inline constexpr uint32_t kNameComponentMaxBytes =
        kNameComponentCapacity - 1u;

    /**
     * @brief Fixed-capacity UTF-8 display name used by runtime/editor presentation.
     *
     * NameComponent is intentionally not entity identity. Renaming an entity does not
     * change its EntityHandle.
     *
     * @par Capacity
     * Storage is 64 bytes including the null terminator, leaving at most 63 bytes for
     * UTF-8 payload.
     *
     * @par Why fixed storage
     * This keeps the public component free of STL ownership/allocator state.
     * The exact fixed-capacity policy is a Nocturne design choice.
     *
     * @ingroup world_ecs
     */
    struct NameComponent
    {
        /** Null-terminated UTF-8 display-name storage. */
        char value[kNameComponentCapacity]{};
    };

    /** @brief Returns canonical metadata for NameComponent. */
    [[nodiscard]] constexpr ComponentTypeMetadata NameComponentMetadata() noexcept
    {
        return MakeComponentTypeMetadata<NameComponent>(
            kNameComponentTypeId,
            kNameComponentCanonicalName,
            kNameComponentVersion,
            ComponentTypeFlags::EditorVisible | ComponentTypeFlags::Serializable);
    }
}
