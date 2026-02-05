#include "Runtime/Engine.h"
#include "Core/Log.h"

#include <string>
#include <string_view>
#include <chrono>
#include <thread>

#ifndef NOC_CONTENT_ROOT
#define NOC_CONTENT_ROOT .
#endif

// -----------------------------
// Minimal test macros
// -----------------------------
static int g_failCount = 0;

#define TEST_CASE(name) \
    NOC_LOG_INFO("Test", "==== %s ====", name)

#define REQUIRE(cond, msg) \
    do { \
        if (!(cond)) { \
            ++g_failCount; \
            NOC_LOG_ERROR("Test", "FAIL: %s (%s:%d) :: %s", #cond, __FILE__, __LINE__, msg); \
            return false; \
        } \
    } while (0)

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            ++g_failCount; \
            NOC_LOG_ERROR("Test", "FAIL: %s (%s:%d) :: %s", #cond, __FILE__, __LINE__, msg); \
        } \
    } while (0)

// -----------------------------
// Helpers
// -----------------------------
static bool PumpUntilReady(noc::Engine & engine, noc::ResourceHandle h, uint32_t timeoutMs)
{
    const auto start = std::chrono::steady_clock::now();

    while (true)
    {
        // IMPORTANT: finalize completed loads on the main thread.
        engine.Resources().Update();

        if (engine.Resources().IsReady(h))   return true;
        if (engine.Resources().HasFailed(h)) return false;

        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        if (elapsed >= timeoutMs)
            return false;

        // Optional: avoid busy spin
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}


static std::string PreviewPrintable(std::string_view s, size_t n = 64)
{
    std::string out;
    n = (s.size() < n) ? s.size() : n;
    out.reserve(n);
    for (size_t i = 0; i < n; ++i)
    {
        unsigned char c = static_cast<unsigned char>(s[i]);
        out.push_back((c >= 32 && c < 127) ? static_cast<char>(c) : '.');
    }
    return out;
}

// -----------------------------
// Phase 5 tests
// -----------------------------
static bool Test_TextResource_Load_And_Content(noc::Engine& engine)
{
    TEST_CASE("TextResource: load hello.txt and access content");

    // NOTE: adjust file name to whatever you actually have in Data/
    auto ht = engine.Resources().RequestText("hello.txt");
    REQUIRE(ht.IsValid(), "RequestText returned invalid handle");

    const auto hu = ht.Untyped();
    const bool ready = PumpUntilReady(engine, hu, 2000);
    REQUIRE(ready, "Text resource did not become READY in time (or FAILED)");

    const auto* txt = engine.Resources().GetText(ht);
    REQUIRE(txt != nullptr, "GetText returned null even though resource is READY");

    CHECK(txt->Size() > 0, "TextResource is empty (maybe file empty?)");
    NOC_LOG_INFO("Test", "Text size=%zu preview='%s'", txt->Size(), PreviewPrintable(txt->View()).c_str());

    return true;
}

static bool Test_TextResource_Cache_SingleInstance(noc::Engine& engine)
{
    TEST_CASE("TextResource: cache returns same handle for same vpath");

    auto a = engine.Resources().RequestText("hello.txt");
    auto b = engine.Resources().RequestText("hello.txt");

    REQUIRE(a.IsValid() && b.IsValid(), "One of the handles is invalid");

    // Strong check: untyped handles match exactly.
    // If your system generates separate typed handles that still map to same record, this should be true.
    CHECK(a.Untyped() == b.Untyped(), "Same vpath did not return same underlying record/handle");

    return true;
}

static bool Test_TextResource_MissingFile_Fails(noc::Engine& engine)
{
    TEST_CASE("TextResource: missing file fails cleanly");

    auto h = engine.Resources().RequestText("this_file_should_not_exist_12345.txt");
    REQUIRE(h.IsValid(), "RequestText returned invalid handle for missing file (should still return a record and later fail)");

    const bool ready = PumpUntilReady(engine, h.Untyped(), 2000);
    REQUIRE(!ready, "Missing file unexpectedly became READY");

    REQUIRE(engine.Resources().HasFailed(h.Untyped()), "Missing file should be FAILED");
    const char* err = engine.Resources().GetError(h.Untyped()); // add if not present
    CHECK(err && err[0] != '\0', "Error string should be non-empty on failure");
    if (err) NOC_LOG_INFO("Test", "Expected failure error: %s", err);

    return true;
}

static bool Test_TypeIsolation_BinaryVsText(noc::Engine& engine)
{
    TEST_CASE("Type isolation: binary and text requests do not alias incorrectly");

    auto hb = engine.Resources().RequestBinary("hello.txt");
    auto ht = engine.Resources().RequestText("hello.txt");

    REQUIRE(hb.IsValid() && ht.IsValid(), "Invalid handles");

    // IMPORTANT POLICY TEST:
    // Best-practice: typed resources must NOT alias the same record as raw-binary,
    // because the record's payload semantics differ.
    //
    // If these are equal, your ResourceID key is insufficient and needs (id,type) as the cache key.
    CHECK(!(hb == ht.Untyped()), "Binary and Text requests alias the same record; cache key should include ResourceType");

    return true;
}

static bool RunAllPhase5Tests()
{
    noc::Engine engine;
    engine.SetContentRoot(NOC_CONTENT_ROOT);

    if (!engine.Init())
    {
        NOC_LOG_ERROR("Test", "Engine.Init failed");
        return false;
    }

    bool ok = true;

    ok &= Test_TextResource_Load_And_Content(engine);
    ok &= Test_TextResource_Cache_SingleInstance(engine);
    ok &= Test_TextResource_MissingFile_Fails(engine);
    ok &= Test_TypeIsolation_BinaryVsText(engine);

    engine.Shutdown();

    if (g_failCount == 0 && ok)
        NOC_LOG_INFO("Test", "ALL TESTS PASSED");
    else
        NOC_LOG_ERROR("Test", "TESTS FAILED: %d failures", g_failCount);

    return (g_failCount == 0 && ok);
}

// Expose to main:
int RunPhase5Tests()
{
    return RunAllPhase5Tests() ? 0 : 1;
}
