#include "Runtime/Engine.h"
#include "Core/Log.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "Resources/ResourceManager.h"
#include "Resources/Typed/ResourceHandleT.h"
#include "Resources/Typed/MeshResource.h"
#include "Resources/Typed/TextureResource.h"

static bool WriteDummyObj_(const std::string& phys) {
    std::filesystem::create_directories(std::filesystem::path(phys).parent_path());
    std::ofstream f(phys, std::ios::binary);
    if (!f) return false;

    // A single triangle
    f <<
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "vt 0 0\n"
        "vt 1 0\n"
        "vt 0 1\n"
        "vn 0 0 1\n"
        "f 1/1/1 2/2/1 3/3/1\n";
    return true;
}

static bool WriteDummyBmp32_(const std::string& phys, int w, int h) {
    // Minimal BGRA BMP
#pragma pack(push, 1)
    struct FH { uint16_t t; uint32_t sz; uint16_t r1; uint16_t r2; uint32_t off; };
    struct IH {
        uint32_t sz; int32_t w; int32_t h; uint16_t planes; uint16_t bpp;
        uint32_t comp; uint32_t imgSz; int32_t xppm; int32_t yppm; uint32_t clrUsed; uint32_t clrImp;
    };
#pragma pack(pop)

    std::filesystem::create_directories(std::filesystem::path(phys).parent_path());
    std::ofstream f(phys, std::ios::binary);
    if (!f) return false;

    const uint32_t bpp = 4;
    const uint32_t rowBytes = (uint32_t)w * bpp;
    const uint32_t pad = (4 - (rowBytes % 4)) % 4;
    const uint32_t stride = rowBytes + pad;
    const uint32_t imgSz = stride * (uint32_t)h;

    FH fh{};
    fh.t = 0x4D42;
    fh.off = sizeof(FH) + sizeof(IH);
    fh.sz = fh.off + imgSz;

    IH ih{};
    ih.sz = 40;
    ih.w = w;
    ih.h = h; // bottom-up
    ih.planes = 1;
    ih.bpp = 32;
    ih.comp = 0;
    ih.imgSz = imgSz;

    f.write((const char*)&fh, sizeof(fh));
    f.write((const char*)&ih, sizeof(ih));

    // Write solid pixels (blue)
    std::vector<uint8_t> row(stride, 0);
    for (int x = 0; x < w; ++x) {
        row[x * 4 + 0] = 255; // B
        row[x * 4 + 1] = 0;
        row[x * 4 + 2] = 0;
        row[x * 4 + 3] = 255;
    }
    for (int y = 0; y < h; ++y) {
        f.write((const char*)row.data(), (std::streamsize)row.size());
    }
    return true;
}

bool RunPhase11Tests(noc::Engine& engine)
{
    NOC_LOG_INFO("Test", "==============================");
    NOC_LOG_INFO("Test", "Phase 11 — Asset Import Pipeline Tests");
    NOC_LOG_INFO("Test", "==============================");

    // Arrange: create dummy source files in Data/
    const std::string objPhys = "Data/Phase11/tri.obj";
    const std::string bmpPhys = "Data/Phase11/tex.bmp";
    if (!WriteDummyObj_(objPhys)) { NOC_LOG_ERROR("Test", "Failed to write obj"); return false; }
    if (!WriteDummyBmp32_(bmpPhys, 4, 4)) { NOC_LOG_ERROR("Test", "Failed to write bmp"); return false; }

    // Act: import by vpath
    bool ok = true;
    ok &= engine.Assets().ImportOne("Phase11/tri.obj");
    ok &= engine.Assets().ImportOne("Phase11/tex.bmp");
    if (!ok) { NOC_LOG_ERROR("Test", "ImportOne failed"); return false; }

    // Verify expected outputs exist
    const std::string nmsh = "DerivedDataCache/Phase11/tri.obj.nmsh";
    const std::string ntx = "DerivedDataCache/Phase11/tex.bmp.ntx";
    const std::string metaObj = "DerivedDataCache/Phase11/tri.obj.meta.json";
    const std::string metaBmp = "DerivedDataCache/Phase11/tex.bmp.meta.json";
    const std::string graph = "DerivedDataCache/AssetGraph.json";

    if (!std::filesystem::exists(nmsh) || !std::filesystem::exists(ntx) ||
        !std::filesystem::exists(metaObj) || !std::filesystem::exists(metaBmp) ||
        !std::filesystem::exists(graph))
    {
        NOC_LOG_ERROR("Test", "Missing expected derived outputs");
        return false;
    }

    // Now verify ResourceManager typed load for runtime blobs:
    // We load by VFS vpath (mounted DDC in Engine::Init modifications).
    auto mh = engine.Resources().RequestMesh("Phase11/tri.obj.nmsh");
    if (!mh.IsValid()) { NOC_LOG_ERROR("Test", "RequestMesh invalid"); return false; }
    if (!engine.Resources().WaitUntilReady(mh.Untyped(), 2000)) { NOC_LOG_ERROR("Test", "Mesh not ready"); return false; }
    const auto* mesh = engine.Resources().GetMesh(mh);
    if (!mesh) { NOC_LOG_ERROR("Test", "GetMesh null"); return false; }

    NOC_LOG_INFO("Test", "Mesh vertices=%u indices=%u", mesh->VertexCount(), mesh->IndexCount());
    if (mesh->VertexCount() != 3 || mesh->IndexCount() != 3) {
        NOC_LOG_ERROR("Test", "Mesh counts mismatch");
        return false;
    }

    auto th = engine.Resources().RequestTexture("Phase11/tex.bmp.ntx");
    if (!th.IsValid()) { NOC_LOG_ERROR("Test", "RequestTexture invalid"); return false; }
    if (!engine.Resources().WaitUntilReady(th.Untyped(), 2000)) { NOC_LOG_ERROR("Test", "Texture not ready"); return false; }
    const auto* tex = engine.Resources().GetTexture(th);
    if (!tex) { NOC_LOG_ERROR("Test", "GetTexture null"); return false; }

    NOC_LOG_INFO("Test", "Texture w=%u h=%u", tex->Width(), tex->Height());
    if (tex->Width() != 4 || tex->Height() != 4) {
        NOC_LOG_ERROR("Test", "Texture dims mismatch");
        return false;
    }

    // Cache check: run import again (should be OK; Phase 11 currently always rewrites if called)
    // Design choice: extend with metadata fingerprint checks next.
    ok &= engine.Assets().ImportOne("Phase11/tri.obj");
    ok &= engine.Assets().ImportOne("Phase11/tex.bmp");

    NOC_LOG_INFO("Test", "Phase 11 tests PASS");
    return ok;
}
