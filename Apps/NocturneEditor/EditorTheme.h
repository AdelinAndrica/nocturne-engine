#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace nocturne::editor
{
    /**
     * @brief Centralized editor color tokens.
     *
     * These values define native editor presentation only; they are not gameplay UI
     * colors and are not an engine-runtime dependency.
     *
     * @ingroup editor
     */
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

    /**
     * @brief Shared baseline layout metrics for native editor controls.
     *
     * @note EditorShellV3 may compute/override concrete layout values for the active
     * shell. Treat this structure as centralized defaults/tokens, not as a promise
     * that every pixel dimension in the shell is read from here.
     *
     * @ingroup editor
     */
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

    /**
     * @brief Read-only access point for Nocturne Editor visual tokens.
     *
     * @par When to use
     * Native editor widgets/chrome should query EditorTheme rather than duplicating
     * palette constants.
     *
     * @par Do not use for
     * Do not introduce EditorTheme into Engine runtime/game UI code.
     *
     * @ingroup editor
     */
    class EditorTheme
    {
    public:
        /** @brief Returns process-lifetime editor color tokens. */
        static const ThemeColors& Colors();

        /** @brief Returns process-lifetime editor metric defaults. */
        static const ThemeMetrics& Metrics();
    };
}
