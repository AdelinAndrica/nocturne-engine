#pragma once

#include "Runtime/ComponentType.h"

#include <cstdint>

namespace noc
{
    inline constexpr ComponentTypeId kNameComponentTypeId{ 4 };
    inline constexpr uint32_t kNameComponentVersion = 1;
    inline constexpr const char* kNameComponentCanonicalName =
        "Nocturne.Name";

    // Design choice (not directly from the book): runtime/editor display names
    // are stored inline as UTF-8 bytes to keep the public component free of STL
    // ownership and allocator concerns. Entity identity never depends on name.
    inline constexpr uint32_t kNameComponentCapacity = 64;
    inline constexpr uint32_t kNameComponentMaxBytes =
        kNameComponentCapacity - 1u;

    struct NameComponent
    {
        char value[kNameComponentCapacity]{};
    };

    [[nodiscard]] constexpr ComponentTypeMetadata NameComponentMetadata() noexcept
    {
        return MakeComponentTypeMetadata<NameComponent>(
            kNameComponentTypeId,
            kNameComponentCanonicalName,
            kNameComponentVersion,
            ComponentTypeFlags::EditorVisible | ComponentTypeFlags::Serializable);
    }
}
