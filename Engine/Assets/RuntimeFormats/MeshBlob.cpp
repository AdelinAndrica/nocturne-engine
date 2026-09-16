#include "Assets/RuntimeFormats/MeshBlob.h"

#include <cstring>
#include <fstream>

namespace noc {

    static void Append_(std::vector<uint8_t>& dst, const void* p, size_t n) {
        const uint8_t* b = (const uint8_t*)p;
        dst.insert(dst.end(), b, b + n);
    }

    bool WriteMeshBlob(const std::string& physicalPath, const IntermediateMesh& m, std::string* outError) {
        const uint32_t vcount = (uint32_t)(m.positions.size() / 3);
        if (vcount == 0 || m.indices.empty()) {
            if (outError) *outError = "MeshBlob: empty mesh";
            return false;
        }

        MeshBlobHeader h{};
        h.vertexCount = vcount;
        h.indexCount = (uint32_t)m.indices.size();
        h.hasNormals = (m.normals.size() / 3 == vcount) ? 1u : 0u;
        h.hasTangents = (m.tangents.size() / 4 == vcount) ? 1u : 0u;
        h.hasUvs = (m.uvs.size() / 2 == vcount) ? 1u : 0u;
        h.submeshCount = (uint32_t)m.submeshes.size();

        std::vector<uint8_t> blob;
        blob.reserve(sizeof(h) + m.positions.size() * 4 + m.indices.size() * 4);

        Append_(blob, &h, sizeof(h));
        Append_(blob, m.positions.data(), m.positions.size() * sizeof(float));
        if (h.hasNormals)  Append_(blob, m.normals.data(), m.normals.size() * sizeof(float));
        if (h.hasTangents) Append_(blob, m.tangents.data(), m.tangents.size() * sizeof(float));
        if (h.hasUvs)      Append_(blob, m.uvs.data(), m.uvs.size() * sizeof(float));
        Append_(blob, m.indices.data(), m.indices.size() * sizeof(uint32_t));

        if (h.submeshCount > 0) {
            for (auto sm : m.submeshes) {
                MeshBlobSubmesh b{};
                b.indexOffset = sm.indexOffset;
                b.indexCount = sm.indexCount;
                b.materialSlot = sm.materialSlot;
                Append_(blob, &b, sizeof(b));
            }
        }

        std::ofstream f(physicalPath, std::ios::binary);
        if (!f) {
            if (outError) *outError = "MeshBlob: failed to open output file";
            return false;
        }
        f.write((const char*)blob.data(), (std::streamsize)blob.size());
        return true;
    }

    bool ReadMeshBlob(const uint8_t* bytes, size_t size, IntermediateMesh* out, std::string* outError) {
        if (!bytes || size < sizeof(MeshBlobHeader) || !out) {
            if (outError) *outError = "MeshBlob: invalid input";
            return false;
        }
        MeshBlobHeader h{};
        std::memcpy(&h, bytes, sizeof(h));
        if (h.magic != 0x48534D4E || h.version != 1) {
            if (outError) *outError = "MeshBlob: bad header";
            return false;
        }

        size_t off = sizeof(h);
        const size_t posBytes = (size_t)h.vertexCount * 3 * sizeof(float);
        if (off + posBytes > size) { if (outError) *outError = "MeshBlob: truncated positions"; return false; }
        out->positions.resize((size_t)h.vertexCount * 3);
        std::memcpy(out->positions.data(), bytes + off, posBytes);
        off += posBytes;

        if (h.hasNormals) {
            const size_t nBytes = (size_t)h.vertexCount * 3 * sizeof(float);
            if (off + nBytes > size) { if (outError) *outError = "MeshBlob: truncated normals"; return false; }
            out->normals.resize((size_t)h.vertexCount * 3);
            std::memcpy(out->normals.data(), bytes + off, nBytes);
            off += nBytes;
        }
        else out->normals.clear();

        if (h.hasTangents) {
            const size_t tBytes = (size_t)h.vertexCount * 4 * sizeof(float);
            if (off + tBytes > size) { if (outError) *outError = "MeshBlob: truncated tangents"; return false; }
            out->tangents.resize((size_t)h.vertexCount * 4);
            std::memcpy(out->tangents.data(), bytes + off, tBytes);
            off += tBytes;
        }
        else out->tangents.clear();

        if (h.hasUvs) {
            const size_t uvBytes = (size_t)h.vertexCount * 2 * sizeof(float);
            if (off + uvBytes > size) { if (outError) *outError = "MeshBlob: truncated uvs"; return false; }
            out->uvs.resize((size_t)h.vertexCount * 2);
            std::memcpy(out->uvs.data(), bytes + off, uvBytes);
            off += uvBytes;
        }
        else out->uvs.clear();

        const size_t idxBytes = (size_t)h.indexCount * sizeof(uint32_t);
        if (off + idxBytes > size) { if (outError) *outError = "MeshBlob: truncated indices"; return false; }
        out->indices.resize((size_t)h.indexCount);
        std::memcpy(out->indices.data(), bytes + off, idxBytes);
        off += idxBytes;

        out->submeshes.clear();
        if (h.submeshCount > 0) {
            const size_t smBytes = (size_t)h.submeshCount * sizeof(MeshBlobSubmesh);
            if (off + smBytes > size) { if (outError) *outError = "MeshBlob: truncated submeshes"; return false; }
            out->submeshes.resize(h.submeshCount);
            for (uint32_t i = 0; i < h.submeshCount; ++i) {
                MeshBlobSubmesh b{};
                std::memcpy(&b, bytes + off, sizeof(b));
                off += sizeof(b);
                out->submeshes[i] = IntermediateSubmesh{ b.indexOffset, b.indexCount, b.materialSlot };
            }
        }
        return true;
    }

} // namespace noc
