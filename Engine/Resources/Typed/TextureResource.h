#pragma once
#include "Assets/IntermediateAssets.h"

namespace noc {

    /**
     * @brief CPU-side decoded texture resource owned by ResourceManager.
     *
     * The resource stores IntermediateTexture/mip data. Width()/Height() report the
     * first mip when present.
     *
     * @ingroup resources
     */
    class TextureResource {
    public:
        /** @brief Constructs the resource by taking ownership of decoded texture data. */
        explicit TextureResource(IntermediateTexture t) : tex_(std::move(t)) {}

        /** @brief Returns the underlying CPU texture as a borrowed read-only reference. */
        const IntermediateTexture& CpuTexture() const { return tex_; }

        /** @brief Returns first-mip width, or 0 when no mip data exists. */
        uint32_t Width() const { return tex_.mips.empty() ? 0u : tex_.mips[0].width; }

        /** @brief Returns first-mip height, or 0 when no mip data exists. */
        uint32_t Height() const { return tex_.mips.empty() ? 0u : tex_.mips[0].height; }

    private:
        IntermediateTexture tex_;
    };

} // namespace noc
