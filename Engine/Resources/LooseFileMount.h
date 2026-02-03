#pragma once

#include "IFileMount.h"

namespace noc
{
    class LooseFileMount final : public IFileMount
    {
    public:
        explicit LooseFileMount(const char* physicalRootUtf8);

        bool OpenRead(std::string_view normalizedRelativeVPath, FileHandle& out) override;
        void Close(FileHandle& h) override;
        size_t Read(FileHandle& h, void* dst, size_t bytes) override;
        uint64_t Size(const FileHandle& h) const override;

    private:
        char root_[512]{}; // UTF-8 root path (normalized to use '/')
    };
}
