#pragma once

#include <cstdint>

namespace noc
{
    class IFileMount;

    struct FileHandle
    {
        IFileMount* mount = nullptr;
        void* backend = nullptr;     // mount-defined opaque state (e.g., WinFile*)
        uint64_t sizeBytes = 0;
        uint64_t cursor = 0;
        bool valid = false;
    };
}
