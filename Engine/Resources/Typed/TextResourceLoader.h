#pragma once
#include "Resources/Typed/IResourceLoader.h"
#include "Resources/Typed/TextResource.h"

namespace noc {

    class TextResourceLoader final : public IResourceLoader
    {
    public:
        ResourceType Type() const override { return ResourceType::Text; }

        ResourceLoadResult Decode(const uint8_t* bytes, size_t size) override
        {
            ResourceLoadResult r{};
            if (!bytes && size != 0)
            {
                r.ok = false;
                r.error = "Text decode: null bytes";
                return r;
            }

            std::string s(reinterpret_cast<const char*>(bytes),
                reinterpret_cast<const char*>(bytes) + size);

            r.object = new TextResource(std::move(s));
            r.ok = true;
            return r;
        }
    };

} // namespace noc