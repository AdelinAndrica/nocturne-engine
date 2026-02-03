# Phase 2 — Window, Message Pump & Main Loop (Final)

> **Status:** READY FOR IMPLEMENTATION ⏳
> **Scope:** Win32 window creation, OS message pump, continuous real-time main loop
> **Depends on:** Phase 1 — Core Systems
>
> **Primary source of truth:** Jason Gregory, *Game Engine Architecture (3rd Edition)* 

This document is the **authoritative implementation reference** for Phase 2 of Nocturne Engine.
It defines the exact responsibilities, boundaries, and verification criteria for introducing a real OS window and a true engine-owned game loop.

---

## 1. Phase Objective

Transform the engine from a **single-tick test harness** into a **real-time application** by adding:

* A native **Win32 window**
* A non-blocking **OS message pump**
* A continuous **engine-owned main loop**
* Deterministic **frame lifecycle stages**

At the end of Phase 2, Nocturne Engine runs continuously until the user closes the window.

No rendering, input mapping, or gameplay logic exists yet.

---

## 2. Book-Grounded Scope (Why This Phase Exists)

Phase 2 is derived primarily from:

* **Chapter 6 — Engine Support Systems**
  Subsystem ownership, startup/shutdown ordering, and engine-controlled execution 
* **Chapter 8.5 — Measuring and Dealing with Time**
  Frame-to-frame timing, real-time loop considerations, and dt stability 
* **Chapter 3 — The Game Loop**
  Why the engine—not the OS—must own the loop structure and frame boundaries 

Everything else is explicitly labeled **Design choice (not directly from the book)**.

---

## 3. Phase 2 Responsibilities (Hard Rules)

Phase 2 introduces **exactly three new responsibilities**:

1. **Window ownership** (platform-specific)
2. **Message pumping** (non-blocking)
3. **Continuous main loop** (engine-owned)

### Explicit Non-Goals

* ❌ No rendering API
* ❌ No input abstraction layer
* ❌ No ECS, scene, or gameplay code
* ❌ No editor or tools

---

## 4. Updated Folder Structure (After Phase 2)

```
Nocturne/
├── Engine/
│   ├── Core/                          // unchanged from Phase 1
│   │   ├── Assert.h
│   │   ├── Assert.cpp
│   │   ├── BuildConfig.h
│   │   ├── Log.h
│   │   ├── Log.cpp
│   │   ├── Clock.h
│   │   ├── Clock.cpp
│   │   ├── Memory/
│   │   │   ├── Allocator.h
│   │   │   ├── Allocator.cpp
│   │   │   ├── DebugAlloc.h
│   │   │   ├── DebugAlloc.cpp
│   │   │   ├── LinearArena.h
│   │   │   └── LinearArena.cpp
│   │   └── Subsystems/
│   │       ├── Subsystem.h
│   │       └── SubsystemRegistry.h / .cpp
│   │
│   ├── Platform/
│   │   └── Win32/
│   │       ├── WinPlatform.h
│   │       ├── WinPlatform.cpp
│   │       ├── WinWindow.h              // NEW (Phase 2)
│   │       └── WinWindow.cpp            // NEW (Phase 2)
│   │
│   └── Runtime/
│       ├── Engine.h
│       ├── Engine.cpp                   // UPDATED (Phase 2)
│       ├── MainLoop.h                   // NEW (Phase 2)
│       └── MainLoop.cpp                 // NEW (Phase 2)
│
├── Apps/
│   └── NocturneHost/
│       ├── main.cpp                     // UPDATED (Phase 2)
│       └── AppConfig.h                  // NEW (Phase 2, window params)
│
└── Docs/
    ├── Nocturne Engine Architecture.md
    ├── Phase 1 — Core Systems.md
    └── Phase 2 — Window & Main Loop.md
```

---

## 5. New Subsystems Introduced

### 5.1 Window Subsystem

**Location:** `Engine/Platform/Win32/WinWindow.*`

**Responsibility:**

* Register Win32 window class
* Create and destroy a native window
* Own the `HWND`
* Forward OS close events to the engine

**Rules:**

