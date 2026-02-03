#include "Runtime/Engine.h"
#include "Core/Log.h"
#include "Resources/VirtualFileSystem.h"
#include "Core/Memory/Allocator.h"

#include <string>

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

int main()
{
    noc::Engine engine;

    engine.SetContentRoot(NOC_CONTENT_ROOT);

#if NOC_ENABLE_ASSERTS // or your debug/dev macro
    // IMPORTANT: this must be an actual file path that exists on disk.
    // Put test_archive.zip inside your content root directory.
    const std::string archiveAbs = JoinPath(NOC_CONTENT_ROOT, "test_archive.zip");
    engine.SetArchivePath(archiveAbs.c_str());
#endif

    if (!engine.Init())
        return -1;

    // Host-side verification (test lives here, not in Engine::Init)
    Phase3_ArchiveSmokeTest(engine);

    const int rc = engine.Run();
    engine.Shutdown();
    return rc;
}
