#include "ArchiveFileMount.h"

#include "Platform/Win32/WinFileSystem.h"
#include "Core/Log.h"

#include <cstring>
#include <vector>

namespace noc
{
#pragma pack(push, 1)
    struct ZipEOCD
    {
        uint32_t signature;           // 0x06054b50
        uint16_t diskNumber;
        uint16_t centralDirDisk;
        uint16_t centralDirRecordsOnDisk;
        uint16_t centralDirRecordsTotal;
        uint32_t centralDirSize;
        uint32_t centralDirOffset;
        uint16_t commentLength;
        // comment follows
    };

    struct ZipCentralDirHeader
    {
        uint32_t signature;           // 0x02014b50
        uint16_t versionMadeBy;
        uint16_t versionNeeded;
        uint16_t flags;
        uint16_t compressionMethod;
        uint16_t modTime;
        uint16_t modDate;
        uint32_t crc32;
        uint32_t compressedSize;
        uint32_t uncompressedSize;
        uint16_t fileNameLen;
        uint16_t extraLen;
        uint16_t commentLen;
        uint16_t diskStart;
        uint16_t internalAttrs;
        uint32_t externalAttrs;
        uint32_t localHeaderOffset;
        // fileName + extra + comment follow
    };

    struct ZipLocalHeader
    {
        uint32_t signature;           // 0x04034b50
        uint16_t versionNeeded;
        uint16_t flags;
        uint16_t compressionMethod;
        uint16_t modTime;
        uint16_t modDate;
        uint32_t crc32;
        uint32_t compressedSize;
        uint32_t uncompressedSize;
        uint16_t fileNameLen;
        uint16_t extraLen;
        // fileName + extra follow, then file data
    };
#pragma pack(pop)

    static constexpr uint32_t kEOCDSig = 0x06054b50u;
    static constexpr uint32_t kCDSig = 0x02014b50u;
    static constexpr uint32_t kLHSig = 0x04034b50u;

    static void CopyPath(char* dst, size_t dstBytes, const char* src)
    {
#if defined(_MSC_VER)
        strncpy_s(dst, dstBytes, src ? src : "", _TRUNCATE);
#else
        std::strncpy(dst, src ? src : "", dstBytes - 1);
        dst[dstBytes - 1] = 0;
#endif
        for (size_t i = 0; dst[i] != 0; ++i)
        {
            if (dst[i] == '\\')
                dst[i] = '/';
        }
    }


    ArchiveFileMount::ArchiveFileMount(const char* archivePathUtf8)
    {
        CopyPath(archivePath_, sizeof(archivePath_), archivePathUtf8);
    }

    bool ArchiveFileMount::ReadAt(uint64_t offset, void* dst, size_t bytes, size_t& outRead) const
    {
        outRead = 0;

        // Use a temporary open file handle to avoid shared cursor issues during indexing.
        // For indexing we keep indexFile_ open; for safety we still seek before read.
        if (!indexFile_.handle)
            return false;

        if (!noc::platform::SeekFile(indexFile_, offset))
            return false;

        outRead = noc::platform::ReadFile(indexFile_, dst, bytes);
        return outRead == bytes;
    }

    bool ArchiveFileMount::ComputeEntryDataOffset(uint64_t localHeaderOffset, uint64_t& outDataOffset) const
    {
        ZipLocalHeader lh{};
        size_t read = 0;
        if (!ReadAt(localHeaderOffset, &lh, sizeof(lh), read))
            return false;

        if (lh.signature != kLHSig)
            return false;

        outDataOffset = localHeaderOffset + sizeof(ZipLocalHeader) + lh.fileNameLen + lh.extraLen;
        return true;
    }

