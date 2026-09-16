#pragma once
#include <cstdint>

namespace noc {

    // Deterministic ID for assets. In Phase 11 we derive this from normalized VFS vpath,
    // using a stable hash. (Design choice: hash algo)
    struct AssetId {
        uint64_t value = 0;
        friend bool operator==(const AssetId& a, const AssetId& b) { return a.value == b.value; }
        friend bool operator!=(const AssetId& a, const AssetId& b) { return !(a == b); }
    };

} // namespace noc
