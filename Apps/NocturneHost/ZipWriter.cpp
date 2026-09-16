#include "ZipWriter.h"

#include <array>
#include <cstdio>
#include <fstream>
#include <vector>

namespace noc::tools {

#pragma pack(push, 1)
    struct ZipLocalHeader {
        uint32_t signature = 0x04034b50;
        uint16_t versionNeeded = 20;
        uint16_t flags = 0;
        uint16_t compressionMethod = 0; // stored
        uint16_t modTime = 0;           // deterministic
        uint16_t modDate = 0;           // deterministic
        uint32_t crc32 = 0;
        uint32_t compressedSize = 0;
        uint32_t uncompressedSize = 0;
        uint16_t fileNameLen = 0;
        uint16_t extraLen = 0;
    };

    struct ZipCentralDirHeader {
        uint32_t signature = 0x02014b50;
        uint16_t versionMadeBy = 20;
        uint16_t versionNeeded = 20;
        uint16_t flags = 0;
        uint16_t compressionMethod = 0; // stored
        uint16_t modTime = 0;
        uint16_t modDate = 0;
        uint32_t crc32 = 0;
        uint32_t compressedSize = 0;
        uint32_t uncompressedSize = 0;
        uint16_t fileNameLen = 0;
        uint16_t extraLen = 0;
        uint16_t commentLen = 0;
        uint16_t diskStart = 0;
        uint16_t internalAttr = 0;
        uint32_t externalAttr = 0;
        uint32_t localHeaderOffset = 0;
    };

    struct ZipEOCD {
        uint32_t signature = 0x06054b50;
        uint16_t diskNumber = 0;
        uint16_t centralDirDisk = 0;
        uint16_t centralDirRecordsOnDisk = 0;
        uint16_t centralDirRecordsTotal = 0;
        uint32_t centralDirSize = 0;
        uint32_t centralDirOffset = 0;
        uint16_t commentLength = 0;
    };
#pragma pack(pop)

    static std::array<uint32_t, 256> g_crcTable{};
    static bool g_crcInit = false;

    static inline void WriteLE16_(std::ostream& os, uint16_t v) {
        os.put((char)(v & 0xFF));
        os.put((char)((v >> 8) & 0xFF));
    }
    static inline void WriteLE32_(std::ostream& os, uint32_t v) {
        os.put((char)(v & 0xFF));
        os.put((char)((v >> 8) & 0xFF));
        os.put((char)((v >> 16) & 0xFF));
        os.put((char)((v >> 24) & 0xFF));
    }

