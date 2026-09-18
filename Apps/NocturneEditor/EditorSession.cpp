#include "EditorSession.h"

#include "Runtime/Reflection/ReflectionRegistry.h"
#include "Runtime/World.h"

namespace nocturne::editor
{
    EditorSession::~EditorSession()
    {
        Shutdown();
    }

    bool EditorSession::Init(
        noc::World& world,
        const noc::ReflectionRegistry& reflection,
        noc::IAllocator& allocator,
        uint32_t historyMaxCount,
        std::size_t historyMaxBytes)
    {
        if (IsInitialized()
            || !reflection.IsFrozen()
            || historyMaxCount == 0
            || historyMaxBytes == 0)
        {
            return false;
        }

        world_ = &world;
        reflection_ = &reflection;
        allocator_ = &allocator;
        history_.Configure(historyMaxCount, historyMaxBytes);
        activeTransaction_.reset();
        transactionHistoryVersion_ = 0;
        selected_ = noc::EntityHandle::Invalid();
        toolCamera_ = noc::EntityHandle::Invalid();
        activeTool_ = EditorTool::Select;
        orientation_ = TransformOrientation::Local;
        sceneDirty_ = false;
        Touch_();
        return true;
    }

    void EditorSession::Shutdown()
    {
        if (!IsInitialized())
            return;

        // Commands own value snapshots, never component pointers. Pending
        // transactions are deferred and therefore can be cancelled without
        // touching World. Clear them before history and runtime references.
        CancelTransaction();
        history_.Clear();
        selected_ = noc::EntityHandle::Invalid();
        toolCamera_ = noc::EntityHandle::Invalid();
        world_ = nullptr;
        reflection_ = nullptr;
        allocator_ = nullptr;
        sceneDirty_ = false;
        Touch_();
    }

    bool EditorSession::IsInitialized() const noexcept
    {
        return world_ && reflection_ && allocator_;
    }

    bool EditorSession::SetSelection(
        noc::EntityHandle entity) noexcept
    {
        if (!IsInitialized())
            return false;

        if (!entity.IsValid())
        {
            ClearSelection();
            return true;
        }

        if (!world_->IsAlive(entity) || IsToolOwned(entity))
            return false;

        if (selected_ != entity)
        {
            selected_ = entity;
            Touch_();
        }

        return true;
    }

    void EditorSession::ClearSelection() noexcept
    {
        if (selected_.IsValid())
        {
            selected_ = noc::EntityHandle::Invalid();
            Touch_();
        }
    }

    void EditorSession::ValidateSelection() noexcept
    {
        if (selected_.IsValid()
            && (!world_
                || !world_->IsAlive(selected_)
                || IsToolOwned(selected_)))
        {
            selected_ = noc::EntityHandle::Invalid();
            Touch_();
        }
    }

    noc::EntityHandle EditorSession::SelectedEntity() const noexcept
    {
        if (!world_
            || !selected_.IsValid()
            || !world_->IsAlive(selected_))
        {
            return noc::EntityHandle::Invalid();
        }

        return selected_;
    }

    bool EditorSession::SetToolCamera(
        noc::EntityHandle entity) noexcept
    {
        if (!IsInitialized())
            return false;

        if (entity.IsValid() && !world_->IsAlive(entity))
            return false;

        if (toolCamera_ == entity)
            return true;

        toolCamera_ = entity;
        if (selected_ == toolCamera_)
            selected_ = noc::EntityHandle::Invalid();

        Touch_();
        return true;
    }

    noc::EntityHandle EditorSession::ToolCamera() const noexcept
    {
        return toolCamera_;
    }

    bool EditorSession::IsToolOwned(
        noc::EntityHandle entity) const noexcept
    {
        return entity.IsValid()
            && toolCamera_.IsValid()
            && entity == toolCamera_;
    }

    void EditorSession::SetActiveTool(EditorTool tool) noexcept
    {
        if (activeTool_ != tool)
        {
            activeTool_ = tool;
            Touch_();
        }
    }

