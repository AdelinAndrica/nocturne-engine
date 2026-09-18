#include "EditorCommandHistory.h"

#include "Core/Log.h"

#include <algorithm>

#include <limits>
#include <new>

namespace nocturne::editor
{
    namespace
    {
        [[nodiscard]] std::size_t SaturatingAdd(
            std::size_t lhs,
            std::size_t rhs) noexcept
        {
            const std::size_t maxValue =
                (std::numeric_limits<std::size_t>::max)();

            return rhs > maxValue - lhs
                ? maxValue
                : lhs + rhs;
        }
    }

    bool CompoundEditorCommand::Init(
        const char* label) noexcept
    {
        if (!label
            || label[0] == '\0'
            || !label_.empty()
            || !commands_.empty()
            || applied_
            || rollbackFailure_)
        {
            return false;
        }

        try
        {
            label_ = label;
        }
        catch (const std::bad_alloc&)
        {
            label_.clear();
            return false;
        }

        return true;
    }

    bool CompoundEditorCommand::Append(
        std::unique_ptr<IEditorCommand> command)
    {
        if (!command
            || label_.empty()
            || applied_
            || rollbackFailure_
            || command->MemoryCostBytes() == 0)
        {
            return false;
        }

        try
        {
            commands_.push_back(
                std::move(command));
        }
        catch (const std::bad_alloc&)
        {
            return false;
        }

        return true;
    }

    bool CompoundEditorCommand::Empty() const noexcept
    {
        return commands_.empty();
    }

    uint32_t CompoundEditorCommand::CommandCount() const noexcept
    {
        return static_cast<uint32_t>(
            commands_.size());
    }

    bool CompoundEditorCommand::HasRollbackFailure() const noexcept
    {
        return rollbackFailure_;
    }

    const char* CompoundEditorCommand::Label() const noexcept
    {
        return label_.c_str();
    }

    std::size_t
    CompoundEditorCommand::MemoryCostBytes() const noexcept
    {
        std::size_t cost =
            sizeof(CompoundEditorCommand);

        cost = SaturatingAdd(
            cost,
            label_.capacity() + 1u);
        cost = SaturatingAdd(
            cost,
            commands_.capacity()
                * sizeof(std::unique_ptr<IEditorCommand>));

        for (const auto& command : commands_)
        {
            if (!command)
                return 0;

            cost = SaturatingAdd(
                cost,
                command->MemoryCostBytes());
        }

        return cost;
    }

    bool CompoundEditorCommand::Execute(
        EditorCommandContext& context)
    {
        if (label_.empty()
            || commands_.empty()
            || applied_
            || rollbackFailure_)
        {
            return false;
        }

        std::size_t executedCount = 0;

        for (;
             executedCount < commands_.size();
             ++executedCount)
        {
            if (commands_[executedCount]->Execute(
                    context))
            {
                continue;
            }

            bool rollbackOk = true;

            while (executedCount > 0)
            {
                --executedCount;

                if (!commands_[executedCount]->Undo(
                        context))
                {
                    rollbackOk = false;
                }
            }

            if (!rollbackOk)
                ReportRollbackFailure_("execute");

            return false;
        }

        applied_ = true;
        return true;
    }

    bool CompoundEditorCommand::Undo(
        EditorCommandContext& context)
    {
        if (!applied_
            || rollbackFailure_)
        {
            return false;
        }

        for (std::size_t i = commands_.size();
             i > 0;
             --i)
        {
            const std::size_t index = i - 1u;

            if (commands_[index]->Undo(context))
                continue;

            // Restore commands that were already undone during this attempt so
            // a failed Undo leaves the compound in its pre-call applied state.
            bool restoreOk = true;

            for (std::size_t restore = index + 1u;
                 restore < commands_.size();
                 ++restore)
            {
                if (!commands_[restore]->Redo(
                        context))
                {
                    restoreOk = false;
                }
            }

            if (!restoreOk)
                ReportRollbackFailure_("undo compensation");

            return false;
        }

        applied_ = false;
        return true;
    }

    bool CompoundEditorCommand::Redo(
        EditorCommandContext& context)
    {
        if (applied_
            || rollbackFailure_
            || commands_.empty())
        {
            return false;
        }

        std::size_t redoneCount = 0;

        for (;
             redoneCount < commands_.size();
             ++redoneCount)
        {
            if (commands_[redoneCount]->Redo(
                    context))
            {
                continue;
            }

            bool rollbackOk = true;

            while (redoneCount > 0)
            {
                --redoneCount;

                if (!commands_[redoneCount]->Undo(
                        context))
                {
                    rollbackOk = false;
                }
            }

            if (!rollbackOk)
                ReportRollbackFailure_("redo");

            return false;
        }

        applied_ = true;
        return true;
    }

    void CompoundEditorCommand::ReportRollbackFailure_(
        const char* phase) noexcept
    {
        rollbackFailure_ = true;

        NOC_LOG_ERROR(
            "EditorHistory",
            "Compound command '%s' suffered rollback failure during %s; runtime state may require recovery.",
            label_.empty() ? "<unnamed>" : label_.c_str(),
            phase ? phase : "unknown phase");
    }

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

        // Capacity growth happens before runtime mutation. Geometric growth
        // avoids the previous reserve(size + 1) O(N^2)-style reallocation
        // pattern while preserving allocation-failure atomicity.
        if (!EnsureAppendCapacity_())
            return false;

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

    bool EditorCommandHistory::RecordExecuted(
        EditorCommandContext& context,
        std::unique_ptr<IEditorCommand> command)
    {
        if (!command || maxCommandCount_ == 0 || maxBytes_ == 0)
        {
            if (command)
                (void)command->Undo(context);
            return false;
        }

        const std::size_t cost = command->MemoryCostBytes();
        if (cost == 0 || cost > maxBytes_)
        {
            (void)command->Undo(context);
            return false;
        }

        if (!EnsureAppendCapacity_())
        {
            (void)command->Undo(context);
            return false;
        }

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

    bool EditorCommandHistory::EnsureAppendCapacity_() noexcept
    {
        const std::size_t required =
            commands_.size() + 1u;

        if (commands_.capacity() >= required)
            return true;

        std::size_t target =
            commands_.capacity() == 0
                ? 16u
                : commands_.capacity()
                    + (std::max)(
                        commands_.capacity() / 2u,
                        std::size_t{ 8u });

        if (target < required)
            target = required;

        try
        {
            commands_.reserve(target);
        }
        catch (const std::bad_alloc&)
        {
            return false;
        }

        return true;
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
