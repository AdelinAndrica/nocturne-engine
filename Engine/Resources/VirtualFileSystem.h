#pragma once

#include "FileHandle.h"
#include "IFileMount.h"

#include "Core/Memory/Allocator.h" // <-- for noc::IAllocator

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace noc
{
    class IFileMount;

    class VirtualFileSystem
    {
    public:
        VirtualFileSystem() = default;

        // out-of-line destructor (defined in .cpp where IFileMount is complete)
        ~VirtualFileSystem();

        bool MountLooseDirectory(const char* physicalRootUtf8);
        bool MountArchive(const char* archivePathUtf8);

        FileHandle OpenRead(std::string_view virtualPath);
        void Close(FileHandle& h);

        size_t Read(FileHandle& h, void* dst, size_t bytes);
        uint64_t Size(const FileHandle& h) const;

        size_t MountCount() const { return mounts_.size(); }

        // ------------------------------------------------------------
        // Convenience APIs (Phase 3 completion)
        // ------------------------------------------------------------

        // Allocates and returns a buffer containing the full file contents.
        // Caller owns the buffer and must free it via alloc.Deallocate().
        // Returns nullptr on failure. outSize is set to 0 on failure.
        uint8_t* ReadAllBytes(const char* virtualPath, size_t& outSize, IAllocator& alloc);

        // Allocates and returns a null-terminated UTF-8 text buffer.
        // Caller owns the buffer and must free it via alloc.Deallocate().
        // Returns true on success, false on failure. outText is nullptr on failure.
        bool ReadAllText(const char* virtualPath, IAllocator& alloc, char*& outText);

    private:
        std::vector<std::unique_ptr<IFileMount>> mounts_;
    };
}