* No rendering knowledge
* No message pumping logic
* No global HWND access

> **Book grounding:** Platform abstraction and OS isolation are mandatory engine support systems. 

---

### 5.2 Main Loop (Runtime-Owned)

**Location:** `Engine/Runtime/MainLoop.*`

**Responsibility:**

* Own the real-time loop
* Define per-frame execution stages
* Integrate Phase 1 TimeSystem

Canonical structure:

```cpp
while (engineRunning)
{
    PumpOSMessages();   // non-blocking
    BeginFrame();       // TimeSystem::BeginFrame
    Tick();             // empty for now
    EndFrame();         // TimeSystem::EndFrame
}
```

> **Book grounding:** The engine must own the loop to guarantee determinism and subsystem ordering. 

---

## 6. Engine Lifecycle (Updated)

### Startup Order (Phase 2)

1. Core subsystems (Phase 1)
2. Window subsystem
3. Main loop initialization

### Shutdown Order

1. Main loop stops
2. Window destroyed
3. Core subsystems shut down (reverse order)

This preserves **Phase 1 guarantees**: no global static teardown hazards.

---

## 7. Message Pump Design

### Non-Blocking Rule (Mandatory)

* Use `PeekMessage`, **never** `GetMessage`
* Engine loop must continue even with no OS messages

```cpp
while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
{
    TranslateMessage(&msg);
    DispatchMessage(&msg);
}
```

> **Book grounding:** Blocking the engine loop breaks real-time simulation assumptions. 

---

## 8. Frame Timing Rules

* `TimeSystem::BeginFrame()` called **once per loop**
* `TimeSystem::EndFrame()` called **once per loop**
* dt clamping behavior from Phase 1 remains unchanged
* No fixed timestep yet (added later)

This ensures **stable dt** before rendering or simulation exist.

---

## 9. Relationships & Dependency Graph (Phase 2)

```
Win32 OS
   ↓
Platform/WinWindow
   ↓
Runtime/MainLoop
   ↓
Runtime/Engine
   ↓
Core (Time, Log, Memory, Assert)
```

Rules:

* Platform code never depends on Runtime
* Runtime owns execution
* Core remains dependency-free

---

## 10. Verification Checklist (Phase 2 Is Done When…)

* [ ] Window appears and remains responsive
* [ ] Engine runs continuously until window close
* [ ] Closing the window exits cleanly
* [ ] dt is stable and logged every frame
* [ ] No blocking calls stall the loop
* [ ] All subsystems shut down deterministically

---

## 11. Common Pitfalls (Phase 2)

* Using `GetMessage` instead of `PeekMessage` (hard stall) 
* Letting Win32 own the main loop (inverted control)
* Tying dt to OS message frequency
* Performing rendering or input mapping too early

---

## 12. Phase 2 Completion Criteria

Phase 2 is complete when:

* The engine owns a real-time loop
* The OS window lifecycle is fully controlled
* Phase 1 systems remain unchanged and stable

No graphics. No gameplay. No shortcuts.

---

## 13. Next Phase Handoff

When Phase 2 is complete, say:

> “Phase 2 is complete. The window runs and the loop is stable. Start Phase 3: Rendering bootstrap.”

Phase 3 will introduce:

* Graphics API selection (DX12)
* Swap chain
* First triangle

---

## 14. Implementations

Below are **all files implemented/modified in Phase 2**, each with:

* **`path/to/file`**
* **full implementation**

---

### `Apps/NocturneHost/AppConfig.h`

```cpp
#pragma once

namespace noc::app
{
    struct AppConfig
    {
        const wchar_t* windowTitle = L"NocturneHost";
        int windowWidth = 1280;
        int windowHeight = 720;
        bool resizable = true;
    };
}
```

---

### `Apps/NocturneHost/main.cpp`

```cpp
#include "Engine/Runtime/Engine.h"

int main()
{
    noc::Engine engine;
    if (!engine.Init())
        return -1;

    const int rc = engine.Run();
    engine.Shutdown();
    return rc;
}
```

---

### `Engine/Platform/Win32/WinWindow.h`

