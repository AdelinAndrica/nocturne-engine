#include "WinPlatform.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace noc::platform {

    uint64_t QueryHiResCounter()
    {
        LARGE_INTEGER v;
        ::QueryPerformanceCounter(&v);
        return static_cast<uint64_t>(v.QuadPart);
    }

    uint64_t QueryHiResFrequency()
    {
        LARGE_INTEGER v;
        ::QueryPerformanceFrequency(&v);
        return static_cast<uint64_t>(v.QuadPart);
    }

} // namespace noc::platform
