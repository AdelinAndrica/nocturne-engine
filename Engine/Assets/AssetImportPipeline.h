#pragma once
#include <string>
#include <string_view>

#include "Assets/AssetDependencyGraph.h"
#include "Assets/Importers/AssetImporterRegistry.h"

namespace noc {

    class Engine;
    class VirtualFileSystem;
    class JobSystem;

    class AssetImportPipeline {
    public:
        bool Init(Engine& engine);
        void Shutdown();

        bool ImportOne(std::string_view pathOrVpath);
        bool ImportAll();

        const AssetDependencyGraph& Graph() const { return graph_; }

    private:
        bool ImportResolved_(const std::string& vpathNormalized, const std::string& physicalPathOrEmpty);

        bool ReadSourceBytes_(const std::string& vpathNormalized,
            const std::string& physicalPathOrEmpty,
            std::vector<uint8_t>* outBytes,
            uint64_t* outTimestampUtcMs,
            uint64_t* outHash);

        bool WriteMetadataJson_(const std::string& physicalMetaPath, const AssetMetadata& md);

        std::string DdcPhysicalRoot_() const;
        std::string DdcGraphPhysicalPath_() const;

        std::string MakeDdcPhysicalPathForSource_(const std::string& vpathNormalized, std::string_view suffix) const;

        static std::string ToLower_(std::string s);
        static std::string ExtensionLower_(std::string_view path);

        static uint64_t Hash64_FNV1a_(const void* data, size_t size);
        static uint64_t Hash64_Combine_(uint64_t a, uint64_t b);

    private:
        Engine* engine_ = nullptr;
        VirtualFileSystem* vfs_ = nullptr;
        JobSystem* jobs_ = nullptr;

        AssetImporterRegistry registry_;
        AssetDependencyGraph graph_;
    };

} // namespace noc
