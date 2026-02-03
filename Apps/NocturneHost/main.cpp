// Apps/NocturneHost/main.cpp

#include "Runtime/Engine.h"
#include "Core/Log.h"
#include "Resources/VirtualFileSystem.h"
#include "Core/Memory/Allocator.h"

#include <string>

// Minimal helper so we can build absolute paths from NOC_CONTENT_ROOT.
// (Design choice: keep this test-local; no need for a global path util yet.)
static std::string JoinPath(const char* a, const char* b)
{
    std::string s = a ? a : "";
    if (!s.empty() && s.back() != '/' && s.back() != '\\')
        s.push_back('/');
    s += (b ? b : "");
    return s;
}

static void Phase3_ArchiveSmokeTest(noc::Engine& engine)
{
    auto& vfs = engine.VFS();
    auto& alloc = engine.Allocator();

    size_t size = 0;
    uint8_t* bytes = vfs.ReadAllBytes("hello_archive.txt", size, alloc);

    if (!bytes)
    {
        NOC_LOG_ERROR("Test", "ReadAllBytes('hello_archive.txt') failed");
        return;
    }

    NOC_LOG_INFO("Test", "Archive read OK: %zu bytes, text='%.*s'",
        size, (int)size, (const char*)bytes);

    alloc.Deallocate(bytes);
}

static void Phase3_MountPriorityTest(noc::Engine& engine)
{
    auto& vfs = engine.VFS();
    auto& alloc = engine.Allocator();

    // Put these on disk:
    //   D:/Projects/Nocturne/Data/priority.txt           -> "CONTENT"
    //   D:/Projects/Nocturne/DataOverrides/priority.txt  -> "OVERRIDE"
    //
    // With "later mounts override earlier", and mount order archive->content->override,
    // we expect OVERRIDE here.
    size_t size = 0;
    uint8_t* bytes = vfs.ReadAllBytes("priority.txt", size, alloc);

    if (!bytes)
    {
        NOC_LOG_ERROR("Test", "ReadAllBytes('priority.txt') failed");
        return;
    }

    NOC_LOG_INFO("Test", "priority.txt resolved to: '%.*s'", (int)size, (const char*)bytes);
    alloc.Deallocate(bytes);
}

int main()
{
    noc::Engine engine;

    // Loose content root (already working in your logs)
    engine.SetContentRoot(NOC_CONTENT_ROOT);

    // Dev overrides: create this folder and add priority.txt to prove precedence.
    // (If you don't have it yet, create D:/Projects/Nocturne/DataOverrides)
    engine.SetOverrideRoot("D:/Projects/Nocturne/DataOverrides");

    // Archive test: ensure this file exists on disk:
    //   D:/Projects/Nocturne/Data/test_archive.zip
    // containing:
    //   hello_archive.txt (stored/method 0)
    const std::string archiveAbs = JoinPath(NOC_CONTENT_ROOT, "test_archive.zip");
    engine.SetArchivePath(archiveAbs.c_str());

    if (!engine.Init())
        return -1;

#if NOC_ENABLE_ASSERTS
    // Phase 3 verification lives in the host (NOT inside Engine::Init()).
    Phase3_ArchiveSmokeTest(engine);
    Phase3_MountPriorityTest(engine);
#endif

    const int rc = engine.Run();
    engine.Shutdown();
    return rc;
}
