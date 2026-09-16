#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace nocturne::editor
{
    // Design choice (not directly from the book): centralized visual tokens keep the
    // Phase 13 native Win32 bootstrap replaceable while giving the editor a coherent skin.
    struct ThemeColors
    {
        COLORREF windowBg;
        COLORREF panelBg;
        COLORREF panelBgAlt;
        COLORREF viewportBg;
        COLORREF toolbarBg;
        COLORREF inputBg;
        COLORREF buttonBg;
        COLORREF buttonHover;
        COLORREF border;
        COLORREF textPrimary;
        COLORREF textMuted;
        COLORREF accent;
        COLORREF accentHover;
        COLORREF success;
        COLORREF warning;
        COLORREF danger;
    };

    struct ThemeMetrics
    {
        int menuHeight = 30;
        int toolbarHeight = 60;
        int statusHeight = 30;
        int panelHeaderHeight = 34;
        int gap = 8;
        int innerPadding = 10;
        int buttonHeight = 40;
        int buttonRadius = 8;
    };

    class EditorTheme
    {
    public:
        static const ThemeColors& Colors();
        static const ThemeMetrics& Metrics();
    };
}
