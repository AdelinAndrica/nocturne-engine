## Phase 7 — Input System (Raw Input + Snapshots + Action Mapping)

> **Status:** READY FOR IMPLEMENTATION ⏳
> **Scope:** Windows Raw Input collection, per-frame buffered snapshot, action mapping (digital + analog), integrated into `Engine::Tick()` / main loop
> **Depends on:** Phase 2 (window + message pump), Phase 4/5 (engine tick update point), Phase 6 optional (jobs unrelated)
>
> **Primary source of truth:** Jason Gregory, *Game Engine Architecture (3rd Ed.)* 
> **Architecture alignment:** “INPUT → InputSystem → ActionMap” + “ProcessOSMessages / BeginFrame / IsDown / MouseDelta / GetActionValue” in the Full Architecture Diagram 
> **Architecture rules:** “Input is polled and buffered once per frame; gameplay consumes buffered data” 

This document is the **authoritative design + implementation reference** for Phase 7 of Nocturne Engine.

---

## 1) Phase name + objective

**Objective:** Add an engine-owned **InputSystem** that:

1. **Collects OS input** using **Win32 Raw Input** (keyboard + mouse) via the existing Win32 window message pump.
2. Produces a **per-frame input snapshot** (current + previous) so gameplay can query:

   * `IsDown()`, `WasPressed()`, `WasReleased()`
   * `MouseDelta()`, `WheelDelta()`
3. Provides an **ActionMap** layer that converts physical inputs into:

   * **Digital actions** (0/1)
   * **Analog actions** (float, e.g., mouse X/Y delta axes)
4. Integrates cleanly into the engine loop:

   * raw messages come in through the window pump
   * `InputSystem::BeginFrame()` runs once per frame
   * `InputSystem` update runs inside `Engine::Tick()` (same place we already “finalize/publish” work like resources).

**Design choice (not directly from the book):** We forward `WM_INPUT` from `WinWindow::WndProc` into `InputSystem` through a small platform-layer sink interface, to avoid platform → input header dependencies while keeping the message pump the single source of OS events.

---

## 2) Key concepts from the books (book-grounded)

* **Engine owns the loop and frame boundaries** (input is sampled once per frame; gameplay reads stable data).
* **Support systems live below gameplay**: platform-specific acquisition is isolated; higher layers see stable interfaces.
* **Deterministic “finalize on main thread” shape**: treat input like other producer/consumer systems—OS produces events, engine publishes a stable per-frame snapshot. (Same shape we used for resources in `Engine::Tick()`.)

---

## 3) What we implement now (tight scope)

### A) Windows raw input acquisition

* Register raw input devices (keyboard + mouse) for the engine window.
* Handle `WM_INPUT` and decode `RAWINPUT` into small internal events.

### B) Per-frame buffered snapshot

* `BeginFrame()`:

  * copies `current → previous`
  * resets per-frame deltas (mouse delta, wheel)
  * clears “pressed/released this frame” transient flags
* Event consumption updates `current` and transient flags.

### C) Action mapping

* `ActionMap` supports bindings:

  * **Digital**: Key, MouseButton
  * **Analog**: MouseDeltaX/Y, MouseWheel
* Query:

  * `GetActionValue(ActionId)` → float
  * `IsActionDown(ActionId)` convenience

### D) Integration

* `MainLoop` sets the raw input sink on the window after creation.
* `Engine::Tick()` calls input begin/update before simulation (simulation comes later).

---

## 4) Implementation steps

1. Add **Input module** scaffolding:

   * `Engine/Input/InputTypes.h`
   * `Engine/Input/ActionMap.h/.cpp`
   * `Engine/Input/InputSystem.h/.cpp`
2. Extend **WinWindow** to forward `WM_INPUT` to an interface sink:

   * `platform::IRawInputSink`
   * `WinWindow::SetRawInputSink()`
3. Register for raw input when the window exists:

   * `InputSystem::AttachToWindow(void* hwnd)` (opaque handle)
4. Integrate into runtime:

   * `Engine` owns `InputSystem input_;`
   * `Engine::Tick()` calls:

     * `input_.BeginFrame();`
     * `input_.Update();`
     * `resources_.Update();` (existing pattern) 
5. Provide a default `ActionMap` with a few example bindings (WASD, mouse look axes).

---

## 5) Verification checklist (Phase 7 done when…)

* [ ] Moving the mouse changes `MouseDelta()` every frame (and resets to 0 when idle).
* [ ] Pressing and releasing a key:

  * `IsDown()` true while held
  * `WasPressed()` true for exactly one frame on press
  * `WasReleased()` true for exactly one frame on release
