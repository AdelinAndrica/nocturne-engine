#pragma once
#include <windows.h>

namespace noc
{
    class Engine;
    class WinWindow;

    class MainLoop
    {
    public:
        void Run(Engine& engine, WinWindow& window);

    private:
        void PumpMessagesNonBlocking(WinWindow& window);
    };
}
