#pragma once

namespace noc
{
    // Engine-owned boot configuration.
    // Phase 3: only VFS/mount policy lives here.
    //
    // Important rule:
    // - These values are consumed during Engine::Init() to mount the VFS.
    // - Changing them after Init() is not supported (config is frozen).
    struct EngineConfig
    {
        // Physical directory roots. May be absolute or relative to process working directory.
        // Default is "Data" to support the common dev layout: <working_dir>/Data/...
        const char* contentRoot = "Data";

        // Optional additional mount for overrides (mount order matters; see Engine::Init()).
        const char* overrideRoot = nullptr;

        // Optional archive mount, e.g. "Packed/game.zip"
        const char* archivePath = nullptr;
    };
}
