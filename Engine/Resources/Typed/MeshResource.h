#pragma once
#include <cstdint>
#include <vector>

#include "Assets/IntermediateAssets.h"

namespace noc {

    class MeshResource {
    public:
        explicit MeshResource(IntermediateMesh mesh) : mesh_(std::move(mesh)) {}
        const IntermediateMesh& CpuMesh() const { return mesh_; }

        uint32_t VertexCount() const { return (uint32_t)(mesh_.positions.size() / 3); }
        uint32_t IndexCount() const { return (uint32_t)mesh_.indices.size(); }

    private:
        IntermediateMesh mesh_;
    };

} // namespace noc
