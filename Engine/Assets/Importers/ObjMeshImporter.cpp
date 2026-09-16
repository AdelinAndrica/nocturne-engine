#include "Assets/Importers/ObjMeshImporter.h"

#include <chrono>
#include <cctype>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace noc {

    static uint64_t NowUtcMs_() {
        using namespace std::chrono;
        return (uint64_t)duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    }

    bool ObjMeshImporter::CanImportExtension(std::string_view extLower) const {
        return extLower == "obj";
    }

    ImportResult ObjMeshImporter::Import(const ImportRequest& req) {
        ImportResult r{};
        r.ok = true;
        r.importerId = std::string(Id());
        r.importerVersion = Version();
        r.metadata.importTimestampUtcMs = NowUtcMs_();
        r.metadata.importerId = r.importerId;
        r.metadata.importerVersion = r.importerVersion;
        return r;
    }

    struct ObjIndexKey {
        int v = 0;
        int vt = 0;
        int vn = 0;
        bool operator==(const ObjIndexKey& o) const { return v == o.v && vt == o.vt && vn == o.vn; }
    };

    struct ObjIndexKeyHash {
        size_t operator()(const ObjIndexKey& k) const noexcept {
            // simple combine
            size_t h = (size_t)k.v * 73856093u;
            h ^= (size_t)k.vt * 19349663u;
            h ^= (size_t)k.vn * 83492791u;
            return h;
        }
    };

    static void SkipSpaces_(const char*& p, const char* end) {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r')) ++p;
    }

    static bool ReadLine_(const char*& p, const char* end, const char*& outLine, const char*& outLineEnd) {
        if (p >= end) return false;
        outLine = p;
        while (p < end && *p != '\n') ++p;
        outLineEnd = p;
        if (p < end && *p == '\n') ++p;
        return true;
    }

    static bool ParseFloat_(const char*& p, const char* end, float* out) {
        SkipSpaces_(p, end);
        if (p >= end) return false;
        char* next = nullptr;
        std::string tmp(p, end);
        float v = std::strtof(tmp.c_str(), &next);
        if (!next || next == tmp.c_str()) return false;
        size_t consumed = (size_t)(next - tmp.c_str());
        p += consumed;
        *out = v;
        return true;
    }

    static bool ParseInt_(const char*& p, const char* end, int* out) {
        SkipSpaces_(p, end);
        if (p >= end) return false;
        char* next = nullptr;
        std::string tmp(p, end);
        long v = std::strtol(tmp.c_str(), &next, 10);
        if (!next || next == tmp.c_str()) return false;
        size_t consumed = (size_t)(next - tmp.c_str());
        p += consumed;
        *out = (int)v;
        return true;
    }

    static int FixIndex_(int idx, int count) {
        // OBJ is 1-based; negative counts from end
        if (idx > 0) return idx - 1;
        if (idx < 0) return count + idx;
        return -1;
    }

    static bool ParseFaceVertex_(const char*& p, const char* end, int* v, int* vt, int* vn) {
        *v = *vt = *vn = 0;
        if (!ParseInt_(p, end, v)) return false;

        if (p < end && *p == '/') {
            ++p;
            if (p < end && *p != '/') {
                ParseInt_(p, end, vt);
            }
            if (p < end && *p == '/') {
                ++p;
                ParseInt_(p, end, vn);
            }
        }
        return true;
    }

    ImportResult ObjMeshImporter::ImportFromMemory(const ImportRequest& req, const uint8_t* bytes, size_t size) {
        ImportResult r{};
        r.importerId = std::string(Id());
        r.importerVersion = Version();

        if (!bytes || size == 0) {
            r.ok = false; r.error = "ObjImporter: empty input";
            return r;
        }

        const char* p = (const char*)bytes;
        const char* end = (const char*)bytes + size;

        std::vector<float> pos; pos.reserve(1024);
        std::vector<float> nrm; nrm.reserve(1024);
        std::vector<float> uv;  uv.reserve(1024);

        std::unordered_map<ObjIndexKey, uint32_t, ObjIndexKeyHash> remap;
        remap.reserve(2048);

        IntermediateMesh mesh{};

        std::vector<uint32_t> faceIdx;

        while (p < end) {
            const char* line = nullptr;
            const char* lineEnd = nullptr;
            if (!ReadLine_(p, end, line, lineEnd)) break;

            const char* s = line;
            SkipSpaces_(s, lineEnd);
            if (s >= lineEnd) continue;
            if (*s == '#') continue;

            // token
            if ((lineEnd - s) >= 2 && s[0] == 'v' && std::isspace((unsigned char)s[1])) {
                s += 1;
                float x = 0, y = 0, z = 0;
                if (!ParseFloat_(s, lineEnd, &x) || !ParseFloat_(s, lineEnd, &y) || !ParseFloat_(s, lineEnd, &z)) continue;
                pos.push_back(x); pos.push_back(y); pos.push_back(z);
            }
            else if ((lineEnd - s) >= 3 && s[0] == 'v' && s[1] == 't' && std::isspace((unsigned char)s[2])) {
                s += 2;
                float u0 = 0, v0 = 0;
                if (!ParseFloat_(s, lineEnd, &u0) || !ParseFloat_(s, lineEnd, &v0)) continue;
                uv.push_back(u0); uv.push_back(v0);
            }
            else if ((lineEnd - s) >= 3 && s[0] == 'v' && s[1] == 'n' && std::isspace((unsigned char)s[2])) {
                s += 2;
                float x = 0, y = 0, z = 0;
                if (!ParseFloat_(s, lineEnd, &x) || !ParseFloat_(s, lineEnd, &y) || !ParseFloat_(s, lineEnd, &z)) continue;
                nrm.push_back(x); nrm.push_back(y); nrm.push_back(z);
            }
            else if ((lineEnd - s) >= 2 && s[0] == 'f' && std::isspace((unsigned char)s[1])) {
                s += 1;
                faceIdx.clear();

                // read N vertices of the face
                while (true) {
                    SkipSpaces_(s, lineEnd);
                    if (s >= lineEnd) break;

                    int iv = 0, ivt = 0, ivn = 0;
                    if (!ParseFaceVertex_(s, lineEnd, &iv, &ivt, &ivn)) break;

                    const int vCount = (int)(pos.size() / 3);
                    const int vtCount = (int)(uv.size() / 2);
                    const int vnCount = (int)(nrm.size() / 3);

                    ObjIndexKey k{};
                    k.v = FixIndex_(iv, vCount);
                    k.vt = (ivt != 0) ? FixIndex_(ivt, vtCount) : -1;
                    k.vn = (ivn != 0) ? FixIndex_(ivn, vnCount) : -1;

                    if (k.v < 0 || k.v >= vCount) { r.ok = false; r.error = "ObjImporter: bad position index"; return r; }
                    if (k.vt >= vtCount) k.vt = -1;
                    if (k.vn >= vnCount) k.vn = -1;

                    auto it = remap.find(k);
                    uint32_t outIndex = 0;
                    if (it == remap.end()) {
                        outIndex = (uint32_t)(mesh.positions.size() / 3);

                        mesh.positions.push_back(pos[k.v * 3 + 0]);
                        mesh.positions.push_back(pos[k.v * 3 + 1]);
                        mesh.positions.push_back(pos[k.v * 3 + 2]);

                        if (k.vn >= 0) {
                            mesh.normals.push_back(nrm[k.vn * 3 + 0]);
                            mesh.normals.push_back(nrm[k.vn * 3 + 1]);
                            mesh.normals.push_back(nrm[k.vn * 3 + 2]);
                        }

                        if (k.vt >= 0) {
                            mesh.uvs.push_back(uv[k.vt * 2 + 0]);
                            mesh.uvs.push_back(uv[k.vt * 2 + 1]);
                        }

                        remap.emplace(k, outIndex);
                    }
                    else {
                        outIndex = it->second;
                    }

                    faceIdx.push_back(outIndex);
                }

                // triangulate fan if >3
                if (faceIdx.size() >= 3) {
                    for (size_t i = 1; i + 1 < faceIdx.size(); ++i) {
                        mesh.indices.push_back(faceIdx[0]);
                        mesh.indices.push_back(faceIdx[i]);
                        mesh.indices.push_back(faceIdx[i + 1]);
                    }
                }
            }
        }

        if (mesh.positions.empty() || mesh.indices.empty()) {
            r.ok = false; r.error = "ObjImporter: produced empty mesh";
            return r;
        }

        // If normals/uvs missing for some vertices, clear the stream for consistency (Phase 11 policy)
        const uint32_t vcount = (uint32_t)(mesh.positions.size() / 3);
        if (mesh.normals.size() / 3 != vcount) mesh.normals.clear();
        if (mesh.uvs.size() / 2 != vcount) mesh.uvs.clear();

        // default single submesh
        mesh.submeshes.clear();
        mesh.submeshes.push_back(IntermediateSubmesh{ 0, (uint32_t)mesh.indices.size(), 0 });
        mesh.materialSlots = { "Default" };

        r.asset = std::move(mesh);
        r.ok = true;

        r.metadata.importTimestampUtcMs = NowUtcMs_();
        r.metadata.importerId = r.importerId;
        r.metadata.importerVersion = r.importerVersion;

        // Dependencies: OBJ may reference MTL, but Phase 11 subset ignores mtllib.
        return r;
    }

} // namespace noc