    EditorTool EditorSession::ActiveTool() const noexcept
    {
        return activeTool_;
    }

    void EditorSession::SetTransformOrientation(
        TransformOrientation value) noexcept
    {
        if (orientation_ != value)
        {
            orientation_ = value;
            Touch_();
        }
    }

    TransformOrientation EditorSession::Orientation() const noexcept
    {
        return orientation_;
    }

    void EditorSession::SetSceneDirty(bool dirty) noexcept
    {
        if (sceneDirty_ != dirty)
        {
            sceneDirty_ = dirty;
            Touch_();
        }
    }

    bool EditorSession::SceneDirty() const noexcept
    {
        return sceneDirty_;
    }

    bool EditorSession::ResetAuthoredScene() noexcept
    {
        if (!IsInitialized())
            return false;

        CancelTransaction();
        selected_ = noc::EntityHandle::Invalid();

        const uint32_t capacity =
            world_->EntityCapacity();

        for (uint32_t index = 0;
             index < capacity;
             ++index)
        {
            const noc::EntityHandle entity =
                world_->EntityAtIndex(index);

            if (!entity.IsValid()
                || IsToolOwned(entity))
            {
                continue;
            }

            if (!world_->DestroyEntity(entity))
                return false;
        }

        history_.Clear();
        sceneDirty_ = false;
        Touch_();
        return true;
    }

    bool EditorSession::BeginTransaction(
        const char* label) noexcept
    {
        if (!IsInitialized()
            || activeTransaction_
            || !label
            || label[0] == '\0')
        {
            return false;
        }

        try
        {
            auto transaction =
                std::make_unique<CompoundEditorCommand>();

            if (!transaction->Init(label))
                return false;

            activeTransaction_ =
                std::move(transaction);
        }
        catch (const std::bad_alloc&)
        {
            return false;
        }

        transactionHistoryVersion_ =
            history_.Version();
        Touch_();
        return true;
    }

    bool EditorSession::AppendTransactionCommand(
        std::unique_ptr<IEditorCommand> command)
    {
        return IsInitialized()
            && activeTransaction_
            && activeTransaction_->Append(
                std::move(command));
    }

    bool EditorSession::CommitTransaction()
    {
        if (!IsInitialized()
            || !activeTransaction_)
        {
            return false;
        }

        if (activeTransaction_->Empty()
            || history_.Version()
                != transactionHistoryVersion_)
        {
            activeTransaction_.reset();
            transactionHistoryVersion_ = 0;
            Touch_();
            return false;
        }

        std::unique_ptr<IEditorCommand> command =
            std::move(activeTransaction_);
        transactionHistoryVersion_ = 0;
        Touch_();

        EditorCommandContext context =
            CommandContext();

        if (!history_.Execute(
                context,
                std::move(command)))
        {
            return false;
        }

        SetSceneDirty();
        return true;
    }

    void EditorSession::CancelTransaction() noexcept
    {
        if (!activeTransaction_)
            return;

        activeTransaction_.reset();
        transactionHistoryVersion_ = 0;
        Touch_();
    }

    bool EditorSession::HasActiveTransaction() const noexcept
    {
        return activeTransaction_ != nullptr;
    }

    uint32_t
    EditorSession::ActiveTransactionCommandCount() const noexcept
    {
        return activeTransaction_
            ? activeTransaction_->CommandCount()
            : 0;
    }

    EditorCommandHistory& EditorSession::History() noexcept
    {
        return history_;
    }

    const EditorCommandHistory& EditorSession::History() const noexcept
    {
        return history_;
    }

    EditorCommandContext EditorSession::CommandContext() noexcept
    {
        return EditorCommandContext{
            *world_,
            *reflection_,
            *allocator_,
            toolCamera_
        };
    }

    uint64_t EditorSession::StateVersion() const noexcept
    {
        return stateVersion_;
    }

    void EditorSession::Touch_() noexcept
    {
        ++stateVersion_;
    }
}