```cpp
#pragma once

#include <windows.h>

namespace noc
{
    struct WinWindowDesc
    {
        const wchar_t* title = L"Nocturne";
        int width = 1280;
        int height = 720;
        bool resizable = true;
    };

    class WinWindow
    {
    public:
        bool Create(const WinWindowDesc& desc);
        void Destroy();

        HWND Handle() const { return hwnd_; }

        bool ShouldQuit() const { return shouldQuit_; }
        void RequestQuit() { shouldQuit_ = true; }

    private:
        static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

        bool RegisterClassOnce();
        void ApplyClientSize(int clientW, int clientH, bool resizable);

    private:
        HWND hwnd_ = nullptr;
        bool classRegistered_ = false;
        bool shouldQuit_ = false;
    };
} // namespace noc
```

---

### `Engine/Platform/Win32/WinWindow.cpp`

```cpp
#include "WinWindow.h"

#include "Engine/Core/Log.h"
#include "Engine/Core/Assert.h"

namespace noc
{
    static const wchar_t* kWndClassName = L"NocturneWindowClass";

    bool WinWindow::RegisterClassOnce()
    {
        if (classRegistered_)
            return true;

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = &WinWindow::WndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = kWndClassName;

        if (!RegisterClassExW(&wc))
        {
            NOC_LOG_ERROR("Win32", "RegisterClassExW failed (err=%lu)", GetLastError());
            return false;
        }

        classRegistered_ = true;
        return true;
    }

    void WinWindow::ApplyClientSize(int clientW, int clientH, bool resizable)
    {
        DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        if (resizable)
            style |= WS_THICKFRAME | WS_MAXIMIZEBOX;

        RECT r{ 0, 0, clientW, clientH };
        AdjustWindowRect(&r, style, FALSE);

        const int winW = r.right - r.left;
        const int winH = r.bottom - r.top;

        SetWindowLongPtrW(hwnd_, GWL_STYLE, (LONG_PTR)style);
        SetWindowPos(hwnd_, nullptr, 100, 100, winW, winH, SWP_NOZORDER | SWP_FRAMECHANGED);
    }

    bool WinWindow::Create(const WinWindowDesc& desc)
    {
        if (!RegisterClassOnce())
            return false;

        HINSTANCE hInst = GetModuleHandleW(nullptr);

        // Create with a temporary style. We'll correct it after AdjustWindowRect.
        DWORD style = WS_OVERLAPPEDWINDOW;

        hwnd_ = CreateWindowExW(
            0,
            kWndClassName,
            desc.title,
            style,
            CW_USEDEFAULT, CW_USEDEFAULT,
            desc.width, desc.height,
            nullptr, nullptr,
            hInst,
            this // pass pointer for association
        );

        if (!hwnd_)
        {
            NOC_LOG_ERROR("Win32", "CreateWindowExW failed (err=%lu)", GetLastError());
            return false;
        }

        ApplyClientSize(desc.width, desc.height, desc.resizable);

        ShowWindow(hwnd_, SW_SHOW);
        UpdateWindow(hwnd_);

        NOC_LOG_INFO("Win32", "Window created: %dx%d", desc.width, desc.height);
        return true;
    }

    void WinWindow::Destroy()
    {
        if (hwnd_)
        {
            DestroyWindow(hwnd_);
            hwnd_ = nullptr;
        }
    }

    LRESULT CALLBACK WinWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        // Associate the WinWindow* with the HWND on WM_NCCREATE.
        if (msg == WM_NCCREATE)
        {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            auto* win = reinterpret_cast<WinWindow*>(cs->lpCreateParams);
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(win));
            return DefWindowProcW(hWnd, msg, wParam, lParam);
        }

        auto* win = reinterpret_cast<WinWindow*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

        switch (msg)
        {
        case WM_CLOSE:
            if (win) win->RequestQuit();
            return 0;

        case WM_DESTROY:
            if (win) win->RequestQuit();
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcW(hWnd, msg, wParam, lParam);
        }
    }
} // namespace noc
```

---

### `Engine/Runtime/MainLoop.h`

```cpp
#pragma once

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
} // namespace noc
```

---

### `Engine/Runtime/MainLoop.cpp`

