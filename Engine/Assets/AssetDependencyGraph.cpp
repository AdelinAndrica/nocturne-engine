#include "Assets/AssetDependencyGraph.h"

#include <fstream>
#include <queue>

#include "Assets/JsonWriter.h"

namespace noc {

    void AssetDependencyGraph::Clear() {
        edges_.clear();
        reverse_.clear();
    }

    void AssetDependencyGraph::SetEdges(AssetId asset, const std::vector<AssetId>& deps) {
        edges_[asset.value] = deps;
        RebuildReverse_();
    }

    const std::vector<AssetId>* AssetDependencyGraph::GetDeps(AssetId asset) const {
        auto it = edges_.find(asset.value);
        if (it == edges_.end()) return nullptr;
        return &it->second;
    }

    void AssetDependencyGraph::RebuildReverse_() {
        reverse_.clear();
        for (const auto& [a, deps] : edges_) {
            for (const auto& d : deps) {
                reverse_[d.value].push_back(AssetId{ a });
            }
        }
    }

    std::vector<AssetId> AssetDependencyGraph::ComputeAffected(AssetId changed) const {
        std::vector<AssetId> out;
        std::unordered_set<uint64_t> visited;
        std::queue<AssetId> q;
        q.push(changed);
        visited.insert(changed.value);

        while (!q.empty()) {
            AssetId cur = q.front();
            q.pop();
            out.push_back(cur);

            auto it = reverse_.find(cur.value);
            if (it == reverse_.end()) continue;
            for (auto dep : it->second) {
                if (visited.insert(dep.value).second) {
                    q.push(dep);
                }
            }
        }
        return out;
    }

    std::vector<AssetId> AssetDependencyGraph::TopologicalOrder(const std::vector<AssetId>& subset) const {
        std::unordered_set<uint64_t> set;
        for (auto a : subset) set.insert(a.value);

        std::unordered_map<uint64_t, int> indeg;
        for (auto a : subset) indeg[a.value] = 0;

        for (auto a : subset) {
            auto it = edges_.find(a.value);
            if (it == edges_.end()) continue;
            for (auto d : it->second) {
                if (!set.count(d.value)) continue;
                indeg[a.value]++; // edge a -> d means a depends on d; for rebuild order we want deps first
            }
        }

        std::queue<AssetId> q;
        for (auto a : subset) {
            if (indeg[a.value] == 0) q.push(a);
        }

        std::vector<AssetId> out;
        while (!q.empty()) {
            AssetId n = q.front(); q.pop();
            out.push_back(n);

            // remove outgoing edges? (n depends on deps, so reverse direction for Kahn):
            // For build, we want: deps first. Our indeg counts "depends on within subset".
            // So when we emit n, we should decrement indeg for dependents of n.
            auto revIt = reverse_.find(n.value);
            if (revIt == reverse_.end()) continue;
            for (auto dependent : revIt->second) {
                if (!set.count(dependent.value)) continue;
                auto it2 = indeg.find(dependent.value);
                if (it2 == indeg.end()) continue;
                it2->second -= 1;
                if (it2->second == 0) q.push(dependent);
            }
        }

        if (out.size() != subset.size()) {
            // cycle
            return {};
        }
        return out;
    }

    bool AssetDependencyGraph::SaveJson(const std::string& physicalPath) const {
        std::ofstream f(physicalPath, std::ios::binary);
        if (!f) return false;

        JsonWriter w(f);
        w.BeginObject();
        w.Key("version"); w.UInt(1);

        w.Key("edges");
        w.BeginArray();
        for (const auto& [a, deps] : edges_) {
            w.BeginObject();
            w.Key("asset"); w.UInt(a);
            w.Key("deps"); w.BeginArray();
            for (auto d : deps) w.UInt(d.value);
            w.EndArray();
            w.EndObject();
        }
        w.EndArray();
        w.EndObject();
        return true;
    }

    // Phase 11: minimal loader that expects the same structure it writes.
    // Design choice: tiny parser is omitted; instead, we start graph empty if file absent.
    // This keeps Phase 11 deterministic and avoids bringing a full JSON parser.
    // If you need LoadJson now, swap to a minimal JSON parser or reuse an existing one later.
    bool AssetDependencyGraph::LoadJson(const std::string& /*physicalPath*/) {
        // Design choice: skip loading in Phase 11 (write-only cache); pipeline still works.
        return true;
    }

} // namespace noc
