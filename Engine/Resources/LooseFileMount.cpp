#include "LooseFileMount.h"

#include "Platform/Win32/WinFileSystem.h"
#include "Core/Log.h"

#include <cstring>
#include <string>

namespace noc
{
    struct LooseBackend
    {
        noc::platform::WinFile file{};
        uint64_t size = 0;
    };

    static void NormalizeRoot(char* dst, size_t dstBytes, const char* src)
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
        // trim trailing '/'
        size_t len = std::strlen(dst);
        while (len > 0 && dst[len - 1] == '/')
        {
            dst[len - 1] = 0;
            --len;
        }
    }

    LooseFileMount::LooseFileMount(const char* physicalRootUtf8)
    {
        NormalizeRoot(root_, sizeof(root_), physicalRootUtf8 ? physicalRootUtf8 : "");
    }

    bool LooseFileMount::OpenRead(std::string_view normalizedRelativeVPath, FileHandle& out)
    {
        out = {};

        char fullPath[1024]{};

        // FIX: JoinPathUtf8(out, outBytes, root, relative)
        const std::string rel(normalizedRelativeVPath);
        if (!noc::platform::JoinPathUtf8(fullPath, sizeof(fullPath), root_, rel.c_str()))
            return false;

        auto* backend = new LooseBackend();

        if (!noc::platform::OpenFile(backend->file, fullPath, noc::platform::FileOpenMode::ReadOnly))
        {
            delete backend;
            return false;
        }

        noc::platform::FileStat st{};
        if (!noc::platform::GetFileStat(backend->file, st))
        {
            noc::platform::CloseFile(backend->file);
            delete backend;
            return false;
        }

        backend->size = st.sizeBytes;

        out.mount = this;
        out.backend = backend;
        out.sizeBytes = backend->size;
        out.cursor = 0;
        out.valid = true;

        return true;
    }

    void LooseFileMount::Close(FileHandle& h)
    {
        if (!h.valid || h.mount != this || !h.backend)
            return;

        auto* backend = reinterpret_cast<LooseBackend*>(h.backend);
        noc::platform::CloseFile(backend->file);
        delete backend;

        h = {};
    }

    size_t LooseFileMount::Read(FileHandle& h, void* dst, size_t bytes)
    {
        if (!h.valid || h.mount != this || !h.backend || bytes == 0)
            return 0;

        auto* backend = reinterpret_cast<LooseBackend*>(h.backend);

        if (!noc::platform::SeekFile(backend->file, h.cursor))
            return 0;

        const size_t read = noc::platform::ReadFile(backend->file, dst, bytes);
        h.cursor += (uint64_t)read;
        return read;
    }

    uint64_t LooseFileMount::Size(const FileHandle& h) const
    {
        return h.sizeBytes;
    }
}