```cpp
#include "MainLoop.h"

#include <windows.h>

#include "Engine/Runtime/Engine.h"
#include "Engine/Platform/Win32/WinWindow.h"
#include "Engine/Core/Log.h"

namespace noc
{
    void MainLoop::PumpMessagesNonBlocking(WinWindow& window)
    {
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            // Defensive: respect WM_QUIT too (e.g., PostQuitMessage)
            if (msg.message == WM_QUIT)
            {
                window.RequestQuit();
                return;
            }

            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    void MainLoop::Run(Engine& engine, WinWindow& window)
    {
        NOC_LOG_INFO("Runtime", "MainLoop starting");

        while (!window.ShouldQuit())
        {
            PumpMessagesNonBlocking(window);

            engine.BeginFrame();
            engine.Tick();      // intentionally empty in Phase 2
            engine.EndFrame();
        }

        NOC_LOG_INFO("Runtime", "MainLoop exiting");
    }
} // namespace noc
```

---

### `Engine/Runtime/Engine.h`
```cpp
#pragma once
#include "Core/BuildConfig.h"
#include "Core/Subsystems/SubsystemRegistry.h"
#include "Core/Memory/Allocator.h"
#include "Core/Memory/LinearArena.h"

#if NOC_ENABLE_ASSERTS
#include "Core/Memory/DebugAlloc.h"
#endif

namespace noc {

    class WinWindow;
    class MainLoop;

    class Engine
    {
    public:
        bool Init();
        void TickOnce();
        void Shutdown();

        int Run();                 // creates window + runs loop
        void BeginFrame();
        void Tick();
        void EndFrame();

        IAllocator& Allocator();
        LinearArena& FrameArena();

        // Internal lifecycle used by subsystems (keeps members private)
        bool InitMemory();
        void KillMemory();

    private:
        SubsystemRegistry registry_;

        MallocAllocator baseAlloc_;

#if NOC_ENABLE_ASSERTS
        DebugAlloc debugAlloc_{ baseAlloc_ };
        IAllocator* alloc_ = &debugAlloc_;
#else
        IAllocator* alloc_ = &baseAlloc_;
#endif

        void* frameArenaMem_ = nullptr;
        LinearArena frameArena_;
    };

} // namespace noc
```

---

### `Engine/Runtime/Engine.cpp`

> This file **includes** the Phase 1 subsystem registrations unchanged, then adds the Phase 2 window subsystem and `Run()`.

