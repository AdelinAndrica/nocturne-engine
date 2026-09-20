#include "Assets/AssetImportPipeline.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <vector>

#include "Core/Log.h"
#include "Core/Jobs/JobSystem.h"
#include "Resources/VirtualFileSystem.h"
#include "Resources/VPath.h"
#include "Runtime/Engine.h"

#include "Assets/JsonWriter.h"

#include "Assets/Importers/TextImporter.h"
#include "Assets/Importers/ObjMeshImporter.h"
#include "Assets/Importers/BmpTextureImporter.h"

#include "Assets/RuntimeFormats/MeshBlob.h"
#include "Assets/RuntimeFormats/TextureBlob.h"
#include "Assets/RuntimeFormats/MaterialBlob.h"

namespace noc {

    static std::string NormalizeVPath_(std::string_view vpath)
    {
        char buf[512]{};
        if (!noc::vpath::NormalizeToRelative(buf, sizeof(buf), vpath))
            return {};
        return std::string(buf);
    }

    static uint64_t NowUtcMs_() {
        using namespace std::chrono;
        return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    std::string AssetImportPipeline::ToLower_(std::string s) {
        for (char& c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    }

    std::string AssetImportPipeline::ExtensionLower_(std::string_view path) {
        std::string p(path);
        auto dot = p.find_last_of('.');
        if (dot == std::string::npos) return "";
        return ToLower_(p.substr(dot + 1));
    }

    uint64_t AssetImportPipeline::Hash64_FNV1a_(const void* data, size_t size) {
        const uint8_t* b = (const uint8_t*)data;
        uint64_t h = 1469598103934665603ull;
        for (size_t i = 0; i < size; ++i) {
            h ^= (uint64_t)b[i];
            h *= 1099511628211ull;
        }
        return h;
    }

    uint64_t AssetImportPipeline::Hash64_Combine_(uint64_t a, uint64_t b) {
        // simple xor+mix
        uint64_t x = a ^ (b + 0x9e3779b97f4a7c15ull + (a << 6) + (a >> 2));
        return x;
    }

    std::string AssetImportPipeline::DdcPhysicalRoot_() const {
        // Convention: content root is mounted as the main data dir (see host snippet in combined.md).
        // Phase 11 design: DerivedDataCache folder under content root.
        // We rely on Engine config/root already set (Engine::SetContentRoot).
        // Engine exposes Config() (in combined.md), but if not, use content root via VFS mount path is internal.
        // Design choice: store DDC under "./DerivedDataCache" relative to content root path on disk.
        // We'll use engine executable dir + "Data/DerivedDataCache" only if content root is unknown.
        // Here we choose: <contentRootPhysical>/DerivedDataCache (pipeline called from host, so cwd is stable).
        return std::string("DerivedDataCache");
    }

    std::string AssetImportPipeline::DdcGraphPhysicalPath_() const {
        return DdcPhysicalRoot_() + "/AssetGraph.json";
    }

    std::string AssetImportPipeline::MakeDdcPhysicalPathForSource_(const std::string& vpathNormalized, std::string_view suffix) const {
        // Mirror vpath directories inside DDC.
        // e.g. "Meshes/cube.obj" -> "DerivedDataCache/Meshes/cube.obj.nmsh"
        return DdcPhysicalRoot_() + "/" + vpathNormalized + std::string(suffix);
    }

    bool AssetImportPipeline::Init(Engine& engine) {
        engine_ = &engine;
        vfs_ = &engine.VFS();
        jobs_ = &engine.Jobs(); // per architecture diagram JobSystem exists on engine :contentReference[oaicite:14]{index=14}

        // Register importers
        registry_.Register(std::make_unique<TextImporter>());
        registry_.Register(std::make_unique<ObjMeshImporter>());
        registry_.Register(std::make_unique<BmpTextureImporter>());

        // Ensure DDC folder exists (physical)
        std::filesystem::create_directories(DdcPhysicalRoot_());

        // Graph (write-only cache in Phase 11)
        graph_.LoadJson(DdcGraphPhysicalPath_());

        NOC_LOG_INFO("Assets", "AssetImportPipeline initialized (DDC='%s')", DdcPhysicalRoot_().c_str());
        return true;
    }

    void AssetImportPipeline::Shutdown() {
        if (engine_) {
            graph_.SaveJson(DdcGraphPhysicalPath_());
            NOC_LOG_INFO("Assets", "Graph saved: %s", DdcGraphPhysicalPath_().c_str());
        }
        engine_ = nullptr;
        vfs_ = nullptr;
        jobs_ = nullptr;
    }

    static bool LooksLikePhysicalPath_(std::string_view s) {
        // Windows: "C:\..." or "\\server\..."
        if (s.size() >= 2 && std::isalpha((unsigned char)s[0]) && s[1] == ':') return true;
        if (s.size() >= 2 && s[0] == '\\' && s[1] == '\\') return true;
        if (!s.empty() && (s[0] == '/' || s[0] == '\\')) return true;
        return false;
    }

    bool AssetImportPipeline::ImportOne(std::string_view pathOrVpath) {
        if (!engine_ || !vfs_) return false;

        std::string physical;
        std::string vpath;

        if (LooksLikePhysicalPath_(pathOrVpath)) {
            physical = std::string(pathOrVpath);
            // Map physical into a vpath is project-specific; for Phase 11 we require vpath for stable ID.
            // Design choice: if physical path contains "Data\", strip before it.
            auto pos = physical.find("Data\\");
            if (pos != std::string::npos) {
                vpath = physical.substr(pos + 5);
                for (char& c : vpath) if (c == '\\') c = '/';
            }
            else {
                vpath = std::filesystem::path(physical).filename().string();
            }
        }
        else {
            vpath = std::string(pathOrVpath);
        }

        // Normalize using the same helper the engine uses for resources (VPath normalization exists in Phase 3/4/5). 
        const std::string norm = NormalizeVPath_(vpath);
        if (norm.empty()) {
            NOC_LOG_ERROR("Assets", "Invalid vpath: %s", vpath.c_str());
            return false;
        }

        return ImportResolved_(norm, physical);

    }

    bool AssetImportPipeline::ImportAll() {
        // Design choice: scan physical filesystem under content root "Data".
        // Historical examples may mount a loose directory like "<repo>/Data".
        // we assume host runs with cwd at repo root and Data exists.
        const std::filesystem::path root("Data");
        if (!std::filesystem::exists(root)) {
            NOC_LOG_ERROR("Assets", "ImportAll: missing Data/ folder (run from repo root)");
            return false;
        }

        std::vector<std::filesystem::path> files;
        for (auto& it : std::filesystem::recursive_directory_iterator(root)) {
            if (!it.is_regular_file()) continue;
            files.push_back(it.path());
        }

        std::atomic<uint32_t> okCount{ 0 };
        std::atomic<uint32_t> failCount{ 0 };
        std::atomic<uint32_t> remaining{ (uint32_t)files.size() };

        for (auto& p : files) {
            jobs_->Enqueue([this, p, &okCount, &failCount, &remaining]() {
                const std::string phys = p.string();
                std::string v = phys;
                auto pos = v.find("Data\\");
                if (pos != std::string::npos) {
                    v = v.substr(pos + 5);
                    for (char& c : v) if (c == '\\') c = '/';
                }
                else {
                    v = p.filename().string();
                }

                char buf[512]{};
                if (!noc::vpath::NormalizeToRelative(buf, sizeof(buf), std::string_view(v))) {
                    failCount.fetch_add(1);
                    remaining.fetch_sub(1);
                    return;
                }

                if (ImportResolved_(std::string(buf), phys)) okCount.fetch_add(1);
                else failCount.fetch_add(1);
                remaining.fetch_sub(1);
                });
        }

        // Wait until all jobs finish (Phase 11: simple busy wait).
        while (remaining.load() != 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        graph_.SaveJson(DdcGraphPhysicalPath_());

        NOC_LOG_INFO("Assets", "ImportAll complete: ok=%u fail=%u", okCount.load(), failCount.load());
        return failCount.load() == 0;
    }

    static uint64_t FileTimeUtcMs_(const std::filesystem::path& p) {
        auto ft = std::filesystem::last_write_time(p);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ft - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
        return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(sctp.time_since_epoch()).count();
    }

    bool AssetImportPipeline::ReadSourceBytes_(const std::string& vpathNormalized,
        const std::string& physicalPathOrEmpty,
        std::vector<uint8_t>* outBytes,
        uint64_t* outTimestampUtcMs,
        uint64_t* outHash)
    {
        if (!outBytes) return false;
        outBytes->clear();

        // ---- Physical path ----
        if (!physicalPathOrEmpty.empty()) {
            std::ifstream f(physicalPathOrEmpty, std::ios::binary);
            if (!f) return false;

            f.seekg(0, std::ios::end);
            const size_t n = (size_t)f.tellg();
            f.seekg(0, std::ios::beg);

            outBytes->resize(n);
            if (n) f.read(reinterpret_cast<char*>(outBytes->data()), (std::streamsize)n);

            if (outTimestampUtcMs) *outTimestampUtcMs = FileTimeUtcMs_(physicalPathOrEmpty);
            if (outHash) *outHash = Hash64_FNV1a_(outBytes->data(), outBytes->size());
            return true;
        }

        // ---- VFS path ----
        size_t size = 0;

        // Use the engine allocator that matches your VFS expectations.
        // Replace Allocator() with whatever your Engine exposes (e.g. engine_->Alloc(), engine_->GetAllocator(), etc.)
        auto& alloc = engine_->Allocator();

        uint8_t* data = vfs_->ReadAllBytes(vpathNormalized.c_str(), size, alloc);
        if (!data && size != 0) {
            // Some implementations may return nullptr on failure with size==0; treat as failure when size!=0.
            return false;
        }

        if (size > 0) {
            outBytes->assign(data, data + size);
        }

        // Free allocator-owned buffer
        if (data) {
            alloc.Deallocate(data);
        }

        // Timestamp via physical assumption: Data/<vpath>
        if (outTimestampUtcMs) {
            std::filesystem::path phys = std::filesystem::path("Data") / std::filesystem::path(vpathNormalized);
            if (std::filesystem::exists(phys)) *outTimestampUtcMs = FileTimeUtcMs_(phys);
            else *outTimestampUtcMs = 0;
        }

        if (outHash) *outHash = Hash64_FNV1a_(outBytes->data(), outBytes->size());
        return true;
    }


    bool AssetImportPipeline::WriteMetadataJson_(const std::string& physicalMetaPath, const AssetMetadata& md) {
        std::filesystem::create_directories(std::filesystem::path(physicalMetaPath).parent_path());

        std::ofstream f(physicalMetaPath, std::ios::binary);
        if (!f) return false;

        JsonWriter w(f);
        w.BeginObject();
        w.KeyUInt("id", md.id.value);
        w.KeyString("sourceVPath", md.sourceVPathNormalized);
        w.KeyString("sourcePhysicalPath", md.sourcePhysicalPath);
        w.KeyUInt("sourceTimestampUtcMs", md.sourceTimestampUtcMs);
        w.KeyUInt("sourceContentHash", md.sourceContentHash);
        w.KeyString("importerId", md.importerId);
        w.KeyUInt("importerVersion", md.importerVersion);
        w.KeyUInt("optionsHash", md.optionsHash);
        w.KeyUInt("importTimestampUtcMs", md.importTimestampUtcMs);
        w.KeyUInt("buildFingerprint", md.buildFingerprint);

        w.Key("dependencies");
        w.BeginArray();
        for (auto& d : md.dependencies) {
            w.BeginObject();
            w.KeyUInt("id", d.id.value);
            w.KeyString("vpath", d.vpathNormalized);
            w.KeyUInt("fingerprint", d.fingerprint);
            w.EndObject();
        }
        w.EndArray();

        w.Key("outputs");
        w.BeginArray();
        for (auto& o : md.outputs) {
            w.BeginObject();
            w.KeyString("vpath", o.vpath);
            w.KeyString("type", o.type);
            w.KeyUInt("version", o.version);
            w.EndObject();
        }
        w.EndArray();

        w.EndObject();
        return true;
    }

    static uint64_t AssetIdFromVPath_(const std::string& vpathNormalized) {
        // Deterministic: hash normalized vpath.
        // (Design choice: FNV1a64)
        uint64_t h = 1469598103934665603ull;
        for (char c : vpathNormalized) {
            h ^= (uint64_t)(uint8_t)c;
            h *= 1099511628211ull;
        }
        return h;
    }

    bool AssetImportPipeline::ImportResolved_(const std::string& vpathNormalized, const std::string& physicalPathOrEmpty) {
        const std::string ext = ExtensionLower_(vpathNormalized);
        IAssetImporter* imp = registry_.FindForExtension(ext);
        if (!imp) {
            return false; // ignore unsupported
        }

        // Read bytes
        std::vector<uint8_t> bytes;
        uint64_t ts = 0;
        uint64_t srcHash = 0;
        if (!ReadSourceBytes_(vpathNormalized, physicalPathOrEmpty, &bytes, &ts, &srcHash)) {
            NOC_LOG_ERROR("Assets", "Import: %s -> FAILED (read)", vpathNormalized.c_str());
            return false;
        }

        ImportRequest req{};
        req.sourceVPathNormalized = vpathNormalized;
        req.sourcePhysicalPath = physicalPathOrEmpty;

        // Run importer (some importers use memory path)
        ImportResult ir{};
        if (ext == "obj") {
            auto* o = dynamic_cast<ObjMeshImporter*>(imp);
            ir = o->ImportFromMemory(req, bytes.data(), bytes.size());
        }
        else if (ext == "bmp") {
            auto* b = dynamic_cast<BmpTextureImporter*>(imp);
            ir = b->ImportFromMemory(req, bytes.data(), bytes.size());
        }
        else {
            // txt: directly store
            ir = imp->Import(req);
            if (ir.ok) {
                IntermediateText t{};
                t.text.assign((const char*)bytes.data(), (const char*)bytes.data() + bytes.size());
                ir.asset = std::move(t);
            }
        }

        if (!ir.ok) {
            NOC_LOG_ERROR("Assets", "Import: %s -> FAILED (%s)", vpathNormalized.c_str(), ir.error.c_str());
            return false;
        }

        // Build metadata
        AssetMetadata md{};
        md.id = AssetId{ AssetIdFromVPath_(vpathNormalized) };
        md.sourceVPathNormalized = vpathNormalized;
        md.sourcePhysicalPath = physicalPathOrEmpty;
        md.sourceTimestampUtcMs = ts;
        md.sourceContentHash = srcHash;

        md.importerId = ir.importerId;
        md.importerVersion = ir.importerVersion;
        md.importTimestampUtcMs = NowUtcMs_();

        // Options hash (Phase 11: no options yet)
        md.optionsHash = 0;

        // Fingerprint combines everything
        md.buildFingerprint = Hash64_Combine_(md.sourceContentHash, md.importerVersion);
        md.buildFingerprint = Hash64_Combine_(md.buildFingerprint, md.optionsHash);

        // Emit artifacts based on intermediate type
        bool wrote = false;
        std::string err;

        if (std::holds_alternative<IntermediateMesh>(ir.asset)) {
            const auto& m = std::get<IntermediateMesh>(ir.asset);
            const std::string blobPhys = MakeDdcPhysicalPathForSource_(vpathNormalized, ".nmsh");
            const std::string metaPhys = MakeDdcPhysicalPathForSource_(vpathNormalized, ".meta.json");

            std::filesystem::create_directories(std::filesystem::path(blobPhys).parent_path());
            if (!WriteMeshBlob(blobPhys, m, &err)) {
                NOC_LOG_ERROR("Assets", "WriteMeshBlob failed: %s", err.c_str());
                return false;
            }

            md.outputs.push_back(AssetOutputArtifact{ vpathNormalized + ".nmsh", "Mesh", 1 });

            if (!WriteMetadataJson_(metaPhys, md)) {
                NOC_LOG_ERROR("Assets", "WriteMetadata failed: %s", metaPhys.c_str());
                return false;
            }

            wrote = true;
        }
        else if (std::holds_alternative<IntermediateTexture>(ir.asset)) {
            const auto& t = std::get<IntermediateTexture>(ir.asset);
            const std::string blobPhys = MakeDdcPhysicalPathForSource_(vpathNormalized, ".ntx");
            const std::string metaPhys = MakeDdcPhysicalPathForSource_(vpathNormalized, ".meta.json");

            std::filesystem::create_directories(std::filesystem::path(blobPhys).parent_path());
            if (!WriteTextureBlob(blobPhys, t, &err)) {
                NOC_LOG_ERROR("Assets", "WriteTextureBlob failed: %s", err.c_str());
                return false;
            }

            md.outputs.push_back(AssetOutputArtifact{ vpathNormalized + ".ntx", "Texture", 1 });

            if (!WriteMetadataJson_(metaPhys, md)) {
                NOC_LOG_ERROR("Assets", "WriteMetadata failed: %s", metaPhys.c_str());
                return false;
            }

            wrote = true;
        }
        else if (std::holds_alternative<IntermediateText>(ir.asset)) {
            const auto& t = std::get<IntermediateText>(ir.asset);
            const std::string blobPhys = MakeDdcPhysicalPathForSource_(vpathNormalized, ".txt.bin");
            const std::string metaPhys = MakeDdcPhysicalPathForSource_(vpathNormalized, ".meta.json");

            std::filesystem::create_directories(std::filesystem::path(blobPhys).parent_path());
            std::ofstream f(blobPhys, std::ios::binary);
            f.write(t.text.data(), (std::streamsize)t.text.size());

            md.outputs.push_back(AssetOutputArtifact{ vpathNormalized + ".txt.bin", "Text", 1 });

            if (!WriteMetadataJson_(metaPhys, md)) {
                NOC_LOG_ERROR("Assets", "WriteMetadata failed: %s", metaPhys.c_str());
                return false;
            }

            wrote = true;
        }

        if (!wrote) {
            NOC_LOG_ERROR("Assets", "Import: %s -> FAILED (unsupported intermediate variant)", vpathNormalized.c_str());
            return false;
        }

        // Update graph edges (Phase 11: dependencies empty for obj/bmp subset)
        graph_.SetEdges(md.id, {});

        graph_.SaveJson(DdcGraphPhysicalPath_());

        NOC_LOG_INFO("Assets", "Import: %s -> OK (wrote artifacts)", vpathNormalized.c_str());
        return true;
    }

} // namespace noc
