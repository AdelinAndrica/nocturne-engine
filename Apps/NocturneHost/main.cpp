#include "Runtime/Engine.h"
#include "Core/Log.h"

#ifndef NOC_CONTENT_ROOT
#define NOC_CONTENT_ROOT "."
#endif

int main()
{
    noc::Engine engine;

    // ---- Engine configuration (pre-Init only) ----
    engine.SetContentRoot(NOC_CONTENT_ROOT);

    // ---- Engine startup ----
    if (!engine.Init())
    {
        NOC_LOG_FATAL("Host", "Engine initialization failed");
        return -1;
    }

    // ---- Run application (creates window + main loop) ----
    const int exitCode = engine.Run();

    // ---- Shutdown ----
    engine.Shutdown();

    return exitCode;
}
