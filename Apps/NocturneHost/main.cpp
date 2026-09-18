#include "Runtime/Engine.h"
#include "Core/Log.h"

#include <string>
#include <vector>

// Phase 11 tests
bool RunPhase11Tests(noc::Engine& engine);

// Phase 12 tests
bool RunPhase12Tests(noc::Engine& engine);

// Phase 15 ECS foundation tests
bool RunPhase15EntityRegistryTests();
bool RunPhase15ComponentStorageTests();
bool RunPhase15ComponentRegistryTests();
bool RunPhase15TransformTests();
bool RunPhase15RenderableTests();
bool RunPhase15CameraTests();
bool RunPhase15NameTests();
bool RunPhase15WorldTests();
bool RunPhase15StressPerfTests();

// Phase 16 runtime reflection tests
bool RunPhase16ReflectionFoundationTests();
bool RunPhase16ReflectionRegistryTests();
bool RunPhase16ReflectionOcpTests();
bool RunPhase16ReflectionPerfTests();
bool RunPhase16EditorSessionTests();
bool RunPhase16EditorPerfTests();

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

    // Phase 16 reflection tests are pure runtime tests and intentionally run
    // before Engine::Init(), so CI does not require DX12 or a native window.
    if (HasArg(args, "--phase16-tests")) {
        bool ok = RunPhase16ReflectionFoundationTests();
        ok &= RunPhase16ReflectionRegistryTests();
        ok &= RunPhase16ReflectionOcpTests();
        ok &= RunPhase16ReflectionPerfTests();
        ok &= RunPhase16EditorSessionTests();
        ok &= RunPhase16EditorPerfTests();
        return ok ? 0 : 1;
    }

    if (HasArg(args, "--phase16-reflection-foundation-tests")) {
        return RunPhase16ReflectionFoundationTests() ? 0 : 1;
    }

    if (HasArg(args, "--phase16-reflection-registry-tests")) {
        return RunPhase16ReflectionRegistryTests() ? 0 : 1;
    }

    if (HasArg(args, "--phase16-editor-perf-tests")) {
        return RunPhase16EditorPerfTests() ? 0 : 1;
    }

    // Phase 15 foundation tests are pure runtime tests. Keep them before
    // Engine::Init() so CI does not depend on DX12, a GPU, content mounts, or
    // a native window.
    if (HasArg(args, "--phase15-tests")) {
        bool ok = RunPhase15EntityRegistryTests();
        ok &= RunPhase15ComponentStorageTests();
        ok &= RunPhase15ComponentRegistryTests();
        ok &= RunPhase15TransformTests();
        ok &= RunPhase15RenderableTests();
        ok &= RunPhase15CameraTests();
        ok &= RunPhase15NameTests();
        ok &= RunPhase15WorldTests();
        ok &= RunPhase15StressPerfTests();
        return ok ? 0 : 1;
    }

    // Backward-compatible focused entry point from the first Phase 15 commit.
    if (HasArg(args, "--phase15-entity-tests")) {
        return RunPhase15EntityRegistryTests() ? 0 : 1;
    }

    if (HasArg(args, "--phase15-component-tests")) {
        return RunPhase15ComponentStorageTests() ? 0 : 1;
    }

    if (HasArg(args, "--phase15-component-registry-tests")) {
        return RunPhase15ComponentRegistryTests() ? 0 : 1;
    }

    if (HasArg(args, "--phase15-transform-tests")) {
        return RunPhase15TransformTests() ? 0 : 1;
    }

    if (HasArg(args, "--phase15-renderable-tests")) {
        return RunPhase15RenderableTests() ? 0 : 1;
    }

    if (HasArg(args, "--phase15-camera-tests")) {
        return RunPhase15CameraTests() ? 0 : 1;
    }

    if (HasArg(args, "--phase15-name-tests")) {
        return RunPhase15NameTests() ? 0 : 1;
    }

    if (HasArg(args, "--phase15-world-tests")) {
        return RunPhase15WorldTests() ? 0 : 1;
    }

    if (HasArg(args, "--phase15-stress-perf")) {
        return RunPhase15StressPerfTests() ? 0 : 1;
    }

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