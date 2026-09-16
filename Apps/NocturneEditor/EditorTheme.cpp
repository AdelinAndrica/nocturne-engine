#include "EditorTheme.h"

namespace nocturne::editor
{
    namespace
    {
        const ThemeColors kColors{
            RGB(10, 15, 22),    // windowBg
            RGB(16, 23, 32),    // panelBg
            RGB(20, 28, 39),    // panelBgAlt
            RGB(13, 22, 33),    // viewportBg
            RGB(14, 21, 30),    // toolbarBg
            RGB(12, 19, 28),    // inputBg
            RGB(21, 31, 45),    // buttonBg
            RGB(27, 40, 58),    // buttonHover
            RGB(34, 47, 63),    // border
            RGB(222, 230, 240), // textPrimary
            RGB(132, 149, 171), // textMuted
            RGB(25, 132, 236),  // accent
            RGB(42, 148, 250),  // accentHover
            RGB(78, 205, 112),  // success
            RGB(226, 166, 67),  // warning
            RGB(232, 86, 103)   // danger
        };

        const ThemeMetrics kMetrics{};
    }

    const ThemeColors& EditorTheme::Colors()
    {
        return kColors;
    }

    const ThemeMetrics& EditorTheme::Metrics()
    {
        return kMetrics;
    }
}
