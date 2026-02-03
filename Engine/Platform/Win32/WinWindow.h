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
}
