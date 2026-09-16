#include "EditorShell.h"

#include "Core/Log.h"
#include "Runtime/Engine.h"
#include "Runtime/MainLoop.h"
#include "Platform/Win32/WinWindow.h"

#ifndef NOC_CONTENT_ROOT
#define NOC_CONTENT_ROOT "Data"
#endif

int main()
{
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

    if (!engine.CreateAndAttachMainWindow(desc, window))
    {
        NOC_LOG_FATAL("Editor", "Failed to create and attach editor window");
        engine.Shutdown();
        return 1;
    }

    nocturne::editor::EditorShell shell;
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
