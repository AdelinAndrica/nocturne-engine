#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace noc::tools {

    // Minimal ZIP writer that produces "stored" (compression method 0) entries only.
    // Deterministic policy:
    // - entry order is caller-defined (caller should sort)
    // - DOS mod time/date are zeroed
    // - no extra fields, no comments
    class ZipWriter {
    public:
        struct EntryInfo {
            std::string name;              // path inside zip, must use '/'
            std::filesystem::path srcPath; // physical file path
        };

        // Create zip at outZipPath, overwriting if exists.
        // Returns false on any IO error.
        static bool WriteStoredZip(const std::filesystem::path& outZipPath,
            const std::vector<EntryInfo>& entries,
            std::string* outError);

        // CRC32 (IEEE) helper (exposed for manifest).
        static uint32_t Crc32File(const std::filesystem::path& file, uint64_t* outSizeBytes, std::string* outError);

    private:
        static uint32_t Crc32Bytes_(const uint8_t* data, size_t size, uint32_t seed);
        static void InitCrcTable_();
    };

} // namespace noc::tools
