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
        using MessageFilterCallback =
            bool(*)(void* userData, const MSG& message);

        // Design choice (not directly from the book): optional hooks let
        // editor-only systems consume accumulated frame input and filter native
        // tool messages without introducing a second application loop.
        void Run(
            Engine& engine,
            WinWindow& window,
            FrameCallback frameCallback = nullptr,
            void* frameUserData = nullptr,
            MessageFilterCallback messageFilter = nullptr,
            void* messageUserData = nullptr);

    private:
        void PumpMessagesNonBlocking(
            WinWindow& window,
            MessageFilterCallback messageFilter,
            void* messageUserData);
    };
}
