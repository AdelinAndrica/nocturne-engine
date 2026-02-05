// NocturneHost/phase6_test.cpp
#include <chrono>
#include <thread>
#include <vector>
#include <string>

#include "Runtime/Engine.h"
#include "Core/Log.h"

#include "Resources/ResourceHandle.h"
#include "Resources/Typed/ResourceHandleT.h"
#include "Resources/Typed/TextResource.h"

namespace {

    static bool PumpUntilDone(noc::Engine& engine,
        const std::vector<noc::ResourceHandle>& handles,
        uint32_t timeoutMs)
    {
        const auto start = std::chrono::steady_clock::now();
        while (true)
        {
            // Main-thread finalize point (Phase 6 contract).
            engine.Resources().Update();

            bool allDone = true;
            for (const auto& h : handles)
            {
                if (!engine.Resources().IsReady(h) && !engine.Resources().HasFailed(h))
                {
                    allDone = false;
                    break;
                }
            }

            if (allDone)
                return true;

            if (timeoutMs != 0)
            {
                const auto now = std::chrono::steady_clock::now();
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
                if ((uint32_t)elapsed >= timeoutMs)
                    return false;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    static std::string PreviewString(const noc::TextResource& txt, size_t maxChars)
    {
        const auto v = txt.View();
        const size_t n = (v.size() < maxChars) ? v.size() : maxChars;
        return std::string(v.substr(0, n));
    }

} // namespace

// Call this from NocturneHost/main.cpp after engine.Init() succeeds.
// Returns true on pass, false on failure.
bool RunPhase6Tests(noc::Engine& engine)
{
    NOC_LOG_INFO("Test", "==============================");
    NOC_LOG_INFO("Test", "Phase 6 — Job System & Async Infrastructure Tests");
    NOC_LOG_INFO("Test", "==============================");

    bool okAll = true;

    // ------------------------------------------------------------
    // 1) TextResource: load hello.txt and access content
    // ------------------------------------------------------------
    {
        NOC_LOG_INFO("Test", "==== TextResource: load hello.txt and access content (job-backed) ====");

        auto h = engine.Resources().RequestText("hello.txt");
        const bool ok = engine.Resources().WaitUntilReady(h.Untyped(), /*timeoutMs=*/5000);

        if (!ok)
        {
            const char* err = engine.Resources().GetError(h.Untyped());
            NOC_LOG_ERROR("Test", "FAILED: hello.txt did not become READY (err=%s)", err ? err : "(none)");
            okAll = false;
        }
        else
        {
            const noc::TextResource* txt = engine.Resources().GetText(h);
            if (!txt)
            {
                NOC_LOG_ERROR("Test", "FAILED: GetText returned null for READY handle");
                okAll = false;
            }
            else
            {
                const auto preview = PreviewString(*txt, 64);
                NOC_LOG_INFO("Test", "Text size=%zu preview='%s'", txt->Size(), preview.c_str());
            }
        }
    }

    // ------------------------------------------------------------
    // 2) Cache: same vpath returns same handle (typed request)
    // ------------------------------------------------------------
    {
        NOC_LOG_INFO("Test", "==== TextResource: cache returns same handle for same vpath ====");

        auto a = engine.Resources().RequestText("hello.txt");
        auto b = engine.Resources().RequestText("hello.txt");

        const auto au = a.Untyped();
        const auto bu = b.Untyped();

        if (!au.IsValid() || !bu.IsValid())
        {
            NOC_LOG_ERROR("Test", "FAILED: RequestText returned invalid handle(s)");
            okAll = false;
        }
        else if (au.index != bu.index || au.generation != bu.generation)
        {
            NOC_LOG_ERROR("Test", "FAILED: cache did not return same handle (a=%u/%u b=%u/%u)",
                au.index, au.generation, bu.index, bu.generation);
            okAll = false;
        }
        else
        {
            NOC_LOG_INFO("Test", "Cache OK: same handle (index=%u gen=%u)", au.index, au.generation);
        }

        // Make sure the underlying load has finalized (in case this is the first request).
        engine.Resources().WaitUntilReady(au, 5000);
    }

    // ------------------------------------------------------------
    // 3) Missing file fails cleanly (typed request)
    // ------------------------------------------------------------
    {
        NOC_LOG_INFO("Test", "==== TextResource: missing file fails cleanly ====");

        auto h = engine.Resources().RequestText("this_file_should_not_exist_12345.txt");

        // WaitUntilReady returns false either for failure or timeout; we validate FAILED state.
        (void)engine.Resources().WaitUntilReady(h.Untyped(), /*timeoutMs=*/5000);

        if (!engine.Resources().HasFailed(h.Untyped()))
        {
            NOC_LOG_ERROR("Test", "FAILED: missing file did not transition to FAILED");
            okAll = false;
        }
        else
        {
            const char* err = engine.Resources().GetError(h.Untyped());
            NOC_LOG_INFO("Test", "Expected failure error: %s", err ? err : "(none)");
        }
    }

    // ------------------------------------------------------------
    // 4) Stress: burst requests (duplicates + mix) and ensure no deadlock
    // ------------------------------------------------------------
    {
        NOC_LOG_INFO("Test", "==== Stress: burst requests (duplicates + mix) ====");

        // Keep this list simple so it works on a clean repo with only hello.txt present.
        // (You can add more real files later.)
        const char* paths[] = {
            "hello.txt",
            "hello.txt",
            "hello.txt",
            "this_file_should_not_exist_12345.txt",
            "hello.txt",
            "this_file_should_not_exist_12345.txt",
        };

        std::vector<noc::ResourceHandle> handles;
        handles.reserve(std::size(paths));

        for (const char* p : paths)
        {
            auto h = engine.Resources().RequestText(p);
            handles.push_back(h.Untyped());
        }

        const bool done = PumpUntilDone(engine, handles, /*timeoutMs=*/5000);
        if (!done)
        {
            NOC_LOG_ERROR("Test", "FAILED: stress burst did not complete within timeout (possible deadlock)");
            okAll = false;
        }
        else
        {
            uint32_t readyCount = 0;
            uint32_t failCount = 0;
            for (auto h : handles)
            {
                if (engine.Resources().IsReady(h)) ++readyCount;
                else if (engine.Resources().HasFailed(h)) ++failCount;
            }

            NOC_LOG_INFO("Test", "Stress results: READY=%u FAILED=%u", readyCount, failCount);

            // Expect at least one READY (hello.txt) and at least one FAILED (missing).
            if (readyCount == 0 || failCount == 0)
            {
                NOC_LOG_WARN("Test", "Unexpected stress distribution (READY=%u FAILED=%u)", readyCount, failCount);
            }
        }
    }

    NOC_LOG_INFO("Test", "==============================");
    NOC_LOG_INFO("Test", "Phase 6 tests: %s", okAll ? "PASS" : "FAIL");
    NOC_LOG_INFO("Test", "==============================");

    return okAll;
}