```cpp
#include "Engine.h"
#include "Core/Log.h"
#include "Core/Assert.h"
#include "Core/Clock.h"
#include "Platform/Win32/WinWindow.h"
#include "Runtime/MainLoop.h"


namespace noc {

	IAllocator& Engine::Allocator() { return *alloc_; }
	LinearArena& Engine::FrameArena() { return frameArena_; }

	bool Engine::InitMemory()
	{
		constexpr std::size_t kFrameArenaBytes = 8 * 1024 * 1024; // Design choice
		frameArenaMem_ = Allocator().Allocate(kFrameArenaBytes, 64);
		frameArena_.Init(frameArenaMem_, kFrameArenaBytes);

		NOC_LOG_INFO("Core", "Memory system initialized (frame arena=%zu bytes)", kFrameArenaBytes);
		return true;
	}

	void Engine::KillMemory()
	{
		// Free frame arena first
		if (frameArenaMem_)
		{
			Allocator().Deallocate(frameArenaMem_);
			frameArenaMem_ = nullptr;
		}

#if NOC_ENABLE_ASSERTS
		NOC_LOG_INFO("Core", "Memory stats: total=%zu outstanding=%zu allocs=%zu",
			debugAlloc_.TotalAllocatedBytes(),
			debugAlloc_.OutstandingBytes(),
			debugAlloc_.AllocationCount());
#endif
	}


	static bool StartupLog(void*)
	{
		noc::GetLogger().Init();
		NOC_LOG_INFO("Core", "Logger initialized");
		return true;
	}

	static void ShutdownLog(void*)
	{
		NOC_LOG_INFO("Core", "Logger shutting down");
		noc::GetLogger().Shutdown();
	}

	static bool StartupTime(void*)
	{
		noc::GetTime().Init();
		NOC_LOG_INFO("Core", "Time system initialized");
		return true;
	}

	static void ShutdownTime(void*)
	{
		NOC_LOG_INFO("Core", "Time system shutting down");
	}

	static bool StartupAssert(void*)
	{
		NOC_LOG_INFO("Core", "Assert system initialized");
		return true;
	}

	static void ShutdownAssert(void*)
	{
		NOC_LOG_INFO("Core", "Assert system shutting down");
	}

	static bool StartupMemory(void* ctx)
	{
		auto* e = static_cast<noc::Engine*>(ctx);
		return e->InitMemory();
	}

	static void ShutdownMemory(void* ctx)
	{
		NOC_LOG_INFO("Core", "Memory system shutting down");
		auto* e = static_cast<noc::Engine*>(ctx);
		e->KillMemory();
	}

	static bool StartupWindow(void* ctx)
	{
		auto* e = static_cast<Engine*>(ctx);
		(void)e; // engine-owned window is created in Engine::Run (not in subsystem) — see note below.
		NOC_LOG_INFO("Win32", "Window subsystem ready");
		return true;
	}

	static void ShutdownWindow(void*)
	{
		NOC_LOG_INFO("Win32", "Window subsystem shutdown");
	}


	bool Engine::Init()
	{
		std::span<const char* const> depsLog{}; // empty: no dependencies

		static const char* kDepsNeedLog[] = { "Log" };
		std::span<const char* const> depsNeedLog{ kDepsNeedLog, 1 };

		registry_.Register(SubsystemDesc{ "Log",    depsLog,     &StartupLog,    &ShutdownLog });
		registry_.Register(SubsystemDesc{ "Time",   depsNeedLog, &StartupTime,   &ShutdownTime });
		registry_.Register(SubsystemDesc{ "Memory", depsNeedLog, &StartupMemory, &ShutdownMemory });
		registry_.Register(SubsystemDesc{ "Assert", depsNeedLog, &StartupAssert, &ShutdownAssert });
		registry_.Register(SubsystemDesc{ "Window", depsNeedLog, &StartupWindow, &ShutdownWindow });


		return registry_.StartupAll(this);
	}

	int Engine::Run()
	{
		// Create the actual native window here (engine-owned lifetime),
		// after subsystems are up, before entering loop.
		// Design choice: explicit ownership in Engine::Run (simplifies future editor/game split).

		WinWindow window;
		WinWindowDesc wd{};
		wd.title = L"NocturneHost";
		wd.width = 1280;
		wd.height = 720;
		wd.resizable = true;

		if (!window.Create(wd))
		{
			NOC_LOG_FATAL("Win32", "Failed to create window");
			return -1;
		}

		MainLoop loop;
		loop.Run(*this, window);

		window.Destroy();
		return 0;
	}

	void Engine::BeginFrame()
	{
		GetTime().BeginFrame();
		FrameArena().Reset();
	}

	void Engine::Tick()
	{
		// Phase 2: intentionally empty.
		// We keep a tiny log sample at low frequency if desired, but avoid per-frame spam.
	}

	void Engine::EndFrame()
	{
		GetTime().EndFrame();
		// Optional: log dt occasionally; do NOT spam every frame.
	}

	void Engine::TickOnce()
	{
		GetTime().BeginFrame();

		FrameArena().Reset();

		// Allocate a few blocks
		void* a = FrameArena().Allocate(256, 16);
		void* b = FrameArena().Allocate(1024, 64);
		(void)a; (void)b;

		GetTime().EndFrame();

		NOC_LOG_INFO("Core", "TickOnce() dt=%.6f sec arenaUsed=%zu bytes",
			GetTime().DeltaSeconds(),
			FrameArena().Used());

	}


	void Engine::Shutdown()
	{
		registry_.ShutdownAll(this);
	}

} // namespace noc
```

---

**End of Phase 2**

---

## What's Next?

Phase 3 will implement:

- Virtual File System
- Loose directory mounts
- ZIP archive mounts (read-only)
- Virtual path namespace
