#include "EditorTheme.h"

namespace nocturne::editor
{
    namespace
    {
        const ThemeColors kColors{
            RGB(11, 17, 24),    // windowBg
            RGB(17, 25, 35),    // panelBg
            RGB(21, 31, 43),    // panelBgAlt
            RGB(15, 25, 37),    // viewportBg
            RGB(15, 23, 33),    // toolbarBg
            RGB(13, 21, 31),    // inputBg
            RGB(24, 35, 51),    // buttonBg
            RGB(32, 48, 70),    // buttonHover
            RGB(39, 54, 73),    // border
            RGB(231, 238, 247), // textPrimary
            RGB(143, 161, 183), // textMuted
            RGB(22, 138, 251),  // accent
            RGB(46, 155, 255),  // accentHover
            RGB(77, 213, 106),  // success
            RGB(246, 184, 74),  // warning
            RGB(255, 92, 108)   // danger
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