* [ ] Action map returns:

  * `MoveForward` is 1.0 while `W` held
  * `LookX` changes with mouse movement
* [ ] Engine shuts down cleanly (no dangling sink pointer on window destroy).

---

## 6) Common pitfalls

* Forgetting to reset per-frame deltas (mouse delta “sticks”).
* Treating raw input events as the authoritative “current state” without buffering (gameplay reads mid-frame changes).
* Mishandling focus loss (keys can get “stuck down”).
  **We handle this by clearing state on focus loss.**

---

## 7) Next chat handoff (ONLY what you should say/bring next)

> “Phase 7 InputSystem is implemented. Raw Input is wired through WinWindow WM_INPUT, InputSystem snapshots per frame, ActionMap returns digital+analog values. Here are logs showing key press, mouse delta, and action values. Start Phase 8 — Rendering Bootstrap.”

---

# C++ Implementations (Phase 7)

Below are **full implementations** for the Input subsystem + the required engine/platform integrations.

---

## `Engine/Input/InputTypes.h`

```cpp
#pragma once
#include <cstdint>

namespace noc
{
	// Keep public headers STL-free (project rule in architecture doc). :contentReference[oaicite:11]{index=11}

	// --- Keys (minimal set to start; extend as needed) ---
	enum class Key : uint16_t
	{
		Unknown = 0,

		Escape,

		W, A, S, D,
		Q, E,
		Space,
		LeftShift,
		LeftCtrl,

		MouseLeft,
		MouseRight,
		MouseMiddle,

		Count
	};

	struct MouseDelta
	{
		int dx = 0;
		int dy = 0;
	};

	// 32-bit stable ID for actions (hash of name).
	using ActionId = uint32_t;

	inline constexpr ActionId InvalidAction = 0;

	// Simple FNV-1a hash for action names (compile-time friendly if needed later).
	inline constexpr ActionId HashActionName(const char* s)
	{
		uint32_t h = 2166136261u;
		if (!s) return 0;
		while (*s)
		{
			h ^= static_cast<uint8_t>(*s++);
			h *= 16777619u;
		}
		return h;
	}
}
```

---

## `Engine/Input/ActionMap.h`

```cpp
#pragma once
#include "InputTypes.h"
#include <cstdint>

namespace noc
{
	class InputSystem;

	// Small, fixed-capacity action map (no STL in public header).
	class ActionMap
	{
	public:
		enum class SourceType : uint8_t
		{
			DigitalKey = 0,
			DigitalMouseButton,

			AnalogMouseDeltaX,
			AnalogMouseDeltaY,
			AnalogMouseWheel
		};

		struct Binding
		{
			ActionId    action = InvalidAction;
			SourceType  type = SourceType::DigitalKey;

			// For DigitalKey / DigitalMouseButton.
			Key         key = Key::Unknown;

			// Scale for analog sources.
			float       scale = 1.0f;

			// For digital, optional: treat as "negative" contribution (e.g., S on MoveForward).
			bool        negate = false;
		};

	public:
		void Clear();

		// Returns false if capacity exceeded.
		bool Bind(const Binding& b);

		// Rebind by action + source type (simple, first match). Returns true if replaced.
		bool Rebind(ActionId action, SourceType type, const Binding& replacement);

		// Call once per frame after InputSystem has updated its snapshot.
		void Update(const InputSystem& input);

		// Query
		float GetActionValue(ActionId action) const;
		bool  IsActionDown(ActionId action) const { return GetActionValue(action) != 0.0f; }

	private:
		static constexpr uint32_t kMaxBindings = 64;
		static constexpr uint32_t kMaxActionsCached = 64;

		Binding bindings_[kMaxBindings]{};
		uint32_t bindingCount_ = 0;

		// Cached computed values (per frame).
		struct ActionValue
		{
			ActionId action = InvalidAction;
			float    value = 0.0f;
		};
		ActionValue values_[kMaxActionsCached]{};
		uint32_t valueCount_ = 0;

	private:
		void SetValue_(ActionId action, float v);
		float GetValue_(ActionId action) const;
	};
}
```

---

## `Engine/Input/ActionMap.cpp`

