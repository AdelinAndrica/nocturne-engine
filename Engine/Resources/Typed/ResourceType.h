#pragma once
#include <cstdint>

namespace noc {

    enum class ResourceType : uint8_t {
        Unknown = 0,
        Binary,
        Text,

        // Phase 11:
        Mesh,
        Texture,
        Material,
    };

} // namespace noc
