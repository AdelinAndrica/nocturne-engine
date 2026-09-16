#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace nocturne::editor
{
    // Design choice (not directly from the book): centralized visual tokens keep the
    // Phase 13 tooling UI coherent and replaceable without leaking into runtime UI.
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
        int menuHeight = 28;
        int toolbarHeight = 52;
        int statusHeight = 26;
        int panelHeaderHeight = 31;
        int gap = 8;
        int innerPadding = 9;
        int buttonHeight = 36;
        int buttonRadius = 6;
        int inputRadius = 6;
        int scrollbarWidth = 9;
        int tableHeaderHeight = 28;
        int tableRowHeight = 26;
        int treeRowHeight = 25;
        int compactRowHeight = 31;
    };

    class EditorTheme
    {
    public:
        static const ThemeColors& Colors();
        static const ThemeMetrics& Metrics();
    };
}
