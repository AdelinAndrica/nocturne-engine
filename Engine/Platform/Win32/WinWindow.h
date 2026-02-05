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

		// Client size (updated on WM_SIZE).
		uint32_t ClientWidth() const { return clientW_; }
		uint32_t ClientHeight() const { return clientH_; }

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

		uint32_t clientW_ = 0;
		uint32_t clientH_ = 0;

		platform::IRawInputSink* rawSink_ = nullptr;
	};
}
