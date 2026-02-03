#include "VPath.h"

#include <cctype>
#include <cstring>

namespace noc::vpath
{
    static bool IsDriveLetterPath(std::string_view p)
    {
        // "C:\..." or "C:/..."
        if (p.size() >= 2 && std::isalpha((unsigned char)p[0]) && p[1] == ':')
            return true;
        return false;
    }

    bool IsValid(std::string_view p)
    {
        if (p.empty())
            return false;

        if (IsDriveLetterPath(p))
            return false;

        // Reject UNC-ish or double leading slashes
        if (p.size() >= 2 && (p[0] == '/' || p[0] == '\\') && (p[1] == '/' || p[1] == '\\'))
            return false;

        // Reject backslashes anywhere
        for (char c : p)
        {
            if (c == '\\')
                return false;
        }

        // Reject ".." segments
        // We check token-by-token split on '/'
        size_t i = 0;
        while (i < p.size())
        {
            // Skip '/'
            while (i < p.size() && p[i] == '/')
                ++i;

            size_t start = i;
            while (i < p.size() && p[i] != '/')
                ++i;

            const size_t len = i - start;
            if (len == 2 && p[start] == '.' && p[start + 1] == '.')
                return false;
        }

        return true;
    }

    bool NormalizeToRelative(char* outBuf, size_t outBufBytes, std::string_view p)
    {
        if (!outBuf || outBufBytes == 0)
            return false;

        // Convert '\' to '/' for the purpose of normalization
        // but reject drive letters and ".." segments.
        if (IsDriveLetterPath(p))
            return false;

        // Reject ".." segments even if backslashes exist
        // (we normalize slashes in a copy loop).
        // Also reject UNC style.
        if (p.size() >= 2 && (p[0] == '/' || p[0] == '\\') && (p[1] == '/' || p[1] == '\\'))
            return false;

        // Build normalized output:
        // - remove leading '/'
        // - collapse multiple '/'
        // - convert '\' to '/'
        size_t out = 0;
        bool lastWasSlash = false;

        for (size_t i = 0; i < p.size(); ++i)
        {
            char c = p[i];
            if (c == '\\') c = '/';

            if (c == '/')
            {
                // skip leading slash
                if (out == 0)
                    continue;

                if (lastWasSlash)
                    continue;

                if (out + 1 >= outBufBytes)
                    return false;

                outBuf[out++] = '/';
                lastWasSlash = true;
                continue;
            }

            // normal char
            if (out + 1 >= outBufBytes)
                return false;

            outBuf[out++] = c;
            lastWasSlash = false;
        }

        // Trim trailing '/'
        while (out > 0 && outBuf[out - 1] == '/')
            --out;

        if (out == 0)
            return false;

        outBuf[out] = 0;

        // Now validate segments for ".."
        std::string_view norm(outBuf, out);
        if (!IsValid(norm))
            return false;

        return true;
    }
}
