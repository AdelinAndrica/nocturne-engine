#include "WinFileSystem.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <cstring>

namespace noc::platform
{
    static bool Utf8ToWide(const char* utf8, wchar_t* outWide, int outWideCount)
    {
        if (!utf8 || !outWide || outWideCount <= 0)
            return false;

        const int needed = ::MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
        if (needed <= 0 || needed > outWideCount)
            return false;

        const int written = ::MultiByteToWideChar(CP_UTF8, 0, utf8, -1, outWide, outWideCount);
        return written > 0;
    }

    bool OpenFile(WinFile& outFile, const char* utf8Path, FileOpenMode mode)
    {
        outFile.handle = nullptr;

        wchar_t widePath[MAX_PATH * 4]{};
        if (!Utf8ToWide(utf8Path, widePath, (int)(sizeof(widePath) / sizeof(widePath[0]))))
            return false;

        DWORD access = 0;
        DWORD share = FILE_SHARE_READ;

        switch (mode)
        {
        case FileOpenMode::ReadOnly:
            access = GENERIC_READ;
            break;
        default:
            access = GENERIC_READ;
            break;
        }

        HANDLE h = ::CreateFileW(
            widePath,
            access,
            share,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );

        if (h == INVALID_HANDLE_VALUE)
            return false;

        outFile.handle = (void*)h;
        return true;
    }

    void CloseFile(WinFile& file)
    {
        if (file.handle)
        {
            ::CloseHandle((HANDLE)file.handle);
            file.handle = nullptr;
        }
    }

    size_t ReadFile(WinFile& file, void* dst, size_t bytes)
    {
        if (!file.handle || !dst || bytes == 0)
            return 0;

        DWORD read = 0;
        const DWORD toRead = (bytes > 0xFFFFFFFFull) ? 0xFFFFFFFFu : (DWORD)bytes;

        if (!::ReadFile((HANDLE)file.handle, dst, toRead, &read, nullptr))
            return 0;

        return (size_t)read;
    }

    bool SeekFile(WinFile& file, uint64_t absoluteOffset)
    {
        if (!file.handle)
            return false;

        LARGE_INTEGER li{};
        li.QuadPart = (LONGLONG)absoluteOffset;

        return ::SetFilePointerEx((HANDLE)file.handle, li, nullptr, FILE_BEGIN) != 0;
    }

    bool GetFileStat(WinFile& file, FileStat& outStat)
    {
        outStat = {};

        if (!file.handle)
            return false;

        LARGE_INTEGER sz{};
        if (!::GetFileSizeEx((HANDLE)file.handle, &sz))
            return false;

        outStat.sizeBytes = (uint64_t)sz.QuadPart;
        return true;
    }

    bool GetExecutableDirectoryUtf8(char* outBuf, size_t outBufBytes)
    {
        if (!outBuf || outBufBytes == 0)
            return false;

        wchar_t wpath[MAX_PATH]{};
        const DWORD len = ::GetModuleFileNameW(nullptr, wpath, (DWORD)(sizeof(wpath) / sizeof(wpath[0])));
        if (len == 0 || len >= (DWORD)(sizeof(wpath) / sizeof(wpath[0])))
            return false;

        // Strip filename.
        for (int i = (int)len - 1; i >= 0; --i)
        {
            if (wpath[i] == L'\\' || wpath[i] == L'/')
            {
                wpath[i] = 0;
                break;
            }
        }

        const int needed = ::WideCharToMultiByte(CP_UTF8, 0, wpath, -1, nullptr, 0, nullptr, nullptr);
        if (needed <= 0 || (size_t)needed > outBufBytes)
            return false;

        const int written = ::WideCharToMultiByte(CP_UTF8, 0, wpath, -1, outBuf, (int)outBufBytes, nullptr, nullptr);
        return written > 0;
    }

    bool JoinPathUtf8(char* outBuf, size_t outBufBytes, const char* a, const char* b)
    {
        if (!outBuf || outBufBytes == 0 || !a || !b)
            return false;

        const size_t aLen = std::strlen(a);
        const size_t bLen = std::strlen(b);

        // Worst case: a + "/" + b + "\0"
        if (aLen + 1 + bLen + 1 > outBufBytes)
            return false;

        std::memcpy(outBuf, a, aLen);
        size_t pos = aLen;

        if (pos > 0 && outBuf[pos - 1] != '/' && outBuf[pos - 1] != '\\')
            outBuf[pos++] = '/';

        // Copy b
        std::memcpy(outBuf + pos, b, bLen);
        pos += bLen;
        outBuf[pos] = 0;

        // Normalize slashes to '/'
        for (size_t i = 0; i < pos; ++i)
        {
            if (outBuf[i] == '\\')
                outBuf[i] = '/';
        }

        return true;
    }
}
