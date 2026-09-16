#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace noc {

    // --- Intermediate assets are engine-owned representations produced by importers ---
    // They are NOT runtime blobs, and NOT vendor formats.

    struct IntermediateText {
        std::string text;
    };

    struct IntermediateSubmesh {
        uint32_t indexOffset = 0;
        uint32_t indexCount = 0;
        uint32_t materialSlot = 0;
    };

    struct IntermediateMesh {
        // Interleaved-ish streams (simple Phase 11 layout)
        std::vector<float> positions; // xyz xyz ...
        std::vector<float> normals;   // xyz ...
        std::vector<float> tangents;  // xyzw ...
        std::vector<float> uvs;       // uv uv ...
        std::vector<uint32_t> indices;

        std::vector<IntermediateSubmesh> submeshes;
        std::vector<std::string> materialSlots; // slot names (or vpaths later)
    };

    enum class IntermediateTextureFormat : uint8_t {
        Unknown = 0,
        BGRA8_UNorm,
    };

    enum class IntermediateColorSpace : uint8_t {
        Linear = 0,
        SRGB,
    };

    struct IntermediateMip {
        uint32_t width = 0;
        uint32_t height = 0;
        std::vector<uint8_t> pixels; // for BGRA8_UNorm: width*height*4
    };

    struct IntermediateTexture {
        IntermediateTextureFormat format = IntermediateTextureFormat::Unknown;
        IntermediateColorSpace colorSpace = IntermediateColorSpace::SRGB;
        std::vector<IntermediateMip> mips;
    };

    struct IntermediateMaterial {
        // minimal PBR-ish (Design choice)
        float baseColor[4] = { 1,1,1,1 };
        float metallic = 0.0f;
        float roughness = 1.0f;

        // vpaths (dependencies)
        std::string baseColorTexture;
        std::string normalTexture;
        std::string ormTexture; // occlusion/roughness/metallic packed
    };

    struct IntermediateNode {
        std::string name;
        float localTRS[10] = { // t(3) r(quat4) s(3)
            0,0,0,  0,0,0,1,  1,1,1
        };
        int32_t parent = -1;

        // references to assets (vpaths)
        std::string mesh;
        std::string material;
    };

    struct IntermediateScene {
        std::vector<IntermediateNode> nodes;
    };

} // namespace noc
