#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Assets/AssetId.h"

namespace noc {

    class AssetDependencyGraph {
    public:
        void Clear();

        void SetEdges(AssetId asset, const std::vector<AssetId>& deps);
        const std::vector<AssetId>* GetDeps(AssetId asset) const;

        // Mark dirty: traverse reverse edges (dependents) and return affected set including root.
        std::vector<AssetId> ComputeAffected(AssetId changed) const;

        // Topo order for a subset (Kahn). If cycle, returns empty.
        std::vector<AssetId> TopologicalOrder(const std::vector<AssetId>& subset) const;

        // Persistence (JSON) to disk path (physical).
        bool SaveJson(const std::string& physicalPath) const;
        bool LoadJson(const std::string& physicalPath);

    private:
        void RebuildReverse_();

    private:
        std::unordered_map<uint64_t, std::vector<AssetId>> edges_;
        std::unordered_map<uint64_t, std::vector<AssetId>> reverse_;
    };

} // namespace noc
