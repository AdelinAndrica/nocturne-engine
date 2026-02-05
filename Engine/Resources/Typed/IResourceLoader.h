#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

#include "Resources/Typed/ResourceType.h"

namespace noc {

    struct ResourceLoadResult {
        bool ok = false;
        std::string error;

        // Type-erased pointer to decoded runtime object.
        // Owned by ResourceManager after completion; must be heap allocated.
        void* object = nullptr;
    };

    class IResourceLoader {
    public:
        virtual ~IResourceLoader() = default;

        virtual ResourceType Type() const = 0;

        // Optional: basic sniffing based on vpath extension, etc.
        // In Phase 5 we route primarily by ResourceType at request time.
        virtual bool CanLoad(const char* /*normalizedVPath*/) const { return true; }

        // Runs on the loader thread in Phase 5.
        // Must be CPU-only and thread-safe.
        virtual ResourceLoadResult Decode(const uint8_t* bytes, size_t size) = 0;
    };

} // namespace noc