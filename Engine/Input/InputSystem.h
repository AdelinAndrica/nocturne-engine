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