```cpp
#include "ActionMap.h"
#include "InputSystem.h"
#include <cmath>

namespace noc
{
	void ActionMap::Clear()
	{
		bindingCount_ = 0;
		valueCount_ = 0;
	}

	bool ActionMap::Bind(const Binding& b)
	{
		if (bindingCount_ >= kMaxBindings)
			return false;
		bindings_[bindingCount_++] = b;
		return true;
	}

	bool ActionMap::Rebind(ActionId action, SourceType type, const Binding& replacement)
	{
		for (uint32_t i = 0; i < bindingCount_; ++i)
		{
			if (bindings_[i].action == action && bindings_[i].type == type)
			{
				bindings_[i] = replacement;
				return true;
			}
		}
		return false;
	}

	void ActionMap::Update(const InputSystem& input)
	{
		// Clear cached values.
		valueCount_ = 0;

		for (uint32_t i = 0; i < bindingCount_; ++i)
		{
			const Binding& b = bindings_[i];
			if (b.action == InvalidAction)
				continue;

			float v = 0.0f;

			switch (b.type)
			{
			case SourceType::DigitalKey:
			case SourceType::DigitalMouseButton:
			{
				const bool down = input.IsDown(b.key);
				v = down ? 1.0f : 0.0f;
				break;
			}

			case SourceType::AnalogMouseDeltaX:
			{
				const MouseDelta md = input.GetMouseDelta();
				v = static_cast<float>(md.dx) * b.scale;
				break;
			}
			case SourceType::AnalogMouseDeltaY:
			{
				const MouseDelta md = input.GetMouseDelta();
				v = static_cast<float>(md.dy) * b.scale;
				break;
			}
			case SourceType::AnalogMouseWheel:
			{
				v = static_cast<float>(input.GetWheelDelta()) * b.scale;
				break;
			}
			default:
				break;
			}

			if (b.negate)
				v = -v;

			// Accumulate contributions (e.g., W + S bindings).
			const float prev = GetValue_(b.action);
			SetValue_(b.action, prev + v);
		}

		// Optional: clamp near-zero to 0 for stability
		for (uint32_t i = 0; i < valueCount_; ++i)
		{
			if (std::fabs(values_[i].value) < 1e-6f)
				values_[i].value = 0.0f;
		}
	}

	float ActionMap::GetActionValue(ActionId action) const
	{
		return GetValue_(action);
	}

	void ActionMap::SetValue_(ActionId action, float v)
	{
		for (uint32_t i = 0; i < valueCount_; ++i)
		{
			if (values_[i].action == action)
			{
				values_[i].value = v;
				return;
			}
		}

		if (valueCount_ < kMaxActionsCached)
		{
			values_[valueCount_++] = ActionValue{ action, v };
		}
	}

	float ActionMap::GetValue_(ActionId action) const
	{
		for (uint32_t i = 0; i < valueCount_; ++i)
		{
			if (values_[i].action == action)
				return values_[i].value;
		}
		return 0.0f;
	}
}
```

---

## `Engine/Input/InputSystem.h`

```cpp
#pragma once
#include "InputTypes.h"
#include "ActionMap.h"

namespace noc
{
	namespace platform
	{
		// Platform-side sink interface is declared in WinWindow.h (platform module).
		struct RawMouseEvent;
		struct RawKeyboardEvent;
		struct IRawInputSink;
	}

	class Engine;

	class InputSystem final : public ActionMap
	{
	public:
		InputSystem() = default;

		// Engine-owned lifecycle (init does not require HWND; attach does).
		bool Init(Engine& engine);
		void Shutdown();

		// Call once after window is created (in Engine::Run / MainLoop setup).
		bool AttachToWindow(void* nativeHwnd);
		void DetachFromWindow();

		// Frame lifecycle
		void BeginFrame();
		void Update(); // consumes queued raw events and updates snapshot + action values

		// Queries
		bool IsDown(Key k) const;
		bool WasPressed(Key k) const;
		bool WasReleased(Key k) const;

		MouseDelta GetMouseDelta() const { return mouseDelta_; }
		int        GetWheelDelta() const { return wheelDelta_; }

		// Action queries (ActionMap methods)
		float GetActionValue(ActionId action) const { return ActionMap::GetActionValue(action); }

		// Platform hook: returns sink pointer that WinWindow will call.
		platform::IRawInputSink* RawSink();

		// Convenience: create a default FPS-ish binding set.
		void BuildDefaultBindings();

		// Call on focus loss to prevent stuck keys.
		void ClearAllState();

	private:
		Engine* engine_ = nullptr;
		void* hwnd_ = nullptr;

		// Snapshot states
		static constexpr uint32_t kKeyCount = static_cast<uint32_t>(Key::Count);

		bool curr_[kKeyCount]{};
		bool prev_[kKeyCount]{};

		MouseDelta mouseDelta_{};
		int wheelDelta_ = 0;

		// Raw event queue (implementation hidden in .cpp)
		struct Impl;
		Impl* impl_ = nullptr;

	private:
		void ApplyKey_(Key k, bool down);
	};
}
```

