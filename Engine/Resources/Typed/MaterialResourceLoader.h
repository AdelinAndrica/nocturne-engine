#pragma once
#include "Assets/RuntimeFormats/MaterialBlob.h"
#include "Resources/Typed/IResourceLoader.h"
#include "Resources/Typed/MaterialResource.h"

namespace noc {

    class MaterialResourceLoader final : public IResourceLoader {
    public:
        ResourceType Type() const override { return ResourceType::Material; }

        ResourceLoadResult Decode(const uint8_t* bytes, size_t size) override {
            ResourceLoadResult rr{};
            IntermediateMaterial m{};
            std::string err;
            if (!ReadMaterialBlob(bytes, size, &m, &err)) {
                rr.ok = false;
                rr.error = err.empty() ? "Material decode failed" : err;
                return rr;
            }
            rr.object = new MaterialResource(std::move(m));
            rr.ok = true;
            return rr;
        }
    };

} // namespace noc
