#pragma once

namespace noc::app
{
    struct AppConfig
    {
        const wchar_t* windowTitle = L"NocturneHost";
        int windowWidth = 1280;
        int windowHeight = 720;
        bool resizable = true;

        // NEW (Phase 3): where to mount loose data from (physical path)
        const char* dataRootUtf8 = "Data";
    };

}