> **Note:** `InputSystem` “is-a” `ActionMap` here to keep public surface small.
> **Design choice (not directly from the book):** you may later prefer composition (`InputSystem` owns an `ActionMap`) once you have multiple maps/profiles.

---

## `Engine/Input/InputSystem.cpp`

```cpp
#include "InputSystem.h"
#include "Runtime/Engine.h"
#include "Core/Log.h"
#include "Core/Assert.h"

#include "Platform/Win32/WinWindow.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <hidusage.h>
#include <vector>

namespace noc
{
	struct InputSystem::Impl final : public platform::IRawInputSink
	{
		std::vector<platform::RawMouseEvent> mouseEvents;
		std::vector<platform::RawKeyboardEvent> keyEvents;

		InputSystem* owner = nullptr;

		void OnRawMouse(const platform::RawMouseEvent& e) override
		{
			mouseEvents.push_back(e);
		}

		void OnRawKeyboard(const platform::RawKeyboardEvent& e) override
		{
			keyEvents.push_back(e);
		}

		void OnFocusLost() override
		{
			owner->ClearAllState();
		}
	};

	// --- Key translation (minimal, extend later) ---
	static Key TranslateVKey(uint16_t vk)
	{
		switch (vk)
		{
		case VK_ESCAPE: return Key::Escape;
		case 'W': return Key::W;
		case 'A': return Key::A;
		case 'S': return Key::S;
		case 'D': return Key::D;
		case 'Q': return Key::Q;
		case 'E': return Key::E;
		case VK_SPACE: return Key::Space;
		case VK_LSHIFT: return Key::LeftShift;
		case VK_LCONTROL: return Key::LeftCtrl;
		default: return Key::Unknown;
		}
	}

	bool InputSystem::Init(Engine& engine)
	{
		engine_ = &engine;
		impl_ = new Impl();
		impl_->owner = this;

		BuildDefaultBindings();

		NOC_LOG_INFO("Input", "InputSystem initialized");
		return true;
	}

	void InputSystem::Shutdown()
	{
		DetachFromWindow();

		delete impl_;
		impl_ = nullptr;
		engine_ = nullptr;

		NOC_LOG_INFO("Input", "InputSystem shutdown");
	}

	bool InputSystem::AttachToWindow(void* nativeHwnd)
	{
		hwnd_ = nativeHwnd;
		if (!hwnd_)
			return false;

		RAWINPUTDEVICE rid[2]{};

		// Mouse
		rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
		rid[0].usUsage = HID_USAGE_GENERIC_MOUSE;
		rid[0].dwFlags = RIDEV_INPUTSINK; // receive even if not focused (optional); we still clear on focus lost
		rid[0].hwndTarget = (HWND)hwnd_;

		// Keyboard
		rid[1].usUsagePage = HID_USAGE_PAGE_GENERIC;
		rid[1].usUsage = HID_USAGE_GENERIC_KEYBOARD;
		rid[1].dwFlags = RIDEV_INPUTSINK;
		rid[1].hwndTarget = (HWND)hwnd_;

		if (!RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE)))
		{
			NOC_LOG_ERROR("Input", "RegisterRawInputDevices failed (err=%lu)", GetLastError());
			return false;
		}

		NOC_LOG_INFO("Input", "Raw input registered");
		return true;
	}

	void InputSystem::DetachFromWindow()
	{
		hwnd_ = nullptr;
	}

	void InputSystem::BeginFrame()
	{
		// Snapshot copy
		for (uint32_t i = 0; i < kKeyCount; ++i)
			prev_[i] = curr_[i];

		// Reset deltas
		mouseDelta_ = {};
		wheelDelta_ = 0;
	}

	void InputSystem::Update()
	{
		if (!impl_)
			return;

		// Consume keyboard events
		for (const auto& e : impl_->keyEvents)
		{
			const Key k = TranslateVKey(e.vkey);
			if (k == Key::Unknown)
				continue;
			ApplyKey_(k, e.down);
		}
		impl_->keyEvents.clear();

		// Consume mouse events
		for (const auto& e : impl_->mouseEvents)
		{
			mouseDelta_.dx += e.dx;
			mouseDelta_.dy += e.dy;
			wheelDelta_ += e.wheel;

			if (e.buttonDownMask & 0x1) ApplyKey_(Key::MouseLeft, true);
			if (e.buttonDownMask & 0x2) ApplyKey_(Key::MouseRight, true);
			if (e.buttonDownMask & 0x4) ApplyKey_(Key::MouseMiddle, true);

			if (e.buttonUpMask & 0x1) ApplyKey_(Key::MouseLeft, false);
			if (e.buttonUpMask & 0x2) ApplyKey_(Key::MouseRight, false);
			if (e.buttonUpMask & 0x4) ApplyKey_(Key::MouseMiddle, false);
		}
		impl_->mouseEvents.clear();

		// Update action values from current snapshot.
		ActionMap::Update(*this);
	}

	bool InputSystem::IsDown(Key k) const
	{
		const uint32_t idx = static_cast<uint32_t>(k);
		if (idx >= kKeyCount)
			return false;
		return curr_[idx];
	}

	bool InputSystem::WasPressed(Key k) const
	{
		const uint32_t idx = static_cast<uint32_t>(k);
		if (idx >= kKeyCount)
			return false;
		return curr_[idx] && !prev_[idx];
	}

	bool InputSystem::WasReleased(Key k) const
	{
		const uint32_t idx = static_cast<uint32_t>(k);
		if (idx >= kKeyCount)
			return false;
		return !curr_[idx] && prev_[idx];
	}

	platform::IRawInputSink* InputSystem::RawSink()
	{
		return impl_;
	}

	void InputSystem::BuildDefaultBindings()
	{
		Clear();

		// Typical FPS movement: forward/back on one action (W adds +1, S adds -1).
		Bind(ActionMap::Binding{ HashActionName("MoveForward"), ActionMap::SourceType::DigitalKey, Key::W, 1.0f, false });
		Bind(ActionMap::Binding{ HashActionName("MoveForward"), ActionMap::SourceType::DigitalKey, Key::S, 1.0f, true });

		Bind(ActionMap::Binding{ HashActionName("MoveRight"), ActionMap::SourceType::DigitalKey, Key::D, 1.0f, false });
		Bind(ActionMap::Binding{ HashActionName("MoveRight"), ActionMap::SourceType::DigitalKey, Key::A, 1.0f, true });

		Bind(ActionMap::Binding{ HashActionName("Jump"), ActionMap::SourceType::DigitalKey, Key::Space, 1.0f, false });

		// Mouse look axes (scaled).
		Bind(ActionMap::Binding{ HashActionName("LookX"), ActionMap::SourceType::AnalogMouseDeltaX, Key::Unknown, 0.01f, false });
		Bind(ActionMap::Binding{ HashActionName("LookY"), ActionMap::SourceType::AnalogMouseDeltaY, Key::Unknown, 0.01f, false });

		NOC_LOG_INFO("Input", "Default action bindings created");
	}

	void InputSystem::ClearAllState()
	{
		for (uint32_t i = 0; i < kKeyCount; ++i)
		{
			curr_[i] = false;
			prev_[i] = false;
		}
		mouseDelta_ = {};
		wheelDelta_ = 0;
	}

	void InputSystem::ApplyKey_(Key k, bool down)
	{
		const uint32_t idx = static_cast<uint32_t>(k);
		if (idx >= kKeyCount)
			return;
		curr_[idx] = down;
	}
}
```