    bool ArchiveFileMount::BuildIndex()
    {
        entries_.clear();

        if (!noc::platform::OpenFile(indexFile_, archivePath_, noc::platform::FileOpenMode::ReadOnly))
        {
            NOC_LOG_ERROR("VFS", "Archive open failed: %s", archivePath_);
            return false;
        }

        noc::platform::FileStat st{};
        if (!noc::platform::GetFileStat(indexFile_, st))
        {
            noc::platform::CloseFile(indexFile_);
            NOC_LOG_ERROR("VFS", "Archive stat failed: %s", archivePath_);
            return false;
        }

        const uint64_t fileSize = st.sizeBytes;
        if (fileSize < sizeof(ZipEOCD))
        {
            noc::platform::CloseFile(indexFile_);
            NOC_LOG_ERROR("VFS", "Archive too small: %s", archivePath_);
            return false;
        }

        // EOCD can be up to 64KB comment + record size. We'll search backwards within that window.
        const uint64_t maxSearch = 64ull * 1024ull + sizeof(ZipEOCD);
        const uint64_t searchStart = (fileSize > maxSearch) ? (fileSize - maxSearch) : 0;

        std::vector<uint8_t> tail((size_t)(fileSize - searchStart));
        if (!noc::platform::SeekFile(indexFile_, searchStart))
        {
            noc::platform::CloseFile(indexFile_);
            return false;
        }

        const size_t got = noc::platform::ReadFile(indexFile_, tail.data(), tail.size());
        if (got != tail.size())
        {
            noc::platform::CloseFile(indexFile_);
            return false;
        }

        // Find EOCD signature from end
        int64_t eocdPos = -1;
        for (int64_t i = (int64_t)tail.size() - (int64_t)sizeof(ZipEOCD); i >= 0; --i)
        {
            const uint32_t sig = *(const uint32_t*)(tail.data() + i);
            if (sig == kEOCDSig)
            {
                eocdPos = i;
                break;
            }
        }

        if (eocdPos < 0)
        {
            noc::platform::CloseFile(indexFile_);
            NOC_LOG_ERROR("VFS", "EOCD not found in archive: %s", archivePath_);
            return false;
        }

        ZipEOCD eocd{};
        std::memcpy(&eocd, tail.data() + eocdPos, sizeof(eocd));

        const uint64_t cdOffset = (uint64_t)eocd.centralDirOffset;
        const uint64_t cdSize = (uint64_t)eocd.centralDirSize;

        if (cdOffset + cdSize > fileSize)
        {
            noc::platform::CloseFile(indexFile_);
            NOC_LOG_ERROR("VFS", "Central directory out of range: %s", archivePath_);
            return false;
        }

        // Read entire central directory
        std::vector<uint8_t> cd((size_t)cdSize);
        if (!noc::platform::SeekFile(indexFile_, cdOffset))
        {
            noc::platform::CloseFile(indexFile_);
            return false;
        }

        const size_t cdGot = noc::platform::ReadFile(indexFile_, cd.data(), cd.size());
        if (cdGot != cd.size())
        {
            noc::platform::CloseFile(indexFile_);
            return false;
        }

        size_t cursor = 0;
        while (cursor + sizeof(ZipCentralDirHeader) <= cd.size())
        {
            auto* hdr = (ZipCentralDirHeader*)(cd.data() + cursor);
            if (hdr->signature != kCDSig)
                break;

            cursor += sizeof(ZipCentralDirHeader);

            if (cursor + hdr->fileNameLen + hdr->extraLen + hdr->commentLen > cd.size())
                break;

            const char* namePtr = (const char*)(cd.data() + cursor);
            std::string name(namePtr, namePtr + hdr->fileNameLen);

            cursor += hdr->fileNameLen + hdr->extraLen + hdr->commentLen;

            // Normalize slashes in stored name
            for (char& c : name)
                if (c == '\\') c = '/';

            // Skip directory entries
            if (!name.empty() && name.back() == '/')
                continue;

            Entry e{};
            e.method = hdr->compressionMethod;
            e.compressedSize = hdr->compressedSize;
            e.uncompressedSize = hdr->uncompressedSize;

            uint64_t dataOffset = 0;
            if (!ComputeEntryDataOffset((uint64_t)hdr->localHeaderOffset, dataOffset))
                continue;

            e.dataOffset = dataOffset;

            entries_.emplace(std::move(name), e);
        }

        NOC_LOG_INFO("VFS", "Archive indexed: %s entries=%zu", archivePath_, entries_.size());

        // Keep indexFile_ open only for indexing; close now to avoid holding handles.
        noc::platform::CloseFile(indexFile_);
        return true;
    }

