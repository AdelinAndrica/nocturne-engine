#include "Assert.h"
#include <cstdio>
#include <cstdlib>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace noc {

    static AssertFailHandler g_handler = nullptr;

    void SetAssertFailHandler(AssertFailHandler handler)
    {
        g_handler = handler;
    }

    void DefaultAssertFailHandler(const char* expr,
        const char* file,
        int line,
        const char* msg)
    {
        if (g_handler)
        {
            g_handler(expr, file, line, msg);
            return;
        }

        char buffer[1024];
        if (msg)
            std::snprintf(buffer, sizeof(buffer), "ASSERT FAILED: %s\n%s(%d)\nMSG: %s\n", expr, file, line, msg);
        else
            std::snprintf(buffer, sizeof(buffer), "ASSERT FAILED: %s\n%s(%d)\n", expr, file, line);

#if defined(_WIN32)
        OutputDebugStringA(buffer);
#endif

        std::fputs(buffer, stderr);
        std::fflush(stderr);

        // In case no debugger is attached and execution continues, fail hard.
        std::abort();
    }

} // namespace noc
