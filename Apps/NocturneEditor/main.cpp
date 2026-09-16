#include "EditorShellV3.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "Core/Log.h"
#include "Runtime/Engine.h"
#include "Runtime/MainLoop.h"
#include "Platform/Win32/WinWindow.h"

#ifndef NOC_CONTENT_ROOT
#define NOC_CONTENT_ROOT "Data"
#endif

int main()
{
    // Design choice (not directly from the book): make editor UI crisp on mixed-DPI
    // desktop setups. The runtime/window architecture remains unchanged.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    noc::Engine engine;
    engine.SetContentRoot(NOC_CONTENT_ROOT);

    if (!engine.Init())
    {
        NOC_LOG_FATAL("Editor", "Engine initialization failed");
        return 1;
    }

    noc::WinWindow window;
    noc::WinWindowDesc desc{};
    desc.title = L"Nocturne Editor";
    desc.width = 1600;
    desc.height = 920;
    desc.resizable = true;

    // Phase 13 owns only the editor shell. Do not attach the DX12 swap chain to the
    // top-level editor HWND: Phase 14 will provide a dedicated viewport render target.
    if (!window.Create(desc))
    {
        NOC_LOG_FATAL("Editor", "Failed to create editor window");
        engine.Shutdown();
        return 1;
    }

    nocturne::editor::EditorShellV3 shell;
    if (!shell.Init(engine, window))
    {
        NOC_LOG_FATAL("Editor", "Editor shell initialization failed");
        window.Destroy();
        engine.Shutdown();
        return 1;
    }

    // Architectural contract: Runtime owns the main loop. The editor is a client
    // layered on top of the same engine instance; it does not introduce an editor loop.
    noc::MainLoop loop;
    loop.Run(engine, window);

    shell.Shutdown();
    window.Destroy();
    engine.Shutdown();
    return 0;
}
