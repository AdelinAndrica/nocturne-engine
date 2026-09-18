#include "EditorSession.h"
#include "EditorShellV3.h"
#include "EditorViewportController.h"

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
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    noc::Engine engine;
    engine.SetContentRoot(NOC_CONTENT_ROOT);

    if (!engine.Init())
    {
        NOC_LOG_FATAL("Editor", "Engine initialization failed");
        return 1;
    }

    nocturne::editor::EditorSession session;
    if (!session.Init(
            engine.GetWorld(),
            engine.Reflection(),
            engine.Allocator()))
    {
        NOC_LOG_FATAL("Editor", "Editor session initialization failed");
        engine.Shutdown();
        return 1;
    }

    // Prepare the viewport bootstrap scene through the same authoritative World.
    nocturne::editor::EditorViewportController viewport;
    if (!viewport.PrepareScene(engine, session))
    {
        NOC_LOG_FATAL("Editor", "Phase 14 viewport scene preparation failed");
        session.Shutdown();
        engine.Shutdown();
        return 1;
    }

    noc::WinWindow window;
    noc::WinWindowDesc desc{};
    desc.title = L"Nocturne Editor";
    desc.width = 1600;
    desc.height = 920;
    desc.resizable = true;

    // The top-level editor window remains chrome-only. Phase 14 attaches DX12
    // to a dedicated child HWND created inside EditorShellV3's viewport body.
    if (!window.Create(desc))
    {
        NOC_LOG_FATAL("Editor", "Failed to create editor window");
        engine.Shutdown();
        return 1;
    }

    nocturne::editor::EditorShellV3 shell;
    if (!shell.Init(engine, window, session))
    {
        NOC_LOG_FATAL("Editor", "Editor shell initialization failed");
        window.Destroy();
        session.Shutdown();
        engine.Shutdown();
        return 1;
    }

    if (!viewport.Attach(engine, window, shell, session))
    {
        NOC_LOG_FATAL("Editor", "Phase 14 viewport attachment failed");
        shell.Shutdown();
        session.Shutdown();
        engine.Shutdown();
        window.Destroy();
        return 1;
    }

    // Architectural contract: Runtime owns the only main loop. The optional
    // frame hook lets the editor consume accumulated viewport input exactly once
    // per engine frame before World::Update(), without a second editor loop.
    noc::MainLoop loop;
    loop.Run(engine, window,
        [](void* userData)
        {
            static_cast<nocturne::editor::EditorViewportController*>(userData)->TickFrame();
        },
        &viewport);

    viewport.Shutdown();
    shell.Shutdown();
    session.Shutdown();
    // Release DXGI presentation resources while their child HWND is still valid.
    engine.Shutdown();
    window.Destroy();
    return 0;
}
