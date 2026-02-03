#include "WinWindow.h"
#include "Core/Log.h"
#include "Core/Assert.h"

namespace noc
{
    static const wchar_t* kWndClassName = L"NocturneWindowClass";

    bool WinWindow::RegisterClassOnce()
    {
        if (classRegistered_) return true;

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
        if (resizable) style |= WS_THICKFRAME | WS_MAXIMIZEBOX;

        RECT r{ 0, 0, clientW, clientH };
        AdjustWindowRect(&r, style, FALSE);
        const int winW = r.right - r.left;
        const int winH = r.bottom - r.top;

        SetWindowLongPtrW(hwnd_, GWL_STYLE, (LONG_PTR)style);
        SetWindowPos(hwnd_, nullptr, 100, 100, winW, winH, SWP_NOZORDER | SWP_FRAMECHANGED);
    }

    bool WinWindow::Create(const WinWindowDesc& desc)
    {
        if (!RegisterClassOnce()) return false;

        HINSTANCE hInst = GetModuleHandleW(nullptr);

        // Create with a temporary style; ApplyClientSize() will correct it.
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
}
