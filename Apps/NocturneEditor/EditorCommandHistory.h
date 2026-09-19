#pragma once

#include "Runtime/Entity.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace noc
{
    class IAllocator;
    class ReflectionRegistry;
    class World;
}

namespace nocturne::editor
{
    struct EditorCommandContext
    {
        noc::World& world;
        const noc::ReflectionRegistry& reflection;
        noc::IAllocator& allocator;
        noc::EntityHandle toolOwnedEntity{};

        [[nodiscard]] bool IsToolOwned(
            noc::EntityHandle entity) const noexcept
        {
            return entity.IsValid()
                && toolOwnedEntity.IsValid()
                && entity == toolOwnedEntity;
        }
    };

    class IEditorCommand
    {
    public:
        virtual ~IEditorCommand() = default;

        [[nodiscard]] virtual const char* Label() const noexcept = 0;
        [[nodiscard]] virtual std::size_t MemoryCostBytes() const noexcept = 0;

        [[nodiscard]] virtual bool Execute(
            EditorCommandContext& context) = 0;
        [[nodiscard]] virtual bool Undo(
            EditorCommandContext& context) = 0;
        [[nodiscard]] virtual bool Redo(
            EditorCommandContext& context) = 0;

        // Design choice (not directly from the book): commands that create or
        // recreate runtime entities can expose the new runtime identity without
        // coupling history/UI to concrete command types. Invalid means "keep
        // current selection if still alive"; callers still validate staleness.
        [[nodiscard]] virtual noc::EntityHandle
        SelectionHintAfterExecute() const noexcept
        {
            return noc::EntityHandle::Invalid();
        }

        [[nodiscard]] virtual noc::EntityHandle
        SelectionHintAfterUndo() const noexcept
        {
            return noc::EntityHandle::Invalid();
        }

        [[nodiscard]] virtual noc::EntityHandle
        SelectionHintAfterRedo() const noexcept
        {
            return noc::EntityHandle::Invalid();
        }
    };

    // Design choice (not directly from the book): compound commands are the
    // transaction commit unit. Children execute in append order and undo in
    // reverse order. Failed execute/redo attempts compensate already-applied
    // children before returning failure so the history cursor can remain
    // unchanged.
    class CompoundEditorCommand final
        : public IEditorCommand
    {
    public:
        CompoundEditorCommand() = default;

        CompoundEditorCommand(const CompoundEditorCommand&) = delete;
        CompoundEditorCommand& operator=(const CompoundEditorCommand&) = delete;

        [[nodiscard]] bool Init(const char* label) noexcept;
        [[nodiscard]] bool Append(
            std::unique_ptr<IEditorCommand> command);

        [[nodiscard]] bool Empty() const noexcept;
        [[nodiscard]] uint32_t CommandCount() const noexcept;
        [[nodiscard]] bool HasRollbackFailure() const noexcept;

        [[nodiscard]] const char* Label() const noexcept override;
        [[nodiscard]] std::size_t MemoryCostBytes() const noexcept override;

        [[nodiscard]] bool Execute(
            EditorCommandContext& context) override;
        [[nodiscard]] bool Undo(
            EditorCommandContext& context) override;
        [[nodiscard]] bool Redo(
            EditorCommandContext& context) override;

    private:
        void ReportRollbackFailure_(
            const char* phase) noexcept;

        std::string label_;
        std::vector<std::unique_ptr<IEditorCommand>> commands_;
        bool applied_ = false;
        bool rollbackFailure_ = false;
    };

    // Design choice (not directly from the book): editor history owns commands
    // exclusively after successful adoption through Execute/RecordExecuted.
    // Callers transfer std::unique_ptr ownership; rejected/failed commands are
    // destroyed without entering history. History uses a count + approximate
    // command-byte budget. New successful commands after an undo discard the
    // redo tail; failed operations never move the cursor.
    class EditorCommandHistory final
    {
    public:
        EditorCommandHistory() = default;

        EditorCommandHistory(const EditorCommandHistory&) = delete;
        EditorCommandHistory& operator=(const EditorCommandHistory&) = delete;

        void Configure(
            uint32_t maxCommandCount,
            std::size_t maxBytes) noexcept;

        [[nodiscard]] bool Execute(
            EditorCommandContext& context,
            std::unique_ptr<IEditorCommand> command);

        // Adopts a command whose final state is already live (for example a
        // gizmo drag). Failure rolls the command back through Undo so runtime
        // state cannot diverge from history ownership.
        [[nodiscard]] bool RecordExecuted(
            EditorCommandContext& context,
            std::unique_ptr<IEditorCommand> command);

        [[nodiscard]] bool Undo(EditorCommandContext& context);
        [[nodiscard]] bool Redo(EditorCommandContext& context);

        void Clear() noexcept;

        [[nodiscard]] bool CanUndo() const noexcept;
        [[nodiscard]] bool CanRedo() const noexcept;
        [[nodiscard]] uint32_t CommandCount() const noexcept;
        [[nodiscard]] uint32_t Cursor() const noexcept;
        [[nodiscard]] std::size_t UsedBytes() const noexcept;
        [[nodiscard]] uint64_t Version() const noexcept;
        [[nodiscard]] noc::EntityHandle
        LastSelectionHint() const noexcept;

        [[nodiscard]] const char* UndoLabel() const noexcept;
        [[nodiscard]] const char* RedoLabel() const noexcept;

    private:
        [[nodiscard]] bool EnsureAppendCapacity_() noexcept;
        void DiscardRedoTail_() noexcept;
        void EnforceBudget_() noexcept;

        std::vector<std::unique_ptr<IEditorCommand>> commands_;
        uint32_t cursor_ = 0;
        uint32_t maxCommandCount_ = 512;
        std::size_t maxBytes_ = 8u * 1024u * 1024u;
        std::size_t usedBytes_ = 0;
        uint64_t version_ = 0;
        noc::EntityHandle lastSelectionHint_{};
    };
}
