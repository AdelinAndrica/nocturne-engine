#pragma once

#include "Runtime/ComponentType.h"

#include <cstdint>

namespace noc
{
    class IAllocator;
    class ReflectionRegistry;

    // Compatibility facade over the engine-wide ReflectionRegistry.
    //
    // Design choice (not directly from the book): Phase 16 keeps this narrow
    // Phase 15 API as a derived read-only view so existing runtime callers do
    // not become a second schema authority. No component registration or
    // canonical-name ownership exists here.
    class ComponentRegistry
    {
    public:
        ComponentRegistry() = default;
        ~ComponentRegistry();

        ComponentRegistry(const ComponentRegistry&) = delete;
        ComponentRegistry& operator=(const ComponentRegistry&) = delete;
        ComponentRegistry(ComponentRegistry&&) = delete;
        ComponentRegistry& operator=(ComponentRegistry&&) = delete;

        [[nodiscard]] bool Init(
            const ReflectionRegistry& reflection,
            IAllocator& allocator);
        void Shutdown();

        [[nodiscard]] const ComponentTypeMetadata* Find(
            ComponentTypeId typeId) const;
        [[nodiscard]] const ComponentTypeMetadata* FindByName(
            const char* canonicalName) const;

        [[nodiscard]] uint32_t Count() const;

        // Deterministic enumeration mirrors reflected component TypeId order.
        [[nodiscard]] const ComponentTypeMetadata* MetadataAt(
            uint32_t index) const;

    private:
        struct Impl;
        Impl* impl_ = nullptr;
    };
}
