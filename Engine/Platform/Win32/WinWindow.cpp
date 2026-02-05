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
		{
			style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
		}

		RECT r{ 0, 0, clientW, clientH };
		AdjustWindowRect(&r, style, FALSE);

		const int winW = r.right - r.left;
		const int winH = r.bottom - r.top;

		SetWindowLongPtrW(hwnd_, GWL_STYLE, (LONG_PTR)style);
		SetWindowPos(hwnd_, nullptr, 100, 100, winW, winH, SWP_NOZORDER | SWP_FRAMECHANGED);
	}

	bool WinWindow::Create(const WinWindowDesc& desc)
	{
		RegisterClassOnce();

		HINSTANCE hInst = GetModuleHandleW(nullptr);

		hwnd_ = CreateWindowExW(
			0,
			kWndClassName,
			desc.title,
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT,
			desc.width, desc.height,
			nullptr, nullptr,
			hInst,
			this
		);

		if (!hwnd_)
		{
			NOC_LOG_FATAL("Win32", "CreateWindowExW failed (err=%lu)", GetLastError());
			return false;
		}

		ApplyClientSize(desc.width, desc.height, desc.resizable);

		clientW_ = (uint32_t)desc.width;
		clientH_ = (uint32_t)desc.height;

		ShowWindow(hwnd_, SW_SHOW);
		UpdateWindow(hwnd_);

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

		case WM_SIZE:
			if (win)
			{
				const UINT w = LOWORD(lParam);
				const UINT h = HIWORD(lParam);
				win->clientW_ = (uint32_t)w;
				win->clientH_ = (uint32_t)h;
			}
			return 0;

		default:
			return DefWindowProcW(hWnd, msg, wParam, lParam);
		}
	}
}
