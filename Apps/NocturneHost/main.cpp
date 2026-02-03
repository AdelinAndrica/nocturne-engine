#include "Runtime/Engine.h"
#include "Core/Log.h"

#include <algorithm>
#include <cctype>
#include <string>

#ifndef NOC_CONTENT_ROOT
#define NOC_CONTENT_ROOT .
#endif

static void LogPreview(const uint8_t* bytes, size_t size)
{
    if (!bytes || size == 0)
    {
        NOC_LOG_INFO("Host", "File empty");
        return;
    }

    const size_t n = std::min<size_t>(size, 64);
    std::string s;
    s.reserve(n);

    for (size_t i = 0; i < n; ++i)
    {
        const char c = static_cast<char>(bytes[i]);
        s.push_back((std::isprint((unsigned char)c) ? c : '.'));
    }

    NOC_LOG_INFO("Host", "Preview (first %zu bytes): %s", n, s.c_str());
}

int main()
{
    noc::Engine engine;

    // Phase 3 config defaults to contentRoot="Data".
    // If your repo uses a different layout, you can override here before Init():
     engine.SetContentRoot(NOC_CONTENT_ROOT);
    // engine.SetOverrideRoot("DataOverrides");
    // engine.SetArchivePath("Data.pak");

    if (!engine.Init())
        return -1;

    // Phase 4 smoke test (host-side): load hello.txt via ResourceManager.
    auto h = engine.Resources().RequestBinary("hello.txt");
    if (!h.IsValid())
    {
        NOC_LOG_ERROR("Host", "Failed to request hello.txt");
    }
    else
    {
        const bool ok = engine.Resources().WaitUntilReady(h, /*timeoutMs*/ 2000);
        if (!ok)
        {
            NOC_LOG_ERROR("Host", "hello.txt did not become ready (failed or timed out)");
        }
        else
        {
            const uint8_t* bytes = engine.Resources().GetBytes(h);
            const size_t size = engine.Resources().GetSize(h);
            NOC_LOG_INFO("Host", "hello.txt loaded (%zu bytes)", size);
            LogPreview(bytes, size);
        }
    }

    auto h1 = engine.Resources().RequestBinary("hello.txt");
    auto h2 = engine.Resources().RequestBinary("hello.txt");

    NOC_LOG_INFO("Host", "Handle1 = (%u, %u)", h1.index, h1.generation);
    NOC_LOG_INFO("Host", "Handle2 = (%u, %u)", h2.index, h2.generation);


    const int rc = engine.Run();

    engine.Shutdown();
    return rc;
}
