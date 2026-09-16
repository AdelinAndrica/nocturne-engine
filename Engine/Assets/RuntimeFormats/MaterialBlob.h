#pragma once
#include <cstdint>
#include <string>

#include "Assets/IntermediateAssets.h"

namespace noc {

    struct MaterialBlobHeader {
        uint32_t magic = 0x54414D4E; // 'NMAT'
        uint16_t version = 1;
        uint16_t reserved = 0;
    };

    bool WriteMaterialBlob(const std::string& physicalPath, const IntermediateMaterial& m, std::string* outError);
    bool ReadMaterialBlob(const uint8_t* bytes, size_t size, IntermediateMaterial* out, std::string* outError);

} // namespace noc
