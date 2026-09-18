#pragma once

#include "Runtime/Reflection/ReflectionMetadata.h"

#include <cstdint>

namespace noc
{
    class IAllocator;

    enum class ReflectionRegistryState : uint8_t
    {
        Uninitialized = 0,
        Building,
        Frozen
    };

    enum class ReflectionRegistryError : uint8_t
    {
        None = 0,
        NotInitialized,
        WrongState,
        InvalidMetadata,
        DuplicateTypeId,
        DuplicateCanonicalName,
        AllocationFailure,
        ValidationFailure
    };

    [[nodiscard]] constexpr const char* ReflectionRegistryErrorName(
        ReflectionRegistryError error) noexcept
    {
        switch (error)
        {
        case ReflectionRegistryError::None:
            return "None";
        case ReflectionRegistryError::NotInitialized:
            return "NotInitialized";
        case ReflectionRegistryError::WrongState:
            return "WrongState";
        case ReflectionRegistryError::InvalidMetadata:
            return "InvalidMetadata";
        case ReflectionRegistryError::DuplicateTypeId:
            return "DuplicateTypeId";
        case ReflectionRegistryError::DuplicateCanonicalName:
            return "DuplicateCanonicalName";
        case ReflectionRegistryError::AllocationFailure:
            return "AllocationFailure";
        case ReflectionRegistryError::ValidationFailure:
            return "ValidationFailure";
        }

        return "Unknown";
    }

    // Engine-wide reflection schema registry.
    //
    // Book grounding:
    // Gregory, Game Engine Architecture 3rd ed., sections 16.2.1.6 and
    // 16.2.2 ground component/property-centric runtime object models.
    //
    // Design choice (not directly from the book):
    // Nocturne owns one explicit allocator-backed registry with a controlled
    // Building -> Freeze -> Frozen lifecycle. Engine startup controls
    // registration order; no translation-unit static initialization is used.
    class ReflectionRegistry
    {
    public:
        ReflectionRegistry() = default;
        ~ReflectionRegistry();

        ReflectionRegistry(const ReflectionRegistry&) = delete;
        ReflectionRegistry& operator=(const ReflectionRegistry&) = delete;
        ReflectionRegistry(ReflectionRegistry&&) = delete;
        ReflectionRegistry& operator=(ReflectionRegistry&&) = delete;

        [[nodiscard]] bool Init(
            IAllocator& allocator,
            uint32_t initialTypeCapacity = 32);
        void Shutdown();

        // Building-only. The registry copies canonical-name storage.
        [[nodiscard]] bool RegisterType(const TypeMetadata& metadata);

        // Re-validates the complete schema and makes metadata immutable.
        // No allocation is performed by Freeze().
        [[nodiscard]] bool Freeze();

        [[nodiscard]] ReflectionRegistryState State() const noexcept;
        [[nodiscard]] ReflectionRegistryError LastError() const noexcept;
        [[nodiscard]] bool IsFrozen() const noexcept;

        [[nodiscard]] uint32_t TypeCount() const noexcept;

        // Deterministic lookup/enumeration. After Freeze these operations do
        // not allocate and returned metadata addresses remain stable until
        // Shutdown().
        [[nodiscard]] const TypeMetadata* FindType(TypeId typeId) const noexcept;
        [[nodiscard]] const TypeMetadata* FindTypeByName(
            const char* canonicalName) const noexcept;
        [[nodiscard]] const TypeMetadata* TypeAt(
            uint32_t index) const noexcept;

    private:
        struct Impl;
        Impl* impl_ = nullptr;
        ReflectionRegistryError lastError_ =
            ReflectionRegistryError::None;
    };
}