---

## `Engine/Platform/Win32/WinWindow.h` (UPDATED)

```cpp
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstdint>

namespace noc
{
	namespace platform
	{
		struct RawMouseEvent
		{
			int dx = 0;
			int dy = 0;
			int wheel = 0;
			uint16_t buttonDownMask = 0; // bit0=L, bit1=R, bit2=M
			uint16_t buttonUpMask = 0;
		};

		struct RawKeyboardEvent
		{
			uint16_t vkey = 0;
			bool down = false;
		};

		struct IRawInputSink
		{
			virtual ~IRawInputSink() = default;
			virtual void OnRawMouse(const RawMouseEvent& e) = 0;
			virtual void OnRawKeyboard(const RawKeyboardEvent& e) = 0;
			virtual void OnFocusLost() = 0;
		};
	}

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

		bool ShouldQuit() const { return shouldQuit_; }
		void RequestQuit() { shouldQuit_ = true; }

		void* Handle() const { return (void*)hwnd_; }

		// Phase 7: forward raw input to engine input system through an abstract sink.
		void SetRawInputSink(platform::IRawInputSink* sink) { rawSink_ = sink; }

	private:
		static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

		void RegisterClassOnce();
		void ApplyClientSize(int clientW, int clientH, bool resizable);

		static void DecodeRawInput_(HRAWINPUT hRawInput, WinWindow* win);
	private:
		HWND hwnd_ = nullptr;
		bool shouldQuit_ = false;

		platform::IRawInputSink* rawSink_ = nullptr;
	};
}
```