    void ZipWriter::InitCrcTable_() {
        if (g_crcInit) return;
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (uint32_t k = 0; k < 8; ++k) {
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            }
            g_crcTable[i] = c;
        }
        g_crcInit = true;
    }

    uint32_t ZipWriter::Crc32Bytes_(const uint8_t* data, size_t size, uint32_t seed) {
        InitCrcTable_();
        uint32_t c = seed ^ 0xFFFFFFFFu;
        for (size_t i = 0; i < size; ++i) {
            c = g_crcTable[(c ^ data[i]) & 0xFFu] ^ (c >> 8);
        }
        return c ^ 0xFFFFFFFFu;
    }

    uint32_t ZipWriter::Crc32File(const std::filesystem::path& file, uint64_t* outSizeBytes, std::string* outError) {
        if (outSizeBytes) *outSizeBytes = 0;

        std::ifstream in(file, std::ios::binary);
        if (!in) {
            if (outError) *outError = "Failed to open file for CRC32: " + file.string();
            return 0;
        }

        constexpr size_t kBufSize = 64 * 1024;
        std::vector<uint8_t> buf(kBufSize);

        uint32_t crc = 0;
        uint64_t total = 0;

        while (in) {
            in.read((char*)buf.data(), (std::streamsize)buf.size());
            const std::streamsize got = in.gcount();
            if (got > 0) {
                crc = Crc32Bytes_(buf.data(), (size_t)got, crc);
                total += (uint64_t)got;
            }
        }

        if (outSizeBytes) *outSizeBytes = total;
        return crc;
    }

    static std::string NormalizeZipName_(std::string_view s) {
        std::string out(s);
        for (char& c : out) {
            if (c == '\\') c = '/';
        }
        // No leading slash.
        while (!out.empty() && out.front() == '/') out.erase(out.begin());
        return out;
    }

    bool ZipWriter::WriteStoredZip(const std::filesystem::path& outZipPath,
        const std::vector<EntryInfo>& entries,
        std::string* outError) {
        // Ensure parent dir exists
        std::error_code ec;
        std::filesystem::create_directories(outZipPath.parent_path(), ec);

        std::ofstream os(outZipPath, std::ios::binary | std::ios::trunc);
        if (!os) {
            if (outError) *outError = "Failed to create zip: " + outZipPath.string();
            return false;
        }

        struct CDRec {
            ZipCentralDirHeader cd{};
            std::string name;
        };
        std::vector<CDRec> cds;
        cds.reserve(entries.size());

        // Write locals + data
        for (const auto& e : entries) {
            const std::string name = NormalizeZipName_(e.name);
            if (name.empty()) continue;

            uint64_t size = 0;
            std::string crcErr;
            const uint32_t crc = Crc32File(e.srcPath, &size, &crcErr);
            if (!crcErr.empty()) {
                if (outError) *outError = crcErr;
                return false;
            }
            if (size > 0xFFFFFFFFull) {
                if (outError) *outError = "File too large for Zip32: " + e.srcPath.string();
                return false;
            }

            const auto localOffset = (uint32_t)os.tellp();

            ZipLocalHeader lh{};
            lh.compressionMethod = 0;
            lh.modTime = 0;
            lh.modDate = 0;
            lh.crc32 = crc;
            lh.compressedSize = (uint32_t)size;
            lh.uncompressedSize = (uint32_t)size;
            lh.fileNameLen = (uint16_t)name.size();
            lh.extraLen = 0;

            // Write local header (packed)
            WriteLE32_(os, lh.signature);
            WriteLE16_(os, lh.versionNeeded);
            WriteLE16_(os, lh.flags);
            WriteLE16_(os, lh.compressionMethod);
            WriteLE16_(os, lh.modTime);
            WriteLE16_(os, lh.modDate);
            WriteLE32_(os, lh.crc32);
            WriteLE32_(os, lh.compressedSize);
            WriteLE32_(os, lh.uncompressedSize);
            WriteLE16_(os, lh.fileNameLen);
            WriteLE16_(os, lh.extraLen);

            os.write(name.data(), (std::streamsize)name.size());

            // Write file bytes
            std::ifstream in(e.srcPath, std::ios::binary);
            if (!in) {
                if (outError) *outError = "Failed to open file for zip write: " + e.srcPath.string();
                return false;
            }
            constexpr size_t kBufSize = 64 * 1024;
            std::vector<char> buf(kBufSize);
            while (in) {
                in.read(buf.data(), (std::streamsize)buf.size());
                const std::streamsize got = in.gcount();
                if (got > 0) os.write(buf.data(), got);
            }

            // Record central directory entry
            CDRec rec{};
            rec.name = name;

            ZipCentralDirHeader cd{};
            cd.compressionMethod = 0;
            cd.modTime = 0;
            cd.modDate = 0;
            cd.crc32 = crc;
            cd.compressedSize = (uint32_t)size;
            cd.uncompressedSize = (uint32_t)size;
            cd.fileNameLen = (uint16_t)name.size();
            cd.extraLen = 0;
            cd.commentLen = 0;
            cd.localHeaderOffset = localOffset;
            rec.cd = cd;

            cds.push_back(std::move(rec));
        }

        const uint32_t cdOffset = (uint32_t)os.tellp();

        // Write central directory
        for (const auto& r : cds) {
            const auto& cd = r.cd;

            WriteLE32_(os, cd.signature);
            WriteLE16_(os, cd.versionMadeBy);
            WriteLE16_(os, cd.versionNeeded);
            WriteLE16_(os, cd.flags);
            WriteLE16_(os, cd.compressionMethod);
            WriteLE16_(os, cd.modTime);
            WriteLE16_(os, cd.modDate);
            WriteLE32_(os, cd.crc32);
            WriteLE32_(os, cd.compressedSize);
            WriteLE32_(os, cd.uncompressedSize);
            WriteLE16_(os, cd.fileNameLen);
            WriteLE16_(os, cd.extraLen);
            WriteLE16_(os, cd.commentLen);
            WriteLE16_(os, cd.diskStart);
            WriteLE16_(os, cd.internalAttr);
            WriteLE32_(os, cd.externalAttr);
            WriteLE32_(os, cd.localHeaderOffset);

            os.write(r.name.data(), (std::streamsize)r.name.size());
        }

        const uint32_t cdEnd = (uint32_t)os.tellp();
        const uint32_t cdSize = cdEnd - cdOffset;

        if (cds.size() > 0xFFFFu) {
            if (outError) *outError = "Too many entries for Zip32 (no Zip64 support): " + outZipPath.string();
            return false;
        }

        // EOCD
        ZipEOCD eocd{};
        eocd.centralDirOffset = cdOffset;
        eocd.centralDirSize = cdSize;
        eocd.centralDirRecordsOnDisk = (uint16_t)cds.size();
        eocd.centralDirRecordsTotal = (uint16_t)cds.size();
        eocd.commentLength = 0;

        WriteLE32_(os, eocd.signature);
        WriteLE16_(os, eocd.diskNumber);
        WriteLE16_(os, eocd.centralDirDisk);
        WriteLE16_(os, eocd.centralDirRecordsOnDisk);
        WriteLE16_(os, eocd.centralDirRecordsTotal);
        WriteLE32_(os, eocd.centralDirSize);
        WriteLE32_(os, eocd.centralDirOffset);
        WriteLE16_(os, eocd.commentLength);

        os.flush();
        if (!os) {
            if (outError) *outError = "Zip write failed: " + outZipPath.string();
            return false;
        }
        return true;
    }

} // namespace noc::tools
