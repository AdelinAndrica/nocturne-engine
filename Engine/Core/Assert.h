#pragma once
#include "BuildConfig.h"

namespace noc {

    using AssertFailHandler = void(*)(const char* expr,
        const char* file,
        int line,
        const char* msg);

    void SetAssertFailHandler(AssertFailHandler handler);
    void DefaultAssertFailHandler(const char* expr,
        const char* file,
        int line,
        const char* msg);

} // namespace noc

#if NOC_ENABLE_ASSERTS

#if defined(_MSC_VER)
#define NOC_DEBUG_BREAK() __debugbreak()
#else
#define NOC_DEBUG_BREAK() ((void)0)
#endif

#define NOC_ASSERT(expr) \
        do { \
            if (!(expr)) { \
                ::noc::DefaultAssertFailHandler(#expr, __FILE__, __LINE__, nullptr); \
                NOC_DEBUG_BREAK(); \
            } \
        } while (0)

#define NOC_ASSERT_MSG(expr, msg) \
        do { \
            if (!(expr)) { \
                ::noc::DefaultAssertFailHandler(#expr, __FILE__, __LINE__, (msg)); \
                NOC_DEBUG_BREAK(); \
            } \
        } while (0)

#else

#define NOC_ASSERT(expr)        do { (void)sizeof(expr); } while (0)
#define NOC_ASSERT_MSG(expr, msg) do { (void)sizeof(expr); (void)(msg); } while (0)

#endif

// Design choice: VERIFY evaluates in all builds
#define NOC_VERIFY(expr) \
    do { \
        if (!(expr)) { \
            NOC_ASSERT(expr); \
        } \
    } while (0)