---

## `Engine/Platform/Win32/WinWindow.cpp` (UPDATED: WM_INPUT + focus)

```cpp
#include <malloc.h>
#include "WinWindow.h"
#include "Core/Log.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace noc
{
	static const wchar_t* kWndClassName = L"NocturneWndClass";

	void WinWindow::RegisterClassOnce()
	{
		static bool registered = false;
		if (registered)
			return;

		WNDCLASSEXW wc{};
		wc.cbSize = sizeof(wc);
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = &WinWindow::WndProc;
		wc.hInstance = GetModuleHandleW(nullptr);
		wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
		wc.lpszClassName = kWndClassName;

		if (!RegisterClassExW(&wc))
		{
			NOC_LOG_FATAL("Win32", "RegisterClassExW failed (err=%lu)", GetLastError());
		}

		registered = true;
	}

	void WinWindow::ApplyClientSize(int clientW, int clientH, bool resizable)
	{
		DWORD style = WS_OVERLAPPEDWINDOW;
		if (!resizable)
			style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);

		RECT r{ 0, 0, clientW, clientH };
		AdjustWindowRect(&r, style, FALSE);

		SetWindowLongPtrW(hwnd_, GWL_STYLE, style);
		SetWindowPos(hwnd_, nullptr, 0, 0, r.right - r.left, r.bottom - r.top,
			SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
	}

	bool WinWindow::Create(const WinWindowDesc& desc)
	{
		RegisterClassOnce();

		const DWORD style = desc.resizable ? WS_OVERLAPPEDWINDOW : (WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX));

		hwnd_ = CreateWindowExW(
			0,
			kWndClassName,
			desc.title,
			style,
			CW_USEDEFAULT, CW_USEDEFAULT,
			desc.width, desc.height,
			nullptr, nullptr,
			GetModuleHandleW(nullptr),
			this);

		if (!hwnd_)
		{
			NOC_LOG_FATAL("Win32", "CreateWindowExW failed (err=%lu)", GetLastError());
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
		rawSink_ = nullptr; // avoid dangling sink
		if (hwnd_)
		{
			DestroyWindow(hwnd_);
			hwnd_ = nullptr;
		}
	}

	void WinWindow::DecodeRawInput_(HRAWINPUT hRawInput, WinWindow* win)
	{
		if (!win) return;

		UINT size = 0;
		GetRawInputData(hRawInput, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
		if (!size) return;

		uint8_t stackBuf[512];
		uint8_t* buf = stackBuf;

		if (size > sizeof(stackBuf))
			buf = (uint8_t*)_alloca(size);

		if (GetRawInputData(hRawInput, RID_INPUT, buf, &size, sizeof(RAWINPUTHEADER)) != size)
			return;

		const RAWINPUT* ri = reinterpret_cast<const RAWINPUT*>(buf);

		if (ri->header.dwType == RIM_TYPEMOUSE)
		{
			const RAWMOUSE& m = ri->data.mouse;
			platform::RawMouseEvent e{};
			e.dx = m.lLastX;
			e.dy = m.lLastY;

			if (m.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)  e.buttonDownMask |= 0x1;
			if (m.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)    e.buttonUpMask |= 0x1;
			if (m.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN) e.buttonDownMask |= 0x2;
			if (m.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)   e.buttonUpMask |= 0x2;
			if (m.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN) e.buttonDownMask |= 0x4;
			if (m.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)   e.buttonUpMask |= 0x4;

			if (m.usButtonFlags & RI_MOUSE_WHEEL)
				e.wheel = (short)m.usButtonData;

			if (win->rawSink_) win->rawSink_->OnRawMouse(e);
		}
		else if (ri->header.dwType == RIM_TYPEKEYBOARD)
		{
			const RAWKEYBOARD& k = ri->data.keyboard;

			platform::RawKeyboardEvent e{};
			e.vkey = (uint16_t)k.VKey;
			e.down = (k.Flags & RI_KEY_BREAK) == 0;

			if (win->rawSink_) win->rawSink_->OnRawKeyboard(e);
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

		case WM_INPUT:
			if (win)
				DecodeRawInput_((HRAWINPUT)lParam, win);
			return 0;

		case WM_KILLFOCUS:
			if (win && win->rawSink_)
				win->rawSink_->OnFocusLost();
			return 0;

		default:
			return DefWindowProcW(hWnd, msg, wParam, lParam);
		}
	}
}
```

