#pragma once

#include <string_view>

namespace noc::vpath
{
    // Returns true if p is a legal virtual path:
    // - not empty
    // - uses '/' only (no '\')
    // - no drive letters
    // - no leading "//"
    // - no ".." segments
    // - no absolute OS paths
    bool IsValid(std::string_view p);

    // Normalizes a path into outBuf:
    // - converts '\' to '/'
    // - removes leading '/'
    // - collapses consecutive '/'
    // - rejects ".." traversal (returns false)
    //
    // Output is null-terminated on success.
    bool NormalizeToRelative(char* outBuf, size_t outBufBytes, std::string_view p);
}
