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

        // Commands own value snapshots, never component pointers. Clearing
        // history before releasing World/Reflection references makes shutdown
        // order explicit and deterministic.
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