---

## `Engine/Runtime/Engine.h` (UPDATED: owns InputSystem)

```cpp
#pragma once
#include "Core/BuildConfig.h"
#include "Core/Subsystems/SubsystemRegistry.h"
#include "Core/Memory/Allocator.h"
#include "Core/Memory/LinearArena.h"

#if NOC_ENABLE_ASSERTS
#include "Core/Memory/DebugAlloc.h"
#endif

#include "Resources/VirtualFileSystem.h"
#include "Resources/ResourceManager.h"
#include "EngineConfig.h"
#include "Core/Jobs/JobSystem.h"

#include "Input/InputSystem.h"


namespace noc {

    class WinWindow;
    class MainLoop;

    class Engine
    {
    public:
        // Config access: only valid BEFORE Init().
        EngineConfig& ConfigMutable();
        const EngineConfig& Config() const { return cfg_; }

        bool SetContentRoot(const char* path);
        bool SetOverrideRoot(const char* path);
        bool SetArchivePath(const char* path);

        bool Init();
        void TickOnce();
        void Shutdown();

        int Run();
        void BeginFrame();
        void Tick();
        void EndFrame();

        IAllocator& Allocator();
        LinearArena& FrameArena();

        VirtualFileSystem& VFS() { return vfs_; }
        ResourceManager& Resources() { return resources_; }
        const ResourceManager& Resources() const { return resources_; }

		InputSystem& Input() { return input_; }
		const InputSystem& Input() const { return input_; }

        JobSystem& Jobs() { return jobs_; }
        const JobSystem& Jobs() const { return jobs_; }

        bool InitMemory();
        void KillMemory();

		bool AttachWindow(WinWindow& window);

    private:
        bool IsConfigMutable() const { return !initialized_; }

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

        VirtualFileSystem vfs_;
        JobSystem jobs_;
        ResourceManager resources_;
        InputSystem input_;


        EngineConfig cfg_{};
        bool initialized_ = false;
    };

} // namespace noc
```

---

## `Engine/Runtime/Engine.cpp` (UPDATED: Input init + tick integration)

