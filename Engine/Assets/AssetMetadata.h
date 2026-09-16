#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "Assets/AssetId.h"

namespace noc {

    struct AssetDependency {
        AssetId id{};
        std::string vpathNormalized;
        uint64_t fingerprint = 0; // dependency content/metadata fingerprint
    };

    struct AssetOutputArtifact {
        std::string vpath;     // virtual path for blob (what runtime loads via VFS)
        std::string type;      // "Mesh", "Texture", "Material", "Text"
        uint32_t version = 1;  // blob version
    };

    struct AssetMetadata {
        AssetId id{};
        std::string sourceVPathNormalized; // canonical vpath
        std::string sourcePhysicalPath;    // optional, if imported from OS path

        uint64_t sourceTimestampUtcMs = 0;
        uint64_t sourceContentHash = 0;

        std::string importerId;
        uint32_t importerVersion = 1;

        uint64_t optionsHash = 0;

        uint64_t importTimestampUtcMs = 0;

        std::vector<AssetDependency> dependencies;
        std::vector<AssetOutputArtifact> outputs;

        // Derived fingerprint used for cache checks: combines source+importer+options+deps.
        uint64_t buildFingerprint = 0;
    };

} // namespace noc
