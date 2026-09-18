#pragma once

#include "EditorCommandHistory.h"
#include "EditorReflectionSnapshot.h"
#include "Runtime/Entity.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace nocturne::editor
{
    // Reflection-backed transient subtree snapshot for editor commands.
    //
    // Design choice (not directly from the book): source/current EntityHandles
    // are transient runtime bookkeeping only. Parent relationships inside the
    // snapshot use node indices, so restored handles may differ after Undo/Redo.
    class ReflectedEntitySubtreeSnapshot final
    {
    public:
        ReflectedEntitySubtreeSnapshot() = default;

        ReflectedEntitySubtreeSnapshot(
            const ReflectedEntitySubtreeSnapshot&) = delete;
        ReflectedEntitySubtreeSnapshot& operator=(
            const ReflectedEntitySubtreeSnapshot&) = delete;

        ReflectedEntitySubtreeSnapshot(
            ReflectedEntitySubtreeSnapshot&&) noexcept = default;
        ReflectedEntitySubtreeSnapshot& operator=(
            ReflectedEntitySubtreeSnapshot&&) noexcept = default;

        [[nodiscard]] bool Capture(
            EditorCommandContext& context,
            noc::EntityHandle root);

        [[nodiscard]] bool DestroyCurrent(
            EditorCommandContext& context);

        [[nodiscard]] bool Instantiate(
            EditorCommandContext& context,
            noc::EntityHandle rootParent);

        // Converts a capture into a template for Duplicate: source entities stay
        // alive while the snapshot no longer treats them as current instances.
        void ForgetCurrentInstances() noexcept;

        void Clear() noexcept;

        [[nodiscard]] bool IsValid() const noexcept;
        [[nodiscard]] uint32_t EntityCount() const noexcept;
        [[nodiscard]] noc::EntityHandle OriginalRootParent() const noexcept;
        [[nodiscard]] noc::EntityHandle CurrentRoot() const noexcept;
        [[nodiscard]] std::size_t MemoryCostBytes() const noexcept;

    private:
        struct Node
        {
            noc::EntityHandle sourceEntity{};
            noc::EntityHandle currentEntity{};
            int32_t parentIndex = -1;
            std::vector<ReflectedComponentSnapshot> components;
        };

        void InvalidateCurrent_() noexcept;

        std::vector<Node> nodes_;
        noc::EntityHandle originalRootParent_{};
        bool captured_ = false;
    };
}
