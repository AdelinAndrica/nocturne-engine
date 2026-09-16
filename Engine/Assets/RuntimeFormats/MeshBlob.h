#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "Assets/IntermediateAssets.h"

namespace noc {

    struct MeshBlobHeader {
        uint32_t magic = 0x48534D4E; // 'NMSH'
        uint16_t version = 1;
        uint16_t reserved = 0;

        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;

        uint32_t hasNormals = 0;
        uint32_t hasTangents = 0;
        uint32_t hasUvs = 0;

        uint32_t submeshCount = 0;
    };

    struct MeshBlobSubmesh {
        uint32_t indexOffset = 0;
        uint32_t indexCount = 0;
        uint32_t materialSlot = 0;
        uint32_t pad = 0;
    };

    bool WriteMeshBlob(const std::string& physicalPath, const IntermediateMesh& m, std::string* outError);
    bool ReadMeshBlob(const uint8_t* bytes, size_t size, IntermediateMesh* out, std::string* outError);

} // namespace noc
