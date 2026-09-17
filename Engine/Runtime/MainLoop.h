#pragma once
#include <windows.h>

namespace noc
{
    class Engine;
    class WinWindow;

    class MainLoop
    {
    public:
        using FrameCallback = void(*)(void* userData);

        // Design choice (not directly from the book): an optional pre-Tick hook lets
        // editor-only systems consume accumulated UI input exactly once per engine
        // frame without introducing a second application loop.
        void Run(Engine& engine, WinWindow& window,
            FrameCallback frameCallback = nullptr, void* userData = nullptr);

    private:
        void PumpMessagesNonBlocking(WinWindow& window);
    };
}
