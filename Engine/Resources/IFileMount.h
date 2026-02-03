#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "FileHandle.h"

namespace noc
{
    class IFileMount
    {
    public:
        virtual ~IFileMount() = default;

        // Returns true if this mount can open the file for read.
        virtual bool OpenRead(std::string_view normalizedRelativeVPath, FileHandle& out) = 0;

        virtual void Close(FileHandle& h) = 0;

        // Read from current cursor; advances cursor.
        virtual size_t Read(FileHandle& h, void* dst, size_t bytes) = 0;

        // Returns file size in bytes.
        virtual uint64_t Size(const FileHandle& h) const = 0;
    };
}
