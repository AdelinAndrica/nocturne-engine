#pragma once
#include <array>
#include <cstddef>

#include "Resources/Typed/IResourceLoader.h"
#include "Resources/Typed/ResourceType.h"

namespace noc {

    class ResourceLoaderRegistry {
    public:
        ResourceLoaderRegistry() { loaders_.fill(nullptr); }

        bool RegisterLoader(IResourceLoader* loader) {
            if (!loader) return false;
            const auto t = loader->Type();
            const auto idx = static_cast<size_t>(t);
            if (idx >= loaders_.size()) return false;
            loaders_[idx] = loader;
            return true;
        }

        IResourceLoader* FindLoader(ResourceType t) const {
            const auto idx = static_cast<size_t>(t);
            if (idx >= loaders_.size()) return nullptr;
            return loaders_[idx];
        }

    private:
        // Small fixed table; expand as ResourceType grows.
        std::array<IResourceLoader*, 16> loaders_{};
    };

} // namespace noc