#include "EditorCommandHistory.h"

#include <new>

namespace nocturne::editor
{
    void EditorCommandHistory::Configure(
        uint32_t maxCommandCount,
        std::size_t maxBytes) noexcept
    {
        maxCommandCount_ = maxCommandCount;
        maxBytes_ = maxBytes;
        EnforceBudget_();
    }

    bool EditorCommandHistory::Execute(
        EditorCommandContext& context,
        std::unique_ptr<IEditorCommand> command)
    {
        if (!command || maxCommandCount_ == 0 || maxBytes_ == 0)
            return false;

        const std::size_t cost = command->MemoryCostBytes();
        if (cost == 0 || cost > maxBytes_)
            return false;

        // Reserve before mutating runtime state so allocation failure leaves
        // both history and World untouched.
        try
        {
            if (commands_.capacity() < commands_.size() + 1u)
                commands_.reserve(commands_.size() + 1u);
        }
        catch (const std::bad_alloc&)
        {
            return false;
        }

        if (!command->Execute(context))
            return false;

        DiscardRedoTail_();

        usedBytes_ += cost;
        commands_.push_back(std::move(command));
        cursor_ = static_cast<uint32_t>(commands_.size());

        EnforceBudget_();
        ++version_;
        return true;
    }

    bool EditorCommandHistory::Undo(EditorCommandContext& context)
    {
        if (!CanUndo())
            return false;

        IEditorCommand& command = *commands_[cursor_ - 1u];
        if (!command.Undo(context))
            return false;

        --cursor_;
        ++version_;
        return true;
    }

    bool EditorCommandHistory::Redo(EditorCommandContext& context)
    {
        if (!CanRedo())
            return false;

        IEditorCommand& command = *commands_[cursor_];
        if (!command.Redo(context))
            return false;

        ++cursor_;
        ++version_;
        return true;
    }

    void EditorCommandHistory::Clear() noexcept
    {
        commands_.clear();
        cursor_ = 0;
        usedBytes_ = 0;
        ++version_;
    }

    bool EditorCommandHistory::CanUndo() const noexcept
    {
        return cursor_ > 0;
    }

    bool EditorCommandHistory::CanRedo() const noexcept
    {
        return cursor_ < commands_.size();
    }

    uint32_t EditorCommandHistory::CommandCount() const noexcept
    {
        return static_cast<uint32_t>(commands_.size());
    }

    uint32_t EditorCommandHistory::Cursor() const noexcept
    {
        return cursor_;
    }

    std::size_t EditorCommandHistory::UsedBytes() const noexcept
    {
        return usedBytes_;
    }

    uint64_t EditorCommandHistory::Version() const noexcept
    {
        return version_;
    }

    const char* EditorCommandHistory::UndoLabel() const noexcept
    {
        return CanUndo()
            ? commands_[cursor_ - 1u]->Label()
            : nullptr;
    }

    const char* EditorCommandHistory::RedoLabel() const noexcept
    {
        return CanRedo()
            ? commands_[cursor_]->Label()
            : nullptr;
    }

    void EditorCommandHistory::DiscardRedoTail_() noexcept
    {
        while (commands_.size() > cursor_)
        {
            usedBytes_ -= commands_.back()->MemoryCostBytes();
            commands_.pop_back();
        }
    }

    void EditorCommandHistory::EnforceBudget_() noexcept
    {
        while (!commands_.empty()
            && (commands_.size() > maxCommandCount_
                || usedBytes_ > maxBytes_))
        {
            const std::size_t cost =
                commands_.front()->MemoryCostBytes();

            commands_.erase(commands_.begin());
            usedBytes_ -= cost;

            if (cursor_ > 0)
                --cursor_;
        }
    }
}
