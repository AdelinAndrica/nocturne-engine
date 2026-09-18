#pragma once

#include "EditorCommandHistory.h"
#include "Runtime/Entity.h"
#include "Runtime/Reflection/ReflectedValue.h"
#include "Runtime/Reflection/ReflectionIds.h"

#include <cstddef>
#include <vector>

namespace nocturne::editor
{
    // Transient editor snapshot of one reflected component instance.
    //
    // Design choice (not directly from the book): this is command/undo state,
    // never a persistent scene serialization format or entity identity.
    //
    // Ownership: the snapshot owns its PropertyEntry vector by value. Each
    // OwnedReflectedValue exclusively owns its reflected payload through the
    // EditorCommandContext allocator captured during Capture(). Clear/destruction
    // releases every payload; no reflected component pointer is retained.
    class ReflectedComponentSnapshot final
    {
    public:
        ReflectedComponentSnapshot() = default;

        ReflectedComponentSnapshot(
            const ReflectedComponentSnapshot&) = delete;
        ReflectedComponentSnapshot& operator=(
            const ReflectedComponentSnapshot&) = delete;

        ReflectedComponentSnapshot(
            ReflectedComponentSnapshot&&) noexcept = default;
        ReflectedComponentSnapshot& operator=(
            ReflectedComponentSnapshot&&) noexcept = default;

        [[nodiscard]] bool Capture(
            EditorCommandContext& context,
            noc::EntityHandle entity,
            noc::TypeId componentTypeId);

        // Component must already exist. Restore uses semantic reflected setters
        // and retries unresolved cross-property validation dependencies.
        [[nodiscard]] bool Restore(
            EditorCommandContext& context,
            noc::EntityHandle entity);

        void Clear() noexcept;

        [[nodiscard]] bool IsValid() const noexcept;
        [[nodiscard]] noc::TypeId ComponentType() const noexcept;
        [[nodiscard]] std::size_t MemoryCostBytes() const noexcept;

    private:
        struct PropertyEntry
        {
            noc::PropertyId propertyId{};
            noc::OwnedReflectedValue value;
            bool restored = false;
        };

        noc::TypeId componentTypeId_{};
        std::vector<PropertyEntry> properties_;
        bool captured_ = false;
    };
}
