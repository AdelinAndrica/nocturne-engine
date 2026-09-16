#include "Phase12CookPack.h"

#include "ZipWriter.h"

#include <algorithm>
#include <fstream>

#include "Core/Log.h"
#include "Assets/JsonWriter.h"

namespace noc::tools {

    static bool RemoveAllSafe_(const std::filesystem::path& p) {
        std::error_code ec;
        if (!std::filesystem::exists(p, ec)) return true;
        std::filesystem::remove_all(p, ec);
        return !ec;
    }

    static bool CopyFileEnsureDirs_(const std::filesystem::path& src, const std::filesystem::path& dst, std::string* outError) {
        std::error_code ec;
        std::filesystem::create_directories(dst.parent_path(), ec);
        if (ec) {
            if (outError) *outError = "Failed to create dirs: " + dst.parent_path().string();
            return false;
        }
        std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            if (outError) *outError = "Failed to copy: " + src.string() + " -> " + dst.string();
            return false;
        }
        return true;
    }

    bool Phase12CookPack::IsWhitelistedArtifact_(const std::filesystem::path& p)
    {
        if (!p.has_filename()) return false;

        const std::string name = p.filename().string();

        // Always keep the graph if present.
        if (name == "AssetGraph.json") return true;

        // Denylist: skip obvious junk/temp files.
        // (Design choice: minimal denylist; cook should package authoritative DDC outputs.)
        if (name == ".DS_Store") return false;
        if (name == "Thumbs.db") return false;

        // Skip editor temp/lock patterns if they exist.
        const std::string s = p.string();
        if (s.find(".tmp") != std::string::npos) return false;
        if (s.find(".lock") != std::string::npos) return false;

        // Otherwise: include ALL DDC artifacts.
        return true;
    }


    std::string Phase12CookPack::ToRelSlash_(const std::filesystem::path& p) {
        std::string s = p.generic_string(); // already '/'
        while (!s.empty() && s.front() == '/') s.erase(s.begin());
        return s;
    }

    static bool WriteCookManifest_(const std::filesystem::path& cookedRoot,
        const std::vector<CookFileInfo>& files,
        std::string* outError) {
        const auto manifestPath = cookedRoot / "CookManifest.json";
        std::ofstream f(manifestPath, std::ios::binary | std::ios::trunc);
        if (!f) {
            if (outError) *outError = "Failed to write manifest: " + manifestPath.string();
            return false;
        }

        noc::JsonWriter w(f);
        w.BeginObject();

        w.KeyUInt("formatVersion", 1);
        w.KeyString("tool", "NocturneHost Phase12CookPack");
        w.KeyUInt("toolVersion", 1);

        w.Key("files");
        w.BeginArray();
        for (const auto& it : files) {
            w.BeginObject();
            w.KeyString("path", it.relPath);
            w.KeyUInt("sizeBytes", it.sizeBytes);
            w.KeyUInt("crc32", it.crc32);
            w.EndObject();
        }
        w.EndArray();

        w.EndObject();
        return true;
    }

    bool Phase12CookPack::Cook(const CookOptions& opt, std::vector<CookFileInfo>* outFiles, std::string* outError) {
        if (outFiles) outFiles->clear();

        const std::filesystem::path ddc = opt.ddcRoot;
        const std::filesystem::path cooked = opt.cookedRoot;
        const std::filesystem::path cookedContent = cooked / "Content";

        std::error_code ec;
        if (!std::filesystem::exists(ddc, ec)) {
            if (outError) *outError = "DDC root not found: " + ddc.string();
            return false;
        }

        if (opt.cleanCooked) {
            if (!RemoveAllSafe_(cooked)) {
                if (outError) *outError = "Failed to clean cooked root: " + cooked.string();
                return false;
            }
        }

        std::filesystem::create_directories(cookedContent, ec);
        if (ec) {
            if (outError) *outError = "Failed to create cooked root: " + cookedContent.string();
            return false;
        }

        // Collect files in deterministic order
        std::vector<std::filesystem::path> files;
        for (auto& it : std::filesystem::recursive_directory_iterator(ddc)) {
            if (!it.is_regular_file()) continue;
            const auto p = it.path();
            if (!IsWhitelistedArtifact_(p)) continue;
            files.push_back(p);
        }
        std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) {
            return a.generic_string() < b.generic_string();
            });

        std::vector<CookFileInfo> cookedFiles;
        cookedFiles.reserve(files.size());

        // Stage files
        for (const auto& src : files) {
            const auto rel = std::filesystem::relative(src, ddc, ec);
            if (ec) {
                if (outError) *outError = "Failed to compute relative path for: " + src.string();
                return false;
            }

            const auto dst = cookedContent / rel;
            if (!CopyFileEnsureDirs_(src, dst, outError)) return false;

            uint64_t size = 0;
            std::string crcErr;
            const uint32_t crc = ZipWriter::Crc32File(dst, &size, &crcErr);
            if (!crcErr.empty()) {
                if (outError) *outError = crcErr;
                return false;
            }

            CookFileInfo info{};
            info.relPath = ToRelSlash_(std::filesystem::path("Content") / rel);
            info.sizeBytes = size;
            info.crc32 = crc;
            cookedFiles.push_back(std::move(info));
        }

        // Also write manifest (outside Content/)
        if (!WriteCookManifest_(cooked, cookedFiles, outError)) return false;

        NOC_LOG_INFO("Cook", "Cook: scan='%s' -> staged='%s' files=%zu",
            ddc.generic_string().c_str(),
            cookedContent.generic_string().c_str(),
            cookedFiles.size());
        NOC_LOG_INFO("Cook", "Wrote manifest: %s", (cooked / "CookManifest.json").generic_string().c_str());

        if (outFiles) *outFiles = std::move(cookedFiles);
        return true;
    }

    bool Phase12CookPack::Pack(const PackOptions& opt, std::string* outError) {
        const std::filesystem::path cooked = opt.cookedRoot;
        const std::filesystem::path cookedContent = cooked / "Content";
        const std::filesystem::path manifestPath = cooked / "CookManifest.json";
        const std::filesystem::path outZip = opt.outZip;

        std::error_code ec;
        if (!std::filesystem::exists(cookedContent, ec)) {
            if (outError) *outError = "Cooked content folder missing: " + cookedContent.string();
            return false;
        }
        if (!std::filesystem::exists(manifestPath, ec)) {
            if (outError) *outError = "Cook manifest missing: " + manifestPath.string();
            return false;
        }

        // Collect all files under cooked/Content in deterministic order
        std::vector<std::filesystem::path> files;
        for (auto& it : std::filesystem::recursive_directory_iterator(cookedContent)) {
            if (!it.is_regular_file()) continue;
            files.push_back(it.path());
        }
        std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) {
            return a.generic_string() < b.generic_string();
            });

        std::vector<ZipWriter::EntryInfo> entries;
        entries.reserve(files.size() + 1);

        // Add manifest at root
        entries.push_back(ZipWriter::EntryInfo{
            .name = "CookManifest.json",
            .srcPath = manifestPath
            });

        // Add content files preserving relative paths under Content/
        for (const auto& p : files) {
            auto rel = std::filesystem::relative(p, cooked, ec); // yields "Content/..."
            if (ec) {
                if (outError) *outError = "Failed to compute relative path for: " + p.string();
                return false;
            }
            entries.push_back(ZipWriter::EntryInfo{
                .name = rel.generic_string(), // already '/'
                .srcPath = p
                });
        }

        std::string zipErr;
        if (!ZipWriter::WriteStoredZip(outZip, entries, &zipErr)) {
            if (outError) *outError = zipErr.empty() ? "ZipWriter failed" : zipErr;
            return false;
        }

        NOC_LOG_INFO("Pack", "Pack: input='%s' -> %s files=%zu",
            cookedContent.generic_string().c_str(),
            outZip.generic_string().c_str(),
            entries.size());
        return true;
    }

} // namespace noc::tools
