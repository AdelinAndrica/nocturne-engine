#pragma once
#include "Assets/RuntimeFormats/MeshBlob.h"
#include "Resources/Typed/IResourceLoader.h"
#include "Resources/Typed/MeshResource.h"

namespace noc {

    class MeshResourceLoader final : public IResourceLoader {
    public:
        ResourceType Type() const override { return ResourceType::Mesh; }

        ResourceLoadResult Decode(const uint8_t* bytes, size_t size) override {
            ResourceLoadResult rr{};
            IntermediateMesh m{};
            std::string err;
            if (!ReadMeshBlob(bytes, size, &m, &err)) {
                rr.ok = false;
                rr.error = err.empty() ? "Mesh decode failed" : err;
                return rr;
            }
            rr.object = new MeshResource(std::move(m));
            rr.ok = true;
            return rr;
        }
    };

} // namespace noc
