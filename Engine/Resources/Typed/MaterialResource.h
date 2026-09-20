#pragma once
#include "Assets/IntermediateAssets.h"

namespace noc {

    /**
     * @brief CPU-side decoded material resource owned by ResourceManager.
     *
     * MaterialResource currently wraps IntermediateMaterial data from the asset pipeline.
     *
     * @ingroup resources
     */
    class MaterialResource {
    public:
        /** @brief Constructs the resource by taking ownership of decoded material data. */
        explicit MaterialResource(IntermediateMaterial m) : mat_(std::move(m)) {}

        /** @brief Returns underlying CPU material data as a borrowed read-only reference. */
        const IntermediateMaterial& CpuMaterial() const { return mat_; }

    private:
        IntermediateMaterial mat_;
    };

} // namespace noc