```cpp
#include "Engine.h"

#include <algorithm>
#include <span>

#include "Core/Assert.h"
#include "Core/Log.h"
#include "Core/Clock.h"

#include "Platform/Win32/WinWindow.h"
#include "Runtime/MainLoop.h"

namespace noc {

    IAllocator& Engine::Allocator() { return *alloc_; }
    LinearArena& Engine::FrameArena() { return frameArena_; }

    EngineConfig& Engine::ConfigMutable()
    {
        if (initialized_)
        {
            NOC_LOG_ERROR("Runtime", "EngineConfig is frozen after Init(). Modify config before calling Init().");
            return cfg_;
        }
        return cfg_;
    }

    bool Engine::SetContentRoot(const char* path)
    {
        if (!IsConfigMutable())
        {
            NOC_LOG_ERROR("Runtime", "SetContentRoot() called after Init(); ignored.");
            return false;
        }
        cfg_.contentRoot = path;
        return true;
    }

    bool Engine::SetOverrideRoot(const char* path)
    {
        if (!IsConfigMutable())
        {
            NOC_LOG_ERROR("Runtime", "SetOverrideRoot() called after Init(); ignored.");
            return false;
        }
        cfg_.overrideRoot = path;
        return true;
    }

    bool Engine::SetArchivePath(const char* path)
    {
        if (!IsConfigMutable())
        {
            NOC_LOG_ERROR("Runtime", "SetArchivePath() called after Init(); ignored.");
            return false;
        }
        cfg_.archivePath = path;
        return true;
    }

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
        (void)e;
        NOC_LOG_INFO("Win32", "Window subsystem ready");
        return true;
    }

    static void ShutdownWindow(void*)
    {
        NOC_LOG_INFO("Win32", "Window subsystem shutdown");
    }

    static bool StartupJobs(void* ctx)
    {
        auto* e = static_cast<noc::Engine*>(ctx);
        // Design choice: worker count = HW threads - 1 (leave room for main thread), clamped.
        const uint32_t hw = (std::max)(1u, std::thread::hardware_concurrency());
        const uint32_t workers = (hw > 1) ? (hw - 1) : 1;
        return e->Jobs().Init(workers);
    }

    static void ShutdownJobs(void* ctx)
    {
        auto* e = static_cast<noc::Engine*>(ctx);
        e->Jobs().Shutdown();
    }

    bool Engine::Init()
    {
        std::span<const char* const> depsLog{};

        static const char* kDepsNeedLog[] = { "Log" };
        std::span<const char* const> depsNeedLog{ kDepsNeedLog, 1 };

        static const char* kDepsJobs[] = { "Log", "Memory" };
        std::span<const char* const> depsJobs{ kDepsJobs, 2 };

        registry_.Register(SubsystemDesc{ "Log",    depsLog,     &StartupLog,    &ShutdownLog });
        registry_.Register(SubsystemDesc{ "Time",   depsNeedLog, &StartupTime,   &ShutdownTime });
        registry_.Register(SubsystemDesc{ "Memory", depsNeedLog, &StartupMemory, &ShutdownMemory });
        registry_.Register(SubsystemDesc{ "Assert", depsNeedLog, &StartupAssert, &ShutdownAssert });
        registry_.Register(SubsystemDesc{ "Window", depsNeedLog, &StartupWindow, &ShutdownWindow });
        registry_.Register(SubsystemDesc{ "Jobs",   depsJobs,    &StartupJobs,   &ShutdownJobs });

        if (!registry_.StartupAll(this))
            return false;

        // Phase 3 policy: later mounts override earlier mounts.
        if (cfg_.archivePath && cfg_.archivePath[0] != 0)
        {
            if (!vfs_.MountArchive(cfg_.archivePath))
                NOC_LOG_WARN("VFS", "Failed to mount archivePath: %s", cfg_.archivePath);
        }

        if (cfg_.contentRoot && cfg_.contentRoot[0] != 0)
        {
            if (!vfs_.MountLooseDirectory(cfg_.contentRoot))
                NOC_LOG_WARN("VFS", "Failed to mount contentRoot: %s", cfg_.contentRoot);
        }

        if (cfg_.overrideRoot && cfg_.overrideRoot[0] != 0)
        {
            if (!vfs_.MountLooseDirectory(cfg_.overrideRoot))
                NOC_LOG_WARN("VFS", "Failed to mount overrideRoot: %s", cfg_.overrideRoot);
        }

        // Phase 6: ResourceManager uses JobSystem (must be after jobs + VFS mounts).
        if (!resources_.Init(*this, vfs_))
            return false;

		// Phase 7: InputSystem init (HWND comes later in AttachWindow).
		if (!input_.Init(*this))
			return false;

        initialized_ = true;
        return true;
    }

    bool Engine::AttachWindow(WinWindow& window)
    {
        // Forward WM_INPUT + focus loss -> InputSystem.
        window.SetRawInputSink(input_.RawSink());

        // Register Raw Input devices (requires HWND).
        if (!input_.AttachToWindow(window.Handle()))
            return false;

        return true;
    }

    int Engine::Run()
    {
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

        if (!AttachWindow(window))
        {
            NOC_LOG_FATAL("Runtime", "Failed to attach InputSystem to window");
            window.Destroy();
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
		input_.BeginFrame();
    }

    void Engine::Tick()
    {
		input_.Update();
        resources_.Update();
    }

    void Engine::EndFrame()
    {
        GetTime().EndFrame();
    }

    void Engine::TickOnce()
    {
        GetTime().BeginFrame();
        FrameArena().Reset();

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
        // Resource manager must shutdown while jobs + memory + log still exist.
        resources_.Shutdown();
		input_.Shutdown();
        registry_.ShutdownAll(this);
    }

} // namespace noc
```

---

## `Engine/Runtime/MainLoop.h` (unchanged structure; shown for completeness)

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
	};
}
```

---

## `Engine/Runtime/MainLoop.cpp` (unchanged; shown for completeness)

```cpp
#include "MainLoop.h"
#include "Runtime/Engine.h"
#include "Platform/Win32/WinWindow.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace noc
{
	static void PumpMessagesNonBlocking()
	{
		MSG msg{};
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	void MainLoop::Run(Engine& engine, WinWindow& window)
	{
		while (!window.ShouldQuit())
		{
			PumpMessagesNonBlocking();

			engine.BeginFrame();
			engine.Tick();
			engine.EndFrame();
		}
	}
}
```

---

## `Apps/NocturneHost/main.cpp`
```cpp
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
```
---

If you want, in the next message paste a short log snippet while pressing **W** and moving the mouse, and I’ll tell you exactly what to print (and where) to prove the snapshot + action map are behaving correctly.
