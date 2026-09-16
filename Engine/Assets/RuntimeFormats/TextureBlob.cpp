#include "Assets/RuntimeFormats/TextureBlob.h"

#include <cstring>
#include <fstream>
#include <vector>

namespace noc {

    static void Append_(std::vector<uint8_t>& dst, const void* p, size_t n) {
        const uint8_t* b = (const uint8_t*)p;
        dst.insert(dst.end(), b, b + n);
    }

    bool WriteTextureBlob(const std::string& physicalPath, const IntermediateTexture& t, std::string* outError) {
        if (t.mips.empty()) {
            if (outError) *outError = "TextureBlob: no mips";
            return false;
        }
        TextureBlobHeader h{};
        h.format = (uint32_t)t.format;
        h.colorSpace = (uint32_t)t.colorSpace;
        h.mipCount = (uint32_t)t.mips.size();

        std::vector<uint8_t> blob;
        Append_(blob, &h, sizeof(h));

        for (const auto& m : t.mips) {
            Append_(blob, &m.width, sizeof(m.width));
            Append_(blob, &m.height, sizeof(m.height));
            uint32_t dataSize = (uint32_t)m.pixels.size();
            Append_(blob, &dataSize, sizeof(dataSize));
            if (dataSize) Append_(blob, m.pixels.data(), m.pixels.size());
        }

        std::ofstream f(physicalPath, std::ios::binary);
        if (!f) { if (outError) *outError = "TextureBlob: open failed"; return false; }
        f.write((const char*)blob.data(), (std::streamsize)blob.size());
        return true;
    }

    bool ReadTextureBlob(const uint8_t* bytes, size_t size, IntermediateTexture* out, std::string* outError) {
        if (!bytes || size < sizeof(TextureBlobHeader) || !out) {
            if (outError) *outError = "TextureBlob: invalid input";
            return false;
        }
        TextureBlobHeader h{};
        std::memcpy(&h, bytes, sizeof(h));
        if (h.magic != 0x5845544E || h.version != 1) {
            if (outError) *outError = "TextureBlob: bad header";
            return false;
        }
        out->format = (IntermediateTextureFormat)h.format;
        out->colorSpace = (IntermediateColorSpace)h.colorSpace;
        out->mips.clear();

        size_t off = sizeof(h);
        for (uint32_t i = 0; i < h.mipCount; ++i) {
            if (off + 12 > size) { if (outError) *outError = "TextureBlob: truncated"; return false; }
            IntermediateMip m{};
            std::memcpy(&m.width, bytes + off, 4); off += 4;
            std::memcpy(&m.height, bytes + off, 4); off += 4;
            uint32_t dataSize = 0;
            std::memcpy(&dataSize, bytes + off, 4); off += 4;
            if (off + dataSize > size) { if (outError) *outError = "TextureBlob: truncated pixels"; return false; }
            m.pixels.resize(dataSize);
            if (dataSize) std::memcpy(m.pixels.data(), bytes + off, dataSize);
            off += dataSize;
            out->mips.push_back(std::move(m));
        }
        return true;
    }

} // namespace noc
