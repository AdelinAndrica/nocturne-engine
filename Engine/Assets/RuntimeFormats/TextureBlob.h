#pragma once
#include <cstdint>
#include <string>

#include "Assets/IntermediateAssets.h"

namespace noc {

    struct TextureBlobHeader {
        uint32_t magic = 0x5845544E; // 'NTEX'
        uint16_t version = 1;
        uint16_t reserved = 0;

        uint32_t format = 0;      // IntermediateTextureFormat
        uint32_t colorSpace = 0;  // IntermediateColorSpace
        uint32_t mipCount = 0;
    };

    bool WriteTextureBlob(const std::string& physicalPath, const IntermediateTexture& t, std::string* outError);
    bool ReadTextureBlob(const uint8_t* bytes, size_t size, IntermediateTexture* out, std::string* outError);

} // namespace noc
