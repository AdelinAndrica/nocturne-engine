#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
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
    };

    // Design choice (not directly from the book): editor history owns commands
    // with a count + approximate-byte budget. New successful commands after an
    // undo discard the redo tail; failed operations never move the cursor.
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

        [[nodiscard]] const char* UndoLabel() const noexcept;
        [[nodiscard]] const char* RedoLabel() const noexcept;

    private:
        void DiscardRedoTail_() noexcept;
        void EnforceBudget_() noexcept;

        std::vector<std::unique_ptr<IEditorCommand>> commands_;
        uint32_t cursor_ = 0;
        uint32_t maxCommandCount_ = 512;
        std::size_t maxBytes_ = 8u * 1024u * 1024u;
        std::size_t usedBytes_ = 0;
        uint64_t version_ = 0;
    };
}
