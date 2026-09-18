#include "../../NocturneEditor/EditorCommandHistory.h"
#include "../../NocturneEditor/EditorCommands.h"
#include "../../NocturneEditor/EditorHierarchyModel.h"
#include "../../NocturneEditor/EditorInspectorModel.h"

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

        NOC_LOG_INFO(
            "Phase16EditorPerf",
            "editor_hierarchy_rebuild: entities=%u time_us=%lld",
            count,
            Micros(begin, end));

        std::vector<nocturne::editor::EditorHierarchyRow>
            firstBuild = model.Rows();

        ok &= CheckEditorPerf(
            model.Rebuild(world, toolOwned)
                && model.Rows().size()
                    == firstBuild.size(),
            "Hierarchy deterministic second rebuild failed");

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
        noc::IAllocator& allocator,
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

        NOC_LOG_INFO(
            "Phase16EditorPerf",
            "editor_inspector_refresh: refreshes=%u time_us=%lld",
            kRefreshCount,
            Micros(begin, end));

        model.Clear();
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
