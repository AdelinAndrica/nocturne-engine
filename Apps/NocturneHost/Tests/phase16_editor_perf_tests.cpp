#include "../../NocturneEditor/EditorCommandHistory.h"
#include "../../NocturneEditor/EditorGizmoTransaction.h"
#include "../../NocturneEditor/EditorCommands.h"
#include "../../NocturneEditor/EditorHierarchyModel.h"
#include "../../NocturneEditor/EditorInspectorModel.h"
#include "../../NocturneEditor/EditorSession.h"

#include "Core/Log.h"
#include "Core/Memory/Allocator.h"
#include "Core/Memory/DebugAlloc.h"
#include "Runtime/Reflection/BuiltinTypes.h"
#include "Runtime/Reflection/FoundationComponents.h"
#include "Runtime/Reflection/ReflectionRegistry.h"
#include "Runtime/World.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace
{
    using Clock = std::chrono::steady_clock;

    bool CheckEditorPerf(
        bool condition,
        const char* message)
    {
        if (!condition)
        {
            NOC_LOG_ERROR(
                "Phase16EditorPerf",
                "%s",
                message);
            return false;
        }

        return true;
    }

    long long Micros(
        Clock::time_point begin,
        Clock::time_point end)
    {
        return std::chrono::duration_cast<
            std::chrono::microseconds>(
                end - begin).count();
    }

    class SmallHistoryCommandSafe final
        : public nocturne::editor::IEditorCommand
    {
    public:
        explicit SmallHistoryCommandSafe(
            uint64_t& value) noexcept
            : value_(&value)
        {
        }

        [[nodiscard]] const char* Label() const noexcept override
        {
            return "Perf Small Command";
        }

        [[nodiscard]] std::size_t
        MemoryCostBytes() const noexcept override
        {
            return sizeof(SmallHistoryCommandSafe);
        }

        [[nodiscard]] bool Execute(
            nocturne::editor::EditorCommandContext&) override
        {
            return Apply_();
        }

        [[nodiscard]] bool Undo(
            nocturne::editor::EditorCommandContext&) override
        {
            if (!value_ || !applied_ || *value_ == 0)
                return false;

            --(*value_);
            applied_ = false;
            return true;
        }

        [[nodiscard]] bool Redo(
            nocturne::editor::EditorCommandContext&) override
        {
            return Apply_();
        }

    private:
        [[nodiscard]] bool Apply_() noexcept
        {
            if (!value_ || applied_)
                return false;

            ++(*value_);
            applied_ = true;
            return true;
        }

        uint64_t* value_ = nullptr;
        bool applied_ = false;
    };

    bool InitReflection(
        noc::ReflectionRegistry& reflection,
        noc::IAllocator& allocator)
    {
        return reflection.Init(allocator, 32)
            && noc::RegisterBuiltinReflectionTypes(
                reflection)
            && noc::RegisterFoundationComponentReflectionTypes(
                reflection)
            && reflection.Freeze();
    }

    bool CreateTransformEntities(
        noc::World& world,
        uint32_t count,
        std::vector<noc::EntityHandle>& outEntities)
    {
        try
        {
            outEntities.clear();
            outEntities.reserve(count);

            for (uint32_t i = 0; i < count; ++i)
            {
                const noc::EntityHandle entity =
                    world.CreateEntity();

                if (!entity.IsValid()
                    || !world.AddTransform(entity))
                {
                    return false;
                }

                outEntities.push_back(entity);
            }
        }
        catch (const std::bad_alloc&)
        {
            return false;
        }

        return outEntities.size() == count;
    }

    bool RunHierarchyScale(
        noc::IAllocator& allocator,
        const noc::ReflectionRegistry& reflection,
        uint32_t count)
    {
        noc::World world;
        nocturne::editor::EditorHierarchyModel model;
        std::vector<noc::EntityHandle> entities;

        bool ok = true;

        ok &= CheckEditorPerf(
            world.Init(allocator, reflection),
            "Hierarchy perf World init failed");

        if (!ok)
            return false;

        ok &= CheckEditorPerf(
            CreateTransformEntities(
                world,
                count,
                entities),
            "Hierarchy perf entity creation failed");

        const noc::EntityHandle toolOwned =
            world.CreateEntity();

        ok &= CheckEditorPerf(
            toolOwned.IsValid()
                && world.AddTransform(toolOwned),
            "Hierarchy perf tool-owned entity setup failed");

        const auto begin = Clock::now();
        const bool built =
            model.Rebuild(world, toolOwned);
        const auto end = Clock::now();

        ok &= CheckEditorPerf(
            built
                && model.RowCount() == count,
            "Hierarchy perf row count/tool-owned filtering mismatch");

        bool allRoots = true;
        for (const auto& row : model.Rows())
        {
            if (row.depth != 1
                || row.hasAuthoredChildren
                || row.entity == toolOwned)
            {
                allRoots = false;
                break;
            }
        }

        ok &= CheckEditorPerf(
            allRoots,
            "Hierarchy perf root projection mismatch");

        const uint32_t firstCapacityGrowth =
            model.LastCapacityGrowthCount();
        const std::size_t retainedHierarchyBytes =
            model.EstimatedRetainedBytes();

        NOC_LOG_INFO(
            "Phase16EditorPerf",
            "editor_hierarchy_rebuild: entities=%u time_us=%lld capacity_growth=%u retained_bytes=%zu",
            count,
            Micros(begin, end),
            firstCapacityGrowth,
            retainedHierarchyBytes);

        std::vector<nocturne::editor::EditorHierarchyRow>
            firstBuild = model.Rows();

        ok &= CheckEditorPerf(
            model.Rebuild(world, toolOwned)
                && model.Rows().size()
                    == firstBuild.size(),
            "Hierarchy deterministic second rebuild failed");

        // The first swap can leave nextRows_ without capacity. One warm rebuild
        // fills both alternating row buffers; steady-state rebuild must then
        // require no further STL capacity growth.
        ok &= CheckEditorPerf(
            model.Rebuild(world, toolOwned)
                && model.LastCapacityGrowthCount() == 0,
            "Warmed hierarchy rebuild grew STL capacity");

        NOC_LOG_INFO(
            "Phase16EditorPerf",
            "editor_hierarchy_allocation: entities=%u steady_capacity_growth=%u retained_bytes=%zu",
            count,
            model.LastCapacityGrowthCount(),
            model.EstimatedRetainedBytes());

        if (ok)
        {
            for (std::size_t i = 0;
                 i < firstBuild.size();
                 ++i)
            {
                if (firstBuild[i].entity
                        != model.Rows()[i].entity
                    || firstBuild[i].depth
                        != model.Rows()[i].depth
                    || firstBuild[i].hasAuthoredChildren
                        != model.Rows()[i].hasAuthoredChildren)
                {
                    ok &= CheckEditorPerf(
                        false,
                        "Hierarchy rebuild order is not deterministic");
                    break;
                }
            }
        }

        world.Shutdown();
        return ok;
    }

    bool RunHierarchyShape(
        noc::IAllocator& allocator,
        const noc::ReflectionRegistry& reflection,
        bool deep)
    {
        constexpr uint32_t kCount = 1000;

        noc::World world;
        nocturne::editor::EditorHierarchyModel model;
        std::vector<noc::EntityHandle> entities;
        bool ok = true;

        ok &= CheckEditorPerf(
            world.Init(allocator, reflection)
                && CreateTransformEntities(
                    world,
                    kCount,
                    entities),
            "Hierarchy shape setup failed");

        if (!ok)
        {
            world.Shutdown();
            return false;
        }

        if (deep)
        {
            for (uint32_t i = 1; i < kCount; ++i)
            {
                if (!world.SetParent(
                        entities[i],
                        entities[i - 1u]))
                {
                    ok &= CheckEditorPerf(
                        false,
                        "Deep hierarchy parent setup failed");
                    break;
                }
            }
        }
        else
        {
            for (uint32_t i = 1; i < kCount; ++i)
            {
                if (!world.SetParent(
                        entities[i],
                        entities[0]))
                {
                    ok &= CheckEditorPerf(
                        false,
                        "Wide hierarchy parent setup failed");
                    break;
                }
            }
        }

        const auto begin = Clock::now();
        const bool built =
            model.Rebuild(world);
        const auto end = Clock::now();

        ok &= CheckEditorPerf(
            built
                && model.RowCount() == kCount,
            "Hierarchy shape projection failed");

        if (built && model.RowCount() == kCount)
        {
            if (deep)
            {
                ok &= CheckEditorPerf(
                    model.Rows().front().depth == 1
                        && model.Rows().back().depth
                            == static_cast<int>(kCount)
                        && model.Rows().front()
                            .hasAuthoredChildren,
                    "Deep hierarchy depth projection mismatch");
            }
            else
            {
                bool childrenDepthTwo = true;

                for (std::size_t i = 1;
                     i < model.Rows().size();
                     ++i)
                {
                    if (model.Rows()[i].depth != 2)
                    {
                        childrenDepthTwo = false;
                        break;
                    }
                }

                ok &= CheckEditorPerf(
                    model.Rows().front().depth == 1
                        && model.Rows().front()
                            .hasAuthoredChildren
                        && childrenDepthTwo,
                    "Wide hierarchy depth projection mismatch");
            }
        }

        NOC_LOG_INFO(
            "Phase16EditorPerf",
            "editor_hierarchy_%s: entities=%u time_us=%lld",
            deep ? "deep" : "wide",
            kCount,
            Micros(begin, end));

        world.Shutdown();
        return ok;
    }

    bool RunHistoryStress(
        noc::IAllocator& allocator,
        const noc::ReflectionRegistry& reflection)
    {
        constexpr uint32_t kCommandCount = 10000;

        noc::World world;
        bool ok = true;

        ok &= CheckEditorPerf(
            world.Init(allocator, reflection),
            "History perf World init failed");

        if (!ok)
            return false;

        nocturne::editor::EditorCommandContext context{
            world,
            reflection,
            allocator,
            noc::EntityHandle::Invalid()
        };

        nocturne::editor::EditorCommandHistory history;
        history.Configure(
            kCommandCount,
            8u * 1024u * 1024u);

        uint64_t value = 0;

        const auto pushBegin = Clock::now();

        for (uint32_t i = 0;
             i < kCommandCount;
             ++i)
        {
            if (!history.Execute(
                    context,
                    std::make_unique<
                        SmallHistoryCommandSafe>(
                            value)))
            {
                ok &= CheckEditorPerf(
                    false,
                    "10k history push failed");
                break;
            }
        }

        const auto pushEnd = Clock::now();
        const std::size_t retainedBytes =
            history.UsedBytes();

        ok &= CheckEditorPerf(
            history.CommandCount() == kCommandCount
                && history.Cursor() == kCommandCount
                && value == kCommandCount,
            "10k history post-push state mismatch");

        const auto undoBegin = Clock::now();

        for (uint32_t i = 0;
             i < kCommandCount;
             ++i)
        {
            if (!history.Undo(context))
            {
                ok &= CheckEditorPerf(
                    false,
                    "10k history undo failed");
                break;
            }
        }

        const auto undoEnd = Clock::now();

        ok &= CheckEditorPerf(
            history.Cursor() == 0
                && value == 0,
            "10k history post-undo state mismatch");

        const auto redoBegin = Clock::now();

        for (uint32_t i = 0;
             i < kCommandCount;
             ++i)
        {
            if (!history.Redo(context))
            {
                ok &= CheckEditorPerf(
                    false,
                    "10k history redo failed");
                break;
            }
        }

        const auto redoEnd = Clock::now();

        ok &= CheckEditorPerf(
            history.Cursor() == kCommandCount
                && value == kCommandCount,
            "10k history post-redo state mismatch");

        NOC_LOG_INFO(
            "Phase16EditorPerf",
            "editor_history_10k: push_us=%lld undo_us=%lld redo_us=%lld retained_bytes=%zu",
            Micros(pushBegin, pushEnd),
            Micros(undoBegin, undoEnd),
            Micros(redoBegin, redoEnd),
            retainedBytes);

        history.Clear();
        world.Shutdown();
        return ok;
    }

    bool RunInspectorRefreshBaseline(
        noc::DebugAlloc& allocator,
        const noc::ReflectionRegistry& reflection)
    {
        constexpr uint32_t kRefreshCount = 1000;

        noc::World world;
        nocturne::editor::EditorInspectorModel model;
        bool ok = true;

        ok &= CheckEditorPerf(
            world.Init(allocator, reflection),
            "Inspector perf World init failed");

        const noc::EntityHandle entity =
            world.CreateEntity();

        ok &= CheckEditorPerf(
            entity.IsValid()
                && world.AddName(
                    entity,
                    "Inspector Perf")
                && world.AddTransform(entity)
                && world.AddRenderable(
                    entity,
                    noc::ResourceHandle{},
                    noc::AABB{
                        noc::Vec3{
                            -1.0f, -1.0f, -1.0f },
                        noc::Vec3{
                            1.0f, 1.0f, 1.0f } })
                && world.AddCamera(entity),
            "Inspector perf entity setup failed");

        nocturne::editor::EditorCommandContext context{
            world,
            reflection,
            allocator,
            noc::EntityHandle::Invalid()
        };

        const std::size_t allocationsBefore =
            allocator.AllocationCount();
        const auto begin = Clock::now();

        uint32_t refreshHits = 0;
        for (uint32_t i = 0;
             i < kRefreshCount;
             ++i)
        {
            if (model.Refresh(
                    context,
                    entity))
            {
                ++refreshHits;
            }
        }

        const auto end = Clock::now();

        ok &= CheckEditorPerf(
            refreshHits == kRefreshCount
                && !model.Components().empty(),
            "Inspector refresh workload failed");

        const std::size_t allocationCalls =
            allocator.AllocationCount()
            - allocationsBefore;

        NOC_LOG_INFO(
            "Phase16EditorPerf",
            "editor_inspector_refresh: refreshes=%u time_us=%lld allocator_calls=%zu retained_model_bytes=%zu",
            kRefreshCount,
            Micros(begin, end),
            allocationCalls,
            model.EstimatedRetainedBytes());

        model.Clear();
        world.Shutdown();
        return ok;
    }

    bool RunSelectionAndCreateBaseline(
        noc::IAllocator& allocator,
        const noc::ReflectionRegistry& reflection)
    {
        constexpr uint32_t kEntityCount = 1000;
        constexpr uint32_t kSelectionCount = 100000;

        noc::World world;
        nocturne::editor::EditorSession session;
        std::vector<noc::EntityHandle> entities;
        bool ok = true;

        ok &= CheckEditorPerf(
            world.Init(allocator, reflection)
                && session.Init(
                    world,
                    reflection,
                    allocator,
                    2048,
                    8u * 1024u * 1024u),
            "Selection/create perf setup failed");

        if (!ok)
        {
            session.Shutdown();
            world.Shutdown();
            return false;
        }

        auto context =
            session.CommandContext();

        const auto createBegin = Clock::now();

        for (uint32_t i = 0;
             i < kEntityCount;
             ++i)
        {
            auto command =
                std::make_unique<
                    nocturne::editor::CreateEntityCommand>();

            if (!command->Init(
                    "Perf Entity")
                || !session.History().Execute(
                    context,
                    std::move(command)))
            {
                ok &= CheckEditorPerf(
                    false,
                    "Create 1k command workload failed");
                break;
            }
        }

        const auto createEnd = Clock::now();

        ok &= CheckEditorPerf(
            world.AliveCount() == kEntityCount
                && session.History().CommandCount()
                    == kEntityCount,
            "Create 1k post-state mismatch");

        try
        {
            entities.reserve(kEntityCount);

            for (uint32_t i = 0;
                 i < world.EntityCapacity();
                 ++i)
            {
                const noc::EntityHandle entity =
                    world.EntityAtIndex(i);

                if (entity.IsValid())
                    entities.push_back(entity);
            }
        }
        catch (const std::bad_alloc&)
        {
            ok &= CheckEditorPerf(
                false,
                "Selection perf entity enumeration allocation failed");
        }

        const auto selectionBegin =
            Clock::now();

        uint32_t selectionHits = 0;
        if (!entities.empty())
        {
            for (uint32_t i = 0;
                 i < kSelectionCount;
                 ++i)
            {
                if (session.SetSelection(
                        entities[
                            i % entities.size()]))
                {
                    ++selectionHits;
                }
            }
        }

        const auto selectionEnd =
            Clock::now();

        ok &= CheckEditorPerf(
            selectionHits == kSelectionCount
                && session.SelectedEntity().IsValid(),
            "Selection latency workload failed");

        NOC_LOG_INFO(
            "Phase16EditorPerf",
            "editor_create_1k: entities=%u time_us=%lld retained_history_bytes=%zu",
            kEntityCount,
            Micros(createBegin, createEnd),
            session.History().UsedBytes());

        NOC_LOG_INFO(
            "Phase16EditorPerf",
            "editor_selection: selections=%u entities=%u time_us=%lld",
            kSelectionCount,
            static_cast<uint32_t>(
                entities.size()),
            Micros(
                selectionBegin,
                selectionEnd));

        session.Shutdown();
        world.Shutdown();
        return ok;
    }

    bool RunReparentAndGizmoCommitBaseline(
        noc::DebugAlloc& allocator,
        const noc::ReflectionRegistry& reflection)
    {
        noc::World world;
        bool ok = true;

        ok &= CheckEditorPerf(
            world.Init(allocator, reflection),
            "Reparent/gizmo perf World init failed");

        if (!ok)
            return false;

        const noc::EntityHandle oldParent =
            world.CreateEntity();
        const noc::EntityHandle newParent =
            world.CreateEntity();
        const noc::EntityHandle child =
            world.CreateEntity();

        ok &= CheckEditorPerf(
            oldParent.IsValid()
                && newParent.IsValid()
                && child.IsValid()
                && world.AddTransform(oldParent)
                && world.AddTransform(newParent)
                && world.AddTransform(child)
                && world.SetLocalTRS(
                    oldParent,
                    noc::Vec3{
                        3.0f, 0.0f, 0.0f },
                    noc::Quat::Identity(),
                    noc::Vec3::One())
                && world.SetLocalTRS(
                    newParent,
                    noc::Vec3{
                        -4.0f, 1.0f, 0.0f },
                    noc::Quat::Identity(),
                    noc::Vec3::One())
                && world.SetLocalTRS(
                    child,
                    noc::Vec3{
                        1.0f, 2.0f, 3.0f },
                    noc::Quat::Identity(),
                    noc::Vec3::One())
                && world.SetParent(
                    child,
                    oldParent),
            "Reparent/gizmo perf transform setup failed");

        world.Update();

        nocturne::editor::EditorCommandContext context{
            world,
            reflection,
            allocator,
            noc::EntityHandle::Invalid()
        };

        nocturne::editor::EditorCommandHistory history;
        history.Configure(
            32,
            1024u * 1024u);

        const auto reparentBegin =
            Clock::now();

        auto reparent =
            std::make_unique<
                nocturne::editor::ReparentEntityCommand>();

        const bool reparentOk =
            reparent->Init(
                context,
                child,
                newParent)
            && history.Execute(
                context,
                std::move(reparent));

        const auto reparentEnd =
            Clock::now();

        ok &= CheckEditorPerf(
            reparentOk
                && world.ParentOf(child)
                    == newParent,
            "Isolated reparent workload failed");

        NOC_LOG_INFO(
            "Phase16EditorPerf",
            "editor_reparent_commit: time_us=%lld",
            Micros(
                reparentBegin,
                reparentEnd));

        history.Clear();

        nocturne::editor::EditorGizmoDragTransaction
            dragTransaction;

        ok &= CheckEditorPerf(
            dragTransaction.Begin(
                context,
                child,
                nocturne::editor::EditorTool::Move,
                nocturne::editor::TransformOrientation::World,
                0),
            "Gizmo hot-path transaction begin failed");

        const std::size_t allocationsBeforePreview =
            allocator.AllocationCount();

        constexpr uint32_t kPreviewUpdates = 10000;
        uint32_t previewHits = 0;

        const auto previewBegin = Clock::now();

        for (uint32_t i = 0;
             i < kPreviewUpdates;
             ++i)
        {
            const float delta =
                static_cast<float>(i % 100u)
                * 0.001f;

            if (dragTransaction.PreviewMove(
                    context,
                    delta))
            {
                ++previewHits;
            }
        }

        const auto previewEnd = Clock::now();

        ok &= CheckEditorPerf(
            previewHits == kPreviewUpdates
                && allocator.AllocationCount()
                    == allocationsBeforePreview,
            "Gizmo mouse-move preview performed engine allocator calls");

        ok &= CheckEditorPerf(
            dragTransaction.Cancel(context),
            "Gizmo hot-path transaction cancel failed");

        NOC_LOG_INFO(
            "Phase16EditorPerf",
            "editor_gizmo_preview: updates=%u time_us=%lld allocator_calls=%zu",
            kPreviewUpdates,
            Micros(previewBegin, previewEnd),
            allocator.AllocationCount()
                - allocationsBeforePreview);

        const noc::TransformComponent* transform =
            world.GetTransform(child);

        noc::Vec3 oldTranslation{};
        noc::Quat oldRotation{};
        noc::Vec3 oldScale{};

        if (transform)
        {
            oldTranslation =
                transform->localTranslation;
            oldRotation =
                transform->localRotation;
            oldScale =
                transform->localScale;
        }

        const noc::Vec3 newTranslation =
            oldTranslation
            + noc::Vec3{
                0.25f, -0.5f, 1.0f };
        const noc::Quat newRotation =
            oldRotation;
        const noc::Vec3 newScale =
            oldScale;

        ok &= CheckEditorPerf(
            transform
                && world.SetLocalTRS(
                    child,
                    newTranslation,
                    newRotation,
                    newScale),
            "Gizmo commit live-state setup failed");

        const auto gizmoCommitBegin =
            Clock::now();

        auto transformCommand =
            std::make_unique<
                nocturne::editor::SetTransformTRSCommand>();

        const bool gizmoCommitOk =
            transformCommand->InitExplicit(
                context,
                child,
                oldTranslation,
                oldRotation,
                oldScale,
                newTranslation,
                newRotation,
                newScale)
            && history.RecordExecuted(
                context,
                std::move(transformCommand));

        const auto gizmoCommitEnd =
            Clock::now();

        ok &= CheckEditorPerf(
            gizmoCommitOk
                && history.CommandCount() == 1
                && history.Cursor() == 1,
            "Isolated gizmo history commit failed");

        NOC_LOG_INFO(
            "Phase16EditorPerf",
            "editor_gizmo_commit: time_us=%lld",
            Micros(
                gizmoCommitBegin,
                gizmoCommitEnd));

        ok &= CheckEditorPerf(
            history.Undo(context),
            "Gizmo commit perf undo failed");

        history.Clear();
        world.Shutdown();
        return ok;
    }

    bool RunSubtreeStress(
        noc::IAllocator& allocator,
        const noc::ReflectionRegistry& reflection)
    {
        constexpr uint32_t kNodeCount = 1000;

        noc::World world;
        std::vector<noc::EntityHandle> entities;
        bool ok = true;

        ok &= CheckEditorPerf(
            world.Init(allocator, reflection)
                && CreateTransformEntities(
                    world,
                    kNodeCount,
                    entities),
            "Subtree perf entity setup failed");

        if (!ok)
        {
            world.Shutdown();
            return false;
        }

        for (uint32_t i = 0;
             i < kNodeCount;
             ++i)
        {
            if (!world.AddName(
                    entities[i],
                    "Stress Node"))
            {
                ok &= CheckEditorPerf(
                    false,
                    "Subtree perf Name setup failed");
                break;
            }

            if (i > 0
                && !world.SetParent(
                    entities[i],
                    entities[0]))
            {
                ok &= CheckEditorPerf(
                    false,
                    "Subtree perf parent setup failed");
                break;
            }
        }

        nocturne::editor::EditorCommandContext context{
            world,
            reflection,
            allocator,
            noc::EntityHandle::Invalid()
        };

        nocturne::editor::EditorCommandHistory history;
        history.Configure(
            16,
            64u * 1024u * 1024u);

        auto deleteCommand =
            std::make_unique<
                nocturne::editor::DeleteEntityCommand>();
        auto* deleteRaw =
            deleteCommand.get();

        const auto deleteBegin = Clock::now();

        const bool deleteOk =
            deleteCommand->Init(
                context,
                entities[0])
            && history.Execute(
                context,
                std::move(deleteCommand));

        const auto deleteEnd = Clock::now();

        ok &= CheckEditorPerf(
            deleteOk
                && world.AliveCount() == 0,
            "1k subtree delete failed");

        const auto undoDeleteBegin =
            Clock::now();
        const bool undoDeleteOk =
            history.Undo(context);
        const auto undoDeleteEnd =
            Clock::now();

        const noc::EntityHandle restoredRoot =
            deleteRaw->CurrentRoot();

        ok &= CheckEditorPerf(
            undoDeleteOk
                && restoredRoot.IsValid()
                && world.IsAlive(restoredRoot)
                && world.AliveCount() == kNodeCount,
            "1k subtree delete undo failed");

        NOC_LOG_INFO(
            "Phase16EditorPerf",
            "editor_subtree_delete_1k: nodes=%u delete_us=%lld undo_us=%lld retained_bytes=%zu",
            kNodeCount,
            Micros(deleteBegin, deleteEnd),
            Micros(
                undoDeleteBegin,
                undoDeleteEnd),
            history.UsedBytes());

        history.Clear();

        auto duplicateCommand =
            std::make_unique<
                nocturne::editor::DuplicateEntityCommand>();

        const auto duplicateBegin =
            Clock::now();

        const bool duplicateOk =
            duplicateCommand->Init(
                context,
                restoredRoot)
            && history.Execute(
                context,
                std::move(duplicateCommand));

        const auto duplicateEnd =
            Clock::now();

        ok &= CheckEditorPerf(
            duplicateOk
                && world.AliveCount()
                    == kNodeCount * 2u,
            "1k subtree duplicate failed");

        const auto undoDuplicateBegin =
            Clock::now();
        const bool undoDuplicateOk =
            history.Undo(context);
        const auto undoDuplicateEnd =
            Clock::now();

        ok &= CheckEditorPerf(
            undoDuplicateOk
                && world.AliveCount()
                    == kNodeCount,
            "1k subtree duplicate undo failed");

        NOC_LOG_INFO(
            "Phase16EditorPerf",
            "editor_subtree_duplicate_1k: nodes=%u duplicate_us=%lld undo_us=%lld retained_bytes=%zu",
            kNodeCount,
            Micros(
                duplicateBegin,
                duplicateEnd),
            Micros(
                undoDuplicateBegin,
                undoDuplicateEnd),
            history.UsedBytes());

        history.Clear();
        world.Shutdown();
        return ok;
    }
}

