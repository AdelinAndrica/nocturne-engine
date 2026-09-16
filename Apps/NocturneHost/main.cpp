#include "Runtime/Engine.h"
#include "Core/Log.h"

#include <string>
#include <vector>

// Phase 11 tests
bool RunPhase11Tests(noc::Engine& engine);

// Phase 12 tests
bool RunPhase12Tests(noc::Engine& engine);

// Phase 12 tooling
#include "Phase12CookPack.h"

#ifndef NOC_CONTENT_ROOT
#define NOC_CONTENT_ROOT "."
#endif

static bool HasArg(const std::vector<std::string>& args, const char* a) {
    for (const auto& s : args) if (s == a) return true;
    return false;
}

static std::string GetArgValue(const std::vector<std::string>& args, const char* key) {
    for (size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == key) return args[i + 1];
    }
    return {};
}

int main(int argc, char** argv)
{
    std::vector<std::string> args;
    args.reserve((size_t)argc);
    for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);

    noc::Engine engine;

    // ---- Engine configuration (same as previous phases) ----
    engine.SetContentRoot(NOC_CONTENT_ROOT);

    if (!engine.Init())
    {
        NOC_LOG_FATAL("Host", "Engine initialization failed");
        return -1;
    }

    // Phase 11 CLI modes (existing in your file):
    // --import <vpathOrPhysical>
    // --import-all
    // --phase11-tests
    //
    // Phase 12 additions:
    const bool doCook = HasArg(args, "--cook");
    const bool doPack = HasArg(args, "--pack");
    const bool doCookPack = HasArg(args, "--cookpack");
    const bool doP12Tests = HasArg(args, "--phase12-tests");

    if (doCook || doCookPack) {
        noc::tools::Phase12CookPack::CookOptions opt{};
        const std::string ddc = GetArgValue(args, "--ddc");
        const std::string cooked = GetArgValue(args, "--cooked");

        if (!ddc.empty()) opt.ddcRoot = ddc;
        if (!cooked.empty()) opt.cookedRoot = cooked;
        opt.cleanCooked = !HasArg(args, "--no-clean");

        std::vector<noc::tools::CookFileInfo> files;
        std::string err;
        if (!noc::tools::Phase12CookPack::Cook(opt, &files, &err)) {
            NOC_LOG_ERROR("Host", "Cook failed: %s", err.c_str());
            engine.Shutdown();
            return 1;
        }
    }

    if (doPack || doCookPack) {
        noc::tools::Phase12CookPack::PackOptions opt{};
        const std::string cooked = GetArgValue(args, "--cooked");
        const std::string outZip = GetArgValue(args, "--out-zip");

        if (!cooked.empty()) opt.cookedRoot = cooked;
        if (!outZip.empty()) opt.outZip = outZip;

        std::string err;
        if (!noc::tools::Phase12CookPack::Pack(opt, &err)) {
            NOC_LOG_ERROR("Host", "Pack failed: %s", err.c_str());
            engine.Shutdown();
            return 1;
        }
    }

    if (doP12Tests) {
        const bool ok = RunPhase12Tests(engine);
        engine.Shutdown();
        return ok ? 0 : 1;
    }

    // Keep existing Phase 11 test hook in your file:
    // if (HasArg(args, "--phase11-tests")) { ... }

    // Normal runtime path (if any) continues here...

    engine.Shutdown();
    return 0;
}