#pragma once
#include <cstdint>
#include <vector>

#include "Assets/IntermediateAssets.h"

namespace noc {

    /**
     * @brief CPU-side decoded mesh resource owned by ResourceManager.
     *
     * MeshResource currently wraps IntermediateMesh data produced by the asset/resource
     * pipeline. Rendering may use it to create/use GPU-facing mesh resources, but the
     * object itself is CPU-side resource data.
     *
     * @ingroup resources
     */
    class MeshResource {
    public:
        /** @brief Constructs the resource by taking ownership of decoded mesh data. */
        explicit MeshResource(IntermediateMesh mesh) : mesh_(std::move(mesh)) {}

        /** @brief Returns the underlying CPU mesh as a borrowed read-only reference. */
        const IntermediateMesh& CpuMesh() const { return mesh_; }

        /**
         * @brief Returns vertex count derived from packed xyz position scalars.
         * @return positions.size() / 3.
         */
        uint32_t VertexCount() const { return (uint32_t)(mesh_.positions.size() / 3); }

        /** @brief Returns the number of stored mesh indices. */
        uint32_t IndexCount() const { return (uint32_t)mesh_.indices.size(); }

    private:
        IntermediateMesh mesh_;
    };

} // namespace noc
