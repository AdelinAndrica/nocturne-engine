#include "Assets/RuntimeFormats/MaterialBlob.h"

#include <cstring>
#include <fstream>
#include <vector>

namespace noc {

    static void AppendU32_(std::vector<uint8_t>& dst, uint32_t v) {
        dst.insert(dst.end(), (uint8_t*)&v, (uint8_t*)&v + 4);
    }
    static void AppendF_(std::vector<uint8_t>& dst, const float* f, size_t n) {
        dst.insert(dst.end(), (uint8_t*)f, (uint8_t*)f + n * sizeof(float));
    }
    static void AppendStr_(std::vector<uint8_t>& dst, const std::string& s) {
        AppendU32_(dst, (uint32_t)s.size());
        dst.insert(dst.end(), (const uint8_t*)s.data(), (const uint8_t*)s.data() + s.size());
    }

    bool WriteMaterialBlob(const std::string& physicalPath, const IntermediateMaterial& m, std::string* outError) {
        MaterialBlobHeader h{};
        std::vector<uint8_t> blob;
        blob.insert(blob.end(), (uint8_t*)&h, (uint8_t*)&h + sizeof(h));

        AppendF_(blob, m.baseColor, 4);
        AppendF_(blob, &m.metallic, 1);
        AppendF_(blob, &m.roughness, 1);

        AppendStr_(blob, m.baseColorTexture);
        AppendStr_(blob, m.normalTexture);
        AppendStr_(blob, m.ormTexture);

        std::ofstream f(physicalPath, std::ios::binary);
        if (!f) { if (outError) *outError = "MaterialBlob: open failed"; return false; }
        f.write((const char*)blob.data(), (std::streamsize)blob.size());
        return true;
    }

    static bool ReadStr_(const uint8_t* bytes, size_t size, size_t* off, std::string* out) {
        if (*off + 4 > size) return false;
        uint32_t n = 0;
        std::memcpy(&n, bytes + *off, 4);
        *off += 4;
        if (*off + n > size) return false;
        out->assign((const char*)(bytes + *off), (size_t)n);
        *off += n;
        return true;
    }

    bool ReadMaterialBlob(const uint8_t* bytes, size_t size, IntermediateMaterial* out, std::string* outError) {
        if (!bytes || size < sizeof(MaterialBlobHeader) || !out) {
            if (outError) *outError = "MaterialBlob: invalid input";
            return false;
        }
        MaterialBlobHeader h{};
        std::memcpy(&h, bytes, sizeof(h));
        if (h.magic != 0x54414D4E || h.version != 1) {
            if (outError) *outError = "MaterialBlob: bad header";
            return false;
        }
        size_t off = sizeof(h);

        if (off + 24 > size) { if (outError) *outError = "MaterialBlob: truncated floats"; return false; }
        std::memcpy(out->baseColor, bytes + off, 16); off += 16;
        std::memcpy(&out->metallic, bytes + off, 4); off += 4;
        std::memcpy(&out->roughness, bytes + off, 4); off += 4;

        if (!ReadStr_(bytes, size, &off, &out->baseColorTexture)) { if (outError) *outError = "MaterialBlob: bad baseColorTexture"; return false; }
        if (!ReadStr_(bytes, size, &off, &out->normalTexture)) { if (outError) *outError = "MaterialBlob: bad normalTexture"; return false; }
        if (!ReadStr_(bytes, size, &off, &out->ormTexture)) { if (outError) *outError = "MaterialBlob: bad ormTexture"; return false; }

        return true;
    }

} // namespace noc
