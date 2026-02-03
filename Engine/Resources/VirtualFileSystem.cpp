#include "VirtualFileSystem.h"

#include "VPath.h"
#include "IFileMount.h"
#include "LooseFileMount.h"
#include "ArchiveFileMount.h"

#include "Core/Log.h"

#include <cstring>   // memcpy
#include <limits>    // numeric_limits

namespace noc
{
    VirtualFileSystem::~VirtualFileSystem() = default;

    bool VirtualFileSystem::MountLooseDirectory(const char* physicalRootUtf8)
    {
        if (!physicalRootUtf8 || physicalRootUtf8[0] == 0)
            return false;

        mounts_.push_back(std::make_unique<LooseFileMount>(physicalRootUtf8));
        NOC_LOG_INFO("VFS", "Mounted loose directory: %s (priority=%zu)", physicalRootUtf8, mounts_.size() - 1);
        return true;
    }

    bool VirtualFileSystem::MountArchive(const char* archivePathUtf8)
    {
        if (!archivePathUtf8 || archivePathUtf8[0] == 0)
            return false;

        auto mount = std::make_unique<ArchiveFileMount>(archivePathUtf8);
        if (!mount->BuildIndex())
        {
            NOC_LOG_ERROR("VFS", "Failed to mount archive: %s", archivePathUtf8);
            return false;
        }

        mounts_.push_back(std::move(mount));
        NOC_LOG_INFO("VFS", "Mounted archive: %s (priority=%zu)", archivePathUtf8, mounts_.size() - 1);
        return true;
    }

    FileHandle VirtualFileSystem::OpenRead(std::string_view virtualPath)
    {
        FileHandle h{};

        char norm[512]{};
        if (!noc::vpath::NormalizeToRelative(norm, sizeof(norm), virtualPath))
        {
            NOC_LOG_ERROR("VFS", "Invalid virtual path: %.*s", (int)virtualPath.size(), virtualPath.data());
            return h;
        }

        const std::string_view normalized{ norm };

        // Later mounts override earlier mounts -> search in reverse order.
        for (size_t i = mounts_.size(); i-- > 0; )
        {
            auto& m = mounts_[i];
            if (!m)
                continue;

            if (m->OpenRead(normalized, h))
            {
                h.valid = true;
                return h;
            }
        }

        NOC_LOG_ERROR("VFS", "File not found in any mount: %s", norm);
        return {};
    }


    void VirtualFileSystem::Close(FileHandle& h)
    {
        if (!h.valid || !h.mount)
            return;

        h.mount->Close(h);

        // Defensive: make double-close safe.
        h.valid = false;
        h.mount = nullptr;
        h.backend = nullptr;
        h.sizeBytes = 0;
        h.cursor = 0;
    }


    size_t VirtualFileSystem::Read(FileHandle& h, void* dst, size_t bytes)
    {
        if (!h.valid || !h.mount)
            return 0;

        return h.mount->Read(h, dst, bytes);
    }

    uint64_t VirtualFileSystem::Size(const FileHandle& h) const
    {
        if (!h.valid || !h.mount)
            return 0;

        return h.mount->Size(h);
    }

    // ------------------------------------------------------------
    // Convenience APIs
    // ------------------------------------------------------------

    uint8_t* VirtualFileSystem::ReadAllBytes(const char* virtualPath, size_t& outSize, IAllocator& alloc)
    {
        outSize = 0;

        if (!virtualPath || virtualPath[0] == 0)
            return nullptr;

        FileHandle h = this->OpenRead(std::string_view{ virtualPath });
        if (!h.valid)
            return nullptr;

        const uint64_t size64 = this->Size(h);
        if (size64 == 0)
        {
            // Zero-length file is valid; return a non-null pointer only if you want.
            // Design choice: return nullptr for empty file, but treat as success.
            this->Close(h);
            outSize = 0;
            return nullptr;
        }

        if (size64 > static_cast<uint64_t>(std::numeric_limits<size_t>::max()))
        {
            NOC_LOG_ERROR("VFS", "ReadAllBytes too large for size_t: %s (size=%llu)",
                virtualPath, (unsigned long long)size64);
            this->Close(h);
            return nullptr;
        }

        const size_t size = static_cast<size_t>(size64);
        void* mem = alloc.Allocate(size, 16);
        if (!mem)
        {
            NOC_LOG_ERROR("VFS", "ReadAllBytes allocation failed: %s (size=%zu)", virtualPath, size);
            this->Close(h);
            return nullptr;
        }

        uint8_t* data = static_cast<uint8_t*>(mem);

        size_t totalRead = 0;
        while (totalRead < size)
        {
            const size_t toRead = size - totalRead;
            const size_t got = this->Read(h, data + totalRead, toRead);
            if (got == 0)
            {
                NOC_LOG_ERROR("VFS", "ReadAllBytes short read: %s (got=%zu expected=%zu)",
                    virtualPath, totalRead, size);
                alloc.Deallocate(data);
                this->Close(h);
                return nullptr;
            }
            totalRead += got;
        }

        this->Close(h);
        outSize = size;
        return data;
    }

    bool VirtualFileSystem::ReadAllText(const char* virtualPath, IAllocator& alloc, char*& outText)
    {
        outText = nullptr;

        if (!virtualPath || virtualPath[0] == 0)
            return false;

        FileHandle h = this->OpenRead(std::string_view{ virtualPath });
        if (!h.valid)
            return false;

        const uint64_t size64 = this->Size(h);
        if (size64 > static_cast<uint64_t>(std::numeric_limits<size_t>::max() - 1))
        {
            NOC_LOG_ERROR("VFS", "ReadAllText too large for size_t: %s (size=%llu)",
                virtualPath, (unsigned long long)size64);
            this->Close(h);
            return false;
        }

        const size_t size = static_cast<size_t>(size64);

        // +1 for '\0'
        char* text = static_cast<char*>(alloc.Allocate(size + 1, 16));
        if (!text)
        {
            NOC_LOG_ERROR("VFS", "ReadAllText allocation failed: %s (size=%zu)", virtualPath, size + 1);
            this->Close(h);
            return false;
        }

        size_t totalRead = 0;
        while (totalRead < size)
        {
            const size_t toRead = size - totalRead;
            const size_t got = this->Read(h, text + totalRead, toRead);
            if (got == 0)
            {
                NOC_LOG_ERROR("VFS", "ReadAllText short read: %s (got=%zu expected=%zu)",
                    virtualPath, totalRead, size);
                alloc.Deallocate(text);
                this->Close(h);
                return false;
            }
            totalRead += got;
        }

        text[size] = '\0';

        this->Close(h);
        outText = text;
        return true;
    }
}
