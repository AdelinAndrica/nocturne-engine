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
