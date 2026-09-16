#pragma once
#include "Assets/IntermediateAssets.h"

namespace noc {

    class TextureResource {
    public:
        explicit TextureResource(IntermediateTexture t) : tex_(std::move(t)) {}
        const IntermediateTexture& CpuTexture() const { return tex_; }

        uint32_t Width() const { return tex_.mips.empty() ? 0u : tex_.mips[0].width; }
        uint32_t Height() const { return tex_.mips.empty() ? 0u : tex_.mips[0].height; }

    private:
        IntermediateTexture tex_;
    };

} // namespace noc