bool RunPhase16EditorPerfTests()
{
    NOC_LOG_INFO(
        "Phase16EditorPerf",
        "%s",
        "Editor stress/performance baseline begin "
        "(timings are observations, not CI timing budgets)");

    noc::MallocAllocator backing;
    noc::DebugAlloc allocator(backing);
    noc::ReflectionRegistry reflection;

    bool ok = true;

    ok &= CheckEditorPerf(
        InitReflection(
            reflection,
            allocator),
        "Editor perf reflected schema setup failed");

    if (ok)
    {
        ok &= RunHierarchyScale(
            allocator,
            reflection,
            100);
        ok &= RunHierarchyScale(
            allocator,
            reflection,
            1000);
        ok &= RunHierarchyScale(
            allocator,
            reflection,
            10000);
        ok &= RunHierarchyShape(
            allocator,
            reflection,
            false);
        ok &= RunHierarchyShape(
            allocator,
            reflection,
            true);
        ok &= RunHistoryStress(
            allocator,
            reflection);
        ok &= RunInspectorRefreshBaseline(
            allocator,
            reflection);
        ok &= RunSelectionAndCreateBaseline(
            allocator,
            reflection);
        ok &= RunReparentAndGizmoCommitBaseline(
            allocator,
            reflection);
        ok &= RunSubtreeStress(
            allocator,
            reflection);
    }

    reflection.Shutdown();

    ok &= CheckEditorPerf(
        allocator.OutstandingBytes() == 0,
        "Editor stress/performance workloads leaked allocator memory");

    NOC_LOG_INFO(
        "Phase16EditorPerf",
        "Editor stress/performance baseline %s",
        ok ? "PASS" : "FAIL");

    return ok;
}
