#pragma once

#include "Runtime/ComponentType.h"

#include <cstdint>

namespace noc
{
    class IAllocator;

    // Registry for component schema metadata.
    //
    // Book grounding:
    // - Gregory, Game Engine Architecture 3rd ed., section 16.2 discusses
    //   component/property-centric object models and data-driven composition.
    // - Section 16.5 motivates stable object identity and lookup facilities.
    //
    // Design choice (not directly from the book): Phase 15 keeps metadata in a
    // small allocator-owned registry sorted by explicit ComponentTypeId.
    // Registration is startup/main-thread structural work, not a frame-hot path.
    class ComponentRegistry
    {
    public:
        ComponentRegistry() = default;
        ~ComponentRegistry();

        ComponentRegistry(const ComponentRegistry&) = delete;
        ComponentRegistry& operator=(const ComponentRegistry&) = delete;
        ComponentRegistry(ComponentRegistry&&) = delete;
        ComponentRegistry& operator=(ComponentRegistry&&) = delete;

        bool Init(IAllocator& allocator, uint32_t initialCapacity = 16);
        void Shutdown();

        // Returns false for malformed metadata, duplicate type IDs, duplicate
        // canonical names, allocation failure, or use before Init().
        [[nodiscard]] bool Register(const ComponentTypeMetadata& metadata);

        [[nodiscard]] const ComponentTypeMetadata* Find(ComponentTypeId typeId) const;
        [[nodiscard]] const ComponentTypeMetadata* FindByName(const char* canonicalName) const;

        [[nodiscard]] uint32_t Count() const;

        // Deterministic enumeration: entries are always sorted by typeId,
        // independent of registration order.
        [[nodiscard]] const ComponentTypeMetadata* MetadataAt(uint32_t index) const;

    private:
        struct Impl;
        Impl* impl_ = nullptr;
    };
}
