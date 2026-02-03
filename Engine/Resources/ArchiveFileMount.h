#pragma once

#include "IFileMount.h"

#include <unordered_map>

#include "Platform/Win32/WinFileSystem.h"

#include <string>

namespace noc
{
    // ZIP read-only mount (Phase 3):
    // - Supports "stored" entries (compression method 0) only.
    // - Logs and refuses compressed entries.
    class ArchiveFileMount final : public IFileMount
    {
    public:
        explicit ArchiveFileMount(const char* archivePathUtf8);

        // Must be called once after construction; returns false if archive can't be indexed.
        bool BuildIndex();

        bool OpenRead(std::string_view normalizedRelativeVPath, FileHandle& out) override;
        void Close(FileHandle& h) override;
        size_t Read(FileHandle& h, void* dst, size_t bytes) override;
        uint64_t Size(const FileHandle& h) const override;

    private:
        struct Entry
        {
            uint32_t method = 0;            // 0 = stored
            uint64_t uncompressedSize = 0;
            uint64_t compressedSize = 0;
            uint64_t dataOffset = 0;        // absolute offset in archive to file data
        };

        struct ArchiveBackend
        {
            noc::platform::WinFile file{};
            Entry entry{};
        };

        bool FindEntry(std::string_view normalizedRelativeVPath, Entry& outEntry) const;

        bool ComputeEntryDataOffset(uint64_t localHeaderOffset, uint64_t& outDataOffset) const;

        bool ReadAt(uint64_t offset, void* dst, size_t bytes, size_t& outRead) const;

    private:
        char archivePath_[1024]{};
        mutable noc::platform::WinFile indexFile_{}; // used only during indexing

        std::unordered_map<std::string, Entry> entries_;
    };
}
