#pragma once

namespace noc
{
    /**
     * @brief Boot-time configuration consumed by Engine::Init().
     *
     * EngineConfig currently owns content-mount policy only. It tells the runtime
     * where loose content, optional overrides and an optional archive live.
     *
     * @par When to use
     * Configure these values before Engine::Init(), normally through
     * Engine::ConfigMutable() or the Set*Root()/SetArchivePath() helpers.
     *
     * @par Lifetime
     * These fields are raw const-char pointers. Engine does not copy the pointed
     * strings into owned storage here, so caller-provided storage must remain valid
     * until Init() has consumed the configuration.
     *
     * @par Frozen after Init
     * Runtime configuration changes after initialization are unsupported.
     *
     * @see Engine
     * @ingroup runtime
     */
    struct EngineConfig
    {
        /**
         * @brief Primary loose content directory.
         *
         * May be absolute or relative to the process working directory.
         * Default: @c "Data".
         */
        const char* contentRoot = "Data";

        /**
         * @brief Optional later loose mount used for overrides.
         *
         * VFS searches later mounts first, so files here may override matching paths
         * from earlier mounts.
         */
        const char* overrideRoot = nullptr;

        /**
         * @brief Optional archive mounted during Engine::Init().
         *
         * Example: @c "Packed/game.zip".
         */
        const char* archivePath = nullptr;
    };
}
