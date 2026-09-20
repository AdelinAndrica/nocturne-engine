#pragma once
#include <windows.h>

namespace noc
{
    class Engine;
    class WinWindow;

    /**
     * @brief Owns the blocking Win32 message/frame loop for an already-initialized Engine.
     *
     * MainLoop is intentionally small: pump pending window messages, begin one
     * engine frame, run an optional pre-Tick callback, service Engine::Tick(), then
     * finish/present through Engine::EndFrame().
     *
     * @par When to use
     * Use MainLoop when an application already owns the WinWindow but wants the
     * standard Nocturne frame order. Engine::Run() uses this internally for the
     * standalone host.
     *
     * @par Editor integration
     * The optional callback exists so editor-only systems can consume accumulated
     * UI input once per engine frame without creating a second master loop.
     *
     * @par Threading
     * Run() is a blocking main/runtime-thread loop. It does not create a second
     * application thread for frame orchestration.
     *
     * @see Engine::Run
     * @see Engine::BeginFrame
     * @see Engine::Tick
     * @see Engine::EndFrame
     * @ingroup runtime
     */
    class MainLoop
    {
    public:
        /**
         * @brief Function pointer invoked once per frame between BeginFrame() and Tick().
         * @param userData Opaque caller-owned pointer supplied to Run().
         */
        using FrameCallback = void(*)(void* userData);

        /**
         * @brief Runs frames until the window requests quit.
         *
         * Per iteration:
         * @code
         * PumpMessagesNonBlocking(window);
         * engine.BeginFrame();
         * if (frameCallback) frameCallback(userData);
         * engine.Tick();
         * engine.EndFrame();
         * @endcode
         *
         * @param engine Initialized Engine to drive. Not owned.
         * @param window Existing window whose quit state controls loop termination. Not owned.
         * @param frameCallback Optional editor/application callback before Engine::Tick().
         * @param userData Opaque pointer forwarded to frameCallback; MainLoop never owns it.
         *
         * @warning frameCallback executes inside the frame loop. Keep it bounded;
         * long blocking work directly increases frame time.
         */
        void Run(Engine& engine, WinWindow& window,
            FrameCallback frameCallback = nullptr, void* userData = nullptr);

    private:
        void PumpMessagesNonBlocking(WinWindow& window);
    };
}
