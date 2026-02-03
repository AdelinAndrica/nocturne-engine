#pragma once

#include <cstdint>
#include <cstddef>

namespace noc::platform
{
    // Opaque native file handle wrapper.
    struct WinFile
    {
        void* handle = nullptr; // HANDLE
    };

    enum class FileOpenMode : uint8_t
    {
        ReadOnly
    };

    struct FileStat
    {
        uint64_t sizeBytes = 0;
    };

    // Opens a file (UTF-8 path). Returns true on success.
    bool OpenFile(WinFile& outFile, const char* utf8Path, FileOpenMode mode);

    // Closes file if open.
    void CloseFile(WinFile& file);

    // Reads from current cursor. Returns bytes read (0 on EOF or failure).
    size_t ReadFile(WinFile& file, void* dst, size_t bytes);

    // Seeks to absolute offset from beginning. Returns true on success.
    bool SeekFile(WinFile& file, uint64_t absoluteOffset);

    // Gets file size. Returns true on success.
    bool GetFileStat(WinFile& file, FileStat& outStat);

    // Utilities

    bool GetExecutableDirectoryUtf8(char* outBuf, size_t outBufBytes); // null-terminated
    bool JoinPathUtf8(char* outBuf, size_t outBufBytes, const char* a, const char* b); // "a/b"
}
