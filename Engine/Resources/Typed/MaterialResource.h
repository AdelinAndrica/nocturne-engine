#pragma once
#include "Assets/IntermediateAssets.h"

namespace noc {

    class MaterialResource {
    public:
        explicit MaterialResource(IntermediateMaterial m) : mat_(std::move(m)) {}
        const IntermediateMaterial& CpuMaterial() const { return mat_; }

    private:
        IntermediateMaterial mat_;
    };

} // namespace noc
