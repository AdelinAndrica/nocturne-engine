#pragma once
#include "Assets/RuntimeFormats/TextureBlob.h"
#include "Resources/Typed/IResourceLoader.h"
#include "Resources/Typed/TextureResource.h"

namespace noc {

    class TextureResourceLoader final : public IResourceLoader {
    public:
        ResourceType Type() const override { return ResourceType::Texture; }

        ResourceLoadResult Decode(const uint8_t* bytes, size_t size) override {
            ResourceLoadResult rr{};
            IntermediateTexture t{};
            std::string err;
            if (!ReadTextureBlob(bytes, size, &t, &err)) {
                rr.ok = false;
                rr.error = err.empty() ? "Texture decode failed" : err;
                return rr;
            }
            rr.object = new TextureResource(std::move(t));
            rr.ok = true;
            return rr;
        }
    };

} // namespace noc
