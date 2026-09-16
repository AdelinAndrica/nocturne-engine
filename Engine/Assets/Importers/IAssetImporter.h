#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "Assets/AssetMetadata.h"
#include "Assets/IntermediateAssets.h"

namespace noc {

    struct ImportOptions {
        // Phase 11: we hash the entire options blob deterministically.
        // Extend later.
        bool flipGreenNormal = false;
        bool forceLinearColor = false;
    };

    struct ImportRequest {
        std::string sourceVPathNormalized; // canonical vpath (preferred)
        std::string sourcePhysicalPath;    // optional OS path
        ImportOptions options{};
    };

    using IntermediateVariant = std::variant<
        IntermediateText,
        IntermediateMesh,
        IntermediateTexture,
        IntermediateMaterial,
        IntermediateScene
    >;

    struct ImportResult {
        bool ok = false;
        std::string error;

        std::string importerId;
        uint32_t importerVersion = 1;

        IntermediateVariant asset;
        AssetMetadata metadata;
    };

    class IAssetImporter {
    public:
        virtual ~IAssetImporter() = default;
        virtual std::string_view Id() const = 0;
        virtual uint32_t Version() const = 0;

        virtual bool CanImportExtension(std::string_view extLower) const = 0;

        // Parse+build intermediate (CPU-only).
        virtual ImportResult Import(const ImportRequest& req) = 0;
    };

} // namespace noc
