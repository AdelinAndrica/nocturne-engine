#pragma once
#include <span>

namespace noc {

    using SubsystemStartupFn = bool (*)(void* ctx);
    using SubsystemShutdownFn = void (*)(void* ctx);

    struct SubsystemDesc
    {
        const char* name;
        std::span<const char* const> dependencies;
        SubsystemStartupFn  startup;
        SubsystemShutdownFn shutdown;
    };

} // namespace noc
