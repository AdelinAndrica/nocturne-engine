#include "Runtime/Engine.h"
#include "Core/Log.h"

#ifndef NOC_CONTENT_ROOT
#define NOC_CONTENT_ROOT .
#endif

// Phase 5 test runner (defined in Apps/NocturneHost/Tests/phase5_tests.cpp)
int RunPhase5Tests();

int main()
{
    // Run the Phase 5 test suite instead of the normal engine loop.
    // If you want to switch between tests and normal host behavior, add a compile-time define
    // (e.g. NOC_RUN_PHASE5_TESTS) and wrap this call in #if / #else.
    return RunPhase5Tests();
}
