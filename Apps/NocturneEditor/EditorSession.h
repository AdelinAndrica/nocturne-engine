#pragma once

#include "EditorCommandHistory.h"
#include "Runtime/Entity.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace noc
{
    class IAllocator;
    class ReflectionRegistry;
    class World;
}

namespace nocturne::editor
{
    enum class EditorTool : uint8_t
    {
        Select = 0,
        Move,
        Rotate,
        Scale
    };

    enum class TransformOrientation : uint8_t
    {
        Local = 0,
        World
    };

    // Editor-only transient state. World remains the sole authority for scene
    // entities/components; this class stores identity and tooling state only.
    class EditorSession final
    {
    public:
        EditorSession() = default;
        ~EditorSession();

        EditorSession(const EditorSession&) = delete;
        EditorSession& operator=(const EditorSession&) = delete;

        [[nodiscard]] bool Init(
            noc::World& world,
            const noc::ReflectionRegistry& reflection,
            noc::IAllocator& allocator,
            uint32_t historyMaxCount = 512,
            std::size_t historyMaxBytes = 8u * 1024u * 1024u);

        void Shutdown();

        [[nodiscard]] bool IsInitialized() const noexcept;

        [[nodiscard]] bool SetSelection(noc::EntityHandle entity) noexcept;
        void ClearSelection() noexcept;
        void ValidateSelection() noexcept;
        [[nodiscard]] noc::EntityHandle SelectedEntity() const noexcept;

        [[nodiscard]] bool SetToolCamera(noc::EntityHandle entity) noexcept;
        [[nodiscard]] noc::EntityHandle ToolCamera() const noexcept;
        [[nodiscard]] bool IsToolOwned(noc::EntityHandle entity) const noexcept;

        void SetActiveTool(EditorTool tool) noexcept;
        [[nodiscard]] EditorTool ActiveTool() const noexcept;

        void SetTransformOrientation(TransformOrientation value) noexcept;
        [[nodiscard]] TransformOrientation Orientation() const noexcept;

        void SetSceneDirty(bool dirty = true) noexcept;

        // Signals that authoritative authored World state changed even when the
        // dirty flag was already true. Viewport/editor observers use StateVersion
        // as their invalidation boundary, so command execute/undo/redo paths must
        // call this after a successful authored mutation.
        void NotifyAuthoredMutation() noexcept;

        [[nodiscard]] bool SceneDirty() const noexcept;

        // In-memory editor scene reset. Destroys authored entities only; the
        // tool camera remains owned by the same World. This establishes a new
        // transient baseline, so selection/history/dirty state are cleared.
        [[nodiscard]] bool ResetAuthoredScene() noexcept;

        // Design choice (not directly from the book): transactions are deferred
        // compound-command builders. Append never mutates World. Commit executes
        // the compound atomically through the same history; cancel simply drops
        // the pending commands. Nested transactions are rejected.
        [[nodiscard]] bool BeginTransaction(
            const char* label) noexcept;
        [[nodiscard]] bool AppendTransactionCommand(
            std::unique_ptr<IEditorCommand> command);
        [[nodiscard]] bool CommitTransaction();
        void CancelTransaction() noexcept;
        [[nodiscard]] bool HasActiveTransaction() const noexcept;
        [[nodiscard]] uint32_t ActiveTransactionCommandCount() const noexcept;

        [[nodiscard]] EditorCommandHistory& History() noexcept;
        [[nodiscard]] const EditorCommandHistory& History() const noexcept;

        [[nodiscard]] EditorCommandContext CommandContext() noexcept;

        [[nodiscard]] uint64_t StateVersion() const noexcept;

    private:
        void Touch_() noexcept;

        noc::World* world_ = nullptr;
        const noc::ReflectionRegistry* reflection_ = nullptr;
        noc::IAllocator* allocator_ = nullptr;

        noc::EntityHandle selected_{};
        noc::EntityHandle toolCamera_{};

        EditorTool activeTool_ = EditorTool::Select;
        TransformOrientation orientation_ = TransformOrientation::Local;

        EditorCommandHistory history_;
        std::unique_ptr<CompoundEditorCommand> activeTransaction_;
        uint64_t transactionHistoryVersion_ = 0;
        bool sceneDirty_ = false;
        uint64_t stateVersion_ = 0;
    };
}
