#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace noc::tools {

    struct CookFileInfo {
        std::string relPath; // using '/'
        uint64_t sizeBytes = 0;
        uint32_t crc32 = 0;
    };

    // Phase 12: minimal cooker/packager implemented inside host tooling.
    //
    // Design choice (not directly from the book):
    // - Cooked/ staging folder and NocturneContent.zip naming are project conventions for now.
    class Phase12CookPack {
    public:
        struct CookOptions {
            std::filesystem::path ddcRoot = "DerivedDataCache";
            std::filesystem::path cookedRoot = "Cooked";
            bool cleanCooked = true;
        };

        struct PackOptions {
            std::filesystem::path cookedRoot = "Cooked";
            std::filesystem::path outZip = "Cooked/NocturneContent.zip";
        };

        static bool Cook(const CookOptions& opt, std::vector<CookFileInfo>* outFiles, std::string* outError);
        static bool Pack(const PackOptions& opt, std::string* outError);

    private:
        static bool IsWhitelistedArtifact_(const std::filesystem::path& p);
        static std::string ToRelSlash_(const std::filesystem::path& p);
    };

} // namespace noc::tools
