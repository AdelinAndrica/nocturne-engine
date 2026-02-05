#pragma once
#include <cstdint>

namespace noc {

    enum class ResourceType : uint8_t {
        Unknown = 0,
        Binary,
        Text,
        // Future:
        // Json,
        // Image,
        // Mesh,
    };

} // namespace noc