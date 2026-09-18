#pragma once

#include "EditorEntitySnapshot.h"

#include <cstddef>
#include <cstdint>

namespace nocturne::editor
{
    // Editor-only, in-memory reusable entity prototype.
    //
    // Design choice (not directly from the book): this seam intentionally
    // reuses ReflectedEntitySubtreeSnapshot instead of creating a prefab-owned
    // schema. It has no file format, asset ID, persistent entity identity,
    // serialized references, migration policy, or compatibility guarantee.
    //
    // The prototype owns only the transient reflected template. Instantiated
    // World entities are owned by World and receive fresh runtime EntityHandles.
    // After instantiation the prototype forgets those runtime handles so the
    // same captured template can instantiate again.
    class TransientEntityPrototype final
    {
    public:
        TransientEntityPrototype() = default;

        TransientEntityPrototype(
            const TransientEntityPrototype&) = delete;
        TransientEntityPrototype& operator=(
            const TransientEntityPrototype&) = delete;

        TransientEntityPrototype(
            TransientEntityPrototype&&) noexcept = default;
        TransientEntityPrototype& operator=(
            TransientEntityPrototype&&) noexcept = default;

        [[nodiscard]] bool Capture(
            EditorCommandContext& context,
            noc::EntityHandle sourceRoot)
        {
            if (snapshot_.IsValid())
                return false;

            if (!snapshot_.Capture(
                    context,
                    sourceRoot))
            {
                return false;
            }

            // Source entities remain alive. The captured subtree becomes an
            // editor-only reusable template instead of "current instances".
            snapshot_.ForgetCurrentInstances();
            return true;
        }

        [[nodiscard]] bool Instantiate(
            EditorCommandContext& context,
            noc::EntityHandle parent,
            noc::EntityHandle& outRoot)
        {
            outRoot =
                noc::EntityHandle::Invalid();

            if (!snapshot_.IsValid())
                return false;

            if (!snapshot_.Instantiate(
                    context,
                    parent))
            {
                return false;
            }

            outRoot =
                snapshot_.CurrentRoot();

            if (!outRoot.IsValid()
                || !context.world.IsAlive(outRoot))
            {
                snapshot_.ForgetCurrentInstances();
                outRoot =
                    noc::EntityHandle::Invalid();
                return false;
            }

            // Prototype instances are World-owned. Do not retain their runtime
            // handles as if they were persistent prefab identities.
            snapshot_.ForgetCurrentInstances();
            return true;
        }

        void Clear() noexcept
        {
            snapshot_.Clear();
        }

        [[nodiscard]] bool IsValid() const noexcept
        {
            return snapshot_.IsValid();
        }

        [[nodiscard]] uint32_t EntityCount() const noexcept
        {
            return snapshot_.EntityCount();
        }

        [[nodiscard]] std::size_t
        MemoryCostBytes() const noexcept
        {
            return sizeof(*this)
                + snapshot_.MemoryCostBytes();
        }

    private:
        ReflectedEntitySubtreeSnapshot snapshot_;
    };
}