    bool ArchiveFileMount::FindEntry(std::string_view normalizedRelativeVPath, Entry& outEntry) const
    {
        auto it = entries_.find(std::string(normalizedRelativeVPath));
        if (it == entries_.end())
            return false;

        outEntry = it->second;
        return true;
    }

    bool ArchiveFileMount::OpenRead(std::string_view normalizedRelativeVPath, FileHandle& out)
    {
        out = {};

        Entry entry{};
        if (!FindEntry(normalizedRelativeVPath, entry))
            return false;

        // Phase 3: stored entries only (ZIP compression method 0).
        if (entry.method != 0)
        {
            NOC_LOG_ERROR("VFS",
                "Archive entry compression not supported in Phase 3 (method=%u): %.*s",
                entry.method, (int)normalizedRelativeVPath.size(), normalizedRelativeVPath.data());
            return false;
        }

        // Sanity check for stored entries.
        if (entry.compressedSize != entry.uncompressedSize)
        {
            NOC_LOG_ERROR("VFS",
                "Archive entry size mismatch for stored method (comp=%u uncomp=%u): %.*s",
                entry.compressedSize, entry.uncompressedSize,
                (int)normalizedRelativeVPath.size(), normalizedRelativeVPath.data());
            return false;
        }

        auto* backend = new ArchiveBackend();

        if (!noc::platform::OpenFile(backend->file, archivePath_, noc::platform::FileOpenMode::ReadOnly))
        {
            NOC_LOG_ERROR("VFS",
                "Archive open failed while opening entry: %.*s (archive=%s)",
                (int)normalizedRelativeVPath.size(), normalizedRelativeVPath.data(),
                archivePath_);
            delete backend;
            return false;
        }

        backend->entry = entry;

        // Seek to entry data start.
        if (!noc::platform::SeekFile(backend->file, entry.dataOffset))
        {
            NOC_LOG_ERROR("VFS",
                "Archive seek failed (offset=%llu) for entry: %.*s (archive=%s)",
                (unsigned long long)entry.dataOffset,
                (int)normalizedRelativeVPath.size(), normalizedRelativeVPath.data(),
                archivePath_);

            noc::platform::CloseFile(backend->file);
            delete backend;
            return false;
        }

        out.mount = this;
        out.backend = backend;
        out.sizeBytes = entry.uncompressedSize;
        out.cursor = 0;
        out.valid = true;
        return true;
    }


    void ArchiveFileMount::Close(FileHandle& h)
    {
        if (!h.valid || h.mount != this || !h.backend)
            return;

        auto* backend = reinterpret_cast<ArchiveBackend*>(h.backend);
        noc::platform::CloseFile(backend->file);
        delete backend;
        h = {};
    }

    size_t ArchiveFileMount::Read(FileHandle& h, void* dst, size_t bytes)
    {
        if (!h.valid || h.mount != this || !h.backend || bytes == 0)
            return 0;

        auto* backend = reinterpret_cast<ArchiveBackend*>(h.backend);

        // Clamp to remaining
        const uint64_t remaining = (h.cursor < h.sizeBytes) ? (h.sizeBytes - h.cursor) : 0;
        if (remaining == 0)
            return 0;

        size_t toRead = bytes;
        if ((uint64_t)toRead > remaining)
            toRead = (size_t)remaining;

        // Seek to absolute position in archive
        const uint64_t abs = backend->entry.dataOffset + h.cursor;
        if (!noc::platform::SeekFile(backend->file, abs))
            return 0;

        const size_t read = noc::platform::ReadFile(backend->file, dst, toRead);
        h.cursor += (uint64_t)read;
        return read;
    }

    uint64_t ArchiveFileMount::Size(const FileHandle& h) const
    {
        return h.sizeBytes;
    }
}
