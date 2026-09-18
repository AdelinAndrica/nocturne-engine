#include "MainLoop.h"
#include "Runtime/Engine.h"
#include "Platform/Win32/WinWindow.h"
#include "Core/Log.h"

namespace noc
{
    void MainLoop::PumpMessagesNonBlocking(
        WinWindow& window,
        MessageFilterCallback messageFilter,
        void* messageUserData)
    {
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            // Defensive: respect WM_QUIT too (e.g., PostQuitMessage).
            if (msg.message == WM_QUIT)
            {
                window.RequestQuit();
                return;
            }

            if (messageFilter
                && messageFilter(messageUserData, msg))
            {
                continue;
            }

            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    void MainLoop::Run(
        Engine& engine,
        WinWindow& window,
        FrameCallback frameCallback,
        void* frameUserData,
        MessageFilterCallback messageFilter,
        void* messageUserData)
    {
        NOC_LOG_INFO("Runtime", "MainLoop starting");

        while (!window.ShouldQuit())
        {
            PumpMessagesNonBlocking(
                window,
                messageFilter,
                messageUserData);

            engine.BeginFrame();
            if (frameCallback)
                frameCallback(frameUserData);
            engine.Tick();
            engine.EndFrame();
        }

        NOC_LOG_INFO("Runtime", "MainLoop exiting");
    }
}
