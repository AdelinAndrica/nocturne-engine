#include "../../NocturneEditor/EditorCommands.h"
#include "../../NocturneEditor/EditorHierarchyModel.h"
#include "../../NocturneEditor/EditorInspectorModel.h"
#include "../../NocturneEditor/EditorSession.h"
#include "../../NocturneEditor/EditorTransformMath.h"

#include "Core/Log.h"
#include "Core/Memory/Allocator.h"
#include "Core/Memory/DebugAlloc.h"
#include "Runtime/Components/TransformComponent.h"
#include "Runtime/Reflection/BuiltinTypes.h"
#include "Runtime/Reflection/ComponentReflection.h"
#include "Runtime/Reflection/FoundationComponents.h"
#include "Runtime/Reflection/ReflectionRegistry.h"
#include "Runtime/World.h"

#include <cmath>
#include <cstring>
#include <memory>

namespace
{
    bool CheckEditorSession(bool condition, const char* message)
    {
        if (!condition)
        {
            NOC_LOG_ERROR("Phase16Editor", "%s", message);
            return false;
        }
        return true;
    }

    std::unique_ptr<nocturne::editor::SetReflectedPropertyCommand>
    MakeTranslationCommand(
        nocturne::editor::EditorCommandContext& context,
        noc::EntityHandle entity,
        const noc::Vec3& value)
    {
        auto command =
            std::make_unique<
                nocturne::editor::SetReflectedPropertyCommand>();

        if (!command->Init(
                context,
                entity,
                noc::TypeId{
                    noc::kTransformComponentTypeId.value },
                noc::MakePropertyId(
                    "Nocturne.Transform.localTranslation"),
                noc::ReflectedConstValueView{
                    noc::BuiltinTypeIds::Vec3,
                    &value }))
        {
            return {};
        }

        return command;
    }

    enum class EnumDrawerTestMode : uint32_t
    {
        Translate = 0,
        Rotate = 1,
        Scale = 2
    };

    struct EnumDrawerTestComponent
    {
        EnumDrawerTestMode mode = EnumDrawerTestMode::Translate;

        [[nodiscard]] bool operator==(
            const EnumDrawerTestComponent&) const = default;
    };

    constexpr noc::TypeId kEnumDrawerModeTypeId{
        0xE160000000000001ull
    };
    // Reflected component TypeIds must remain representable by the runtime
    // ComponentTypeId compatibility facade. Enum/value TypeIds remain full
    // 64-bit reflection identities.
    constexpr noc::TypeId kEnumDrawerComponentTypeId{
        0x00001601ull
    };

    struct EnumDrawerTestStore
    {
        noc::EntityHandle entity{};
        EnumDrawerTestComponent component{};
        bool has = false;
    };

    EnumDrawerTestStore gEnumDrawerStore;

    bool HasEnumDrawerTestComponent(
        const noc::World& world,
        noc::EntityHandle entity)
    {
        return world.IsAlive(entity)
            && gEnumDrawerStore.has
            && gEnumDrawerStore.entity == entity;
    }

    bool AddEnumDrawerTestComponent(
        noc::World& world,
        noc::EntityHandle entity)
    {
        if (!world.IsAlive(entity)
            || (gEnumDrawerStore.has
                && gEnumDrawerStore.entity == entity))
        {
            return false;
        }

        gEnumDrawerStore.entity = entity;
        gEnumDrawerStore.component = {};
        gEnumDrawerStore.has = true;
        return true;
    }

    bool RemoveEnumDrawerTestComponent(
        noc::World& world,
        noc::EntityHandle entity)
    {
        if (!world.IsAlive(entity)
            || !gEnumDrawerStore.has
            || gEnumDrawerStore.entity != entity)
        {
            return false;
        }

        gEnumDrawerStore = {};
        return true;
    }

    const void* GetEnumDrawerTestComponent(
        const noc::World& world,
        noc::EntityHandle entity)
    {
        return HasEnumDrawerTestComponent(world, entity)
            ? &gEnumDrawerStore.component
            : nullptr;
    }

    void* GetMutableEnumDrawerTestComponent(
        noc::World& world,
        noc::EntityHandle entity)
    {
        return HasEnumDrawerTestComponent(world, entity)
            ? &gEnumDrawerStore.component
            : nullptr;
    }

    bool ReadEnumDrawerMode(
        const noc::PropertyAccessContext& context,
        void* destination)
    {
        const auto* runtime =
            static_cast<
                const noc::ComponentPropertyRuntimeContext*>(
                    context.userContext);

        if (!runtime
            || !runtime->world
            || !destination
            || !HasEnumDrawerTestComponent(
                *runtime->world,
                runtime->entity))
        {
            return false;
        }

        *static_cast<EnumDrawerTestMode*>(
            destination) =
                gEnumDrawerStore.component.mode;
        return true;
    }

    bool WriteEnumDrawerMode(
        noc::PropertyAccessContext& context,
        const void* source)
    {
        auto* runtime =
            static_cast<
                noc::ComponentPropertyRuntimeContext*>(
                    context.userContext);

        if (!runtime
            || !runtime->world
            || !source
            || !HasEnumDrawerTestComponent(
                *runtime->world,
                runtime->entity))
        {
            return false;
        }

        gEnumDrawerStore.component.mode =
            *static_cast<
                const EnumDrawerTestMode*>(
                    source);
        return true;
    }

    bool RegisterEnumDrawerTestReflection(
        noc::ReflectionRegistry& registry)
    {
        constexpr noc::PropertyFlags kAuthorable =
            noc::PropertyFlags::EditorVisible
            | noc::PropertyFlags::Serializable
            | noc::PropertyFlags::ScriptVisible;

        const noc::EnumValueMetadata enumValues[] = {
            noc::MakeEnumValueMetadata<EnumDrawerTestMode>(
                noc::MakeEnumValueId(
                    "Nocturne.Tests.EnumDrawerMode.Translate"),
                "Translate",
                EnumDrawerTestMode::Translate),
            noc::MakeEnumValueMetadata<EnumDrawerTestMode>(
                noc::MakeEnumValueId(
                    "Nocturne.Tests.EnumDrawerMode.Rotate"),
                "Rotate",
                EnumDrawerTestMode::Rotate),
            noc::MakeEnumValueMetadata<EnumDrawerTestMode>(
                noc::MakeEnumValueId(
                    "Nocturne.Tests.EnumDrawerMode.Scale"),
                "Scale",
                EnumDrawerTestMode::Scale)
        };

        const noc::EnumMetadata enumMetadata{
            noc::BuiltinTypeIds::UInt32,
            enumValues,
            3,
            false
        };

        noc::TypeMetadata enumType =
            noc::MakeTypeMetadata<EnumDrawerTestMode>(
                kEnumDrawerModeTypeId,
                "Nocturne.Tests.EnumDrawerMode",
                noc::TypeKind::Enum,
                1,
                noc::TypeFlags::EditorVisible
                    | noc::TypeFlags::Serializable);
        enumType.enumMetadata = &enumMetadata;

        if (!registry.RegisterType(enumType))
            return false;

        const noc::PropertyMetadata properties[] = {
            {
                noc::MakePropertyId(
                    "Nocturne.Tests.EnumDrawerComponent.mode"),
                "mode",
                kEnumDrawerComponentTypeId,
                kEnumDrawerModeTypeId,
                kAuthorable,
                &ReadEnumDrawerMode,
                &WriteEnumDrawerMode
            }
        };

        const noc::ComponentMetadata componentOps{
            noc::ComponentReflectionFlags::EditorAddable
                | noc::ComponentReflectionFlags::EditorRemovable
                | noc::ComponentReflectionFlags::Required,
            &HasEnumDrawerTestComponent,
            &AddEnumDrawerTestComponent,
            &RemoveEnumDrawerTestComponent,
            &GetEnumDrawerTestComponent,
            &GetMutableEnumDrawerTestComponent
        };

        noc::TypeMetadata componentType =
            noc::MakeTypeMetadata<EnumDrawerTestComponent>(
                kEnumDrawerComponentTypeId,
                "Nocturne.Tests.EnumDrawerComponent",
                noc::TypeKind::Component,
                1,
                noc::TypeFlags::EditorVisible
                    | noc::TypeFlags::Serializable);
        componentType.properties = properties;
        componentType.propertyCount = 1;
        componentType.componentMetadata = &componentOps;

        return registry.RegisterType(componentType);
    }

    class HistoryProbeCommand final
        : public nocturne::editor::IEditorCommand
    {
    public:
        HistoryProbeCommand(
            int& value,
            int delta,
            std::size_t memoryCost,
            int& destructionCount,
            bool failExecute = false,
            bool failUndo = false,
            bool failRedo = false) noexcept
            : value_(&value)
            , delta_(delta)
            , memoryCost_(memoryCost)
            , destructionCount_(&destructionCount)
            , failExecute_(failExecute)
            , failUndo_(failUndo)
            , failRedo_(failRedo)
        {
        }

        ~HistoryProbeCommand() override
        {
            if (destructionCount_)
                ++(*destructionCount_);
        }

        [[nodiscard]] const char* Label() const noexcept override
        {
            return "History Probe";
        }

        [[nodiscard]] std::size_t MemoryCostBytes() const noexcept override
        {
            return memoryCost_;
        }

        [[nodiscard]] bool Execute(
            nocturne::editor::EditorCommandContext&) override
        {
            if (failExecute_ || !value_)
                return false;
            *value_ += delta_;
            return true;
        }

        [[nodiscard]] bool Undo(
            nocturne::editor::EditorCommandContext&) override
        {
            if (failUndo_ || !value_)
                return false;
            *value_ -= delta_;
            return true;
        }

        [[nodiscard]] bool Redo(
            nocturne::editor::EditorCommandContext&) override
        {
            if (failRedo_ || !value_)
                return false;
            *value_ += delta_;
            return true;
        }

    private:
        int* value_ = nullptr;
        int delta_ = 0;
        std::size_t memoryCost_ = 0;
        int* destructionCount_ = nullptr;
        bool failExecute_ = false;
        bool failUndo_ = false;
        bool failRedo_ = false;
    };

    class SequenceProbeCommand final
        : public nocturne::editor::IEditorCommand
    {
    public:
        SequenceProbeCommand(
            int& value,
            int digit,
            bool failExecute = false) noexcept
            : value_(&value)
            , digit_(digit)
            , failExecute_(failExecute)
        {
        }

        [[nodiscard]] const char* Label() const noexcept override
        {
            return "Sequence Probe";
        }

        [[nodiscard]] std::size_t MemoryCostBytes() const noexcept override
        {
            return sizeof(SequenceProbeCommand);
        }

        [[nodiscard]] bool Execute(
            nocturne::editor::EditorCommandContext&) override
        {
            if (!value_ || failExecute_ || applied_)
                return false;

            before_ = *value_;
            *value_ = (*value_ * 10) + digit_;
            applied_ = true;
            return true;
        }

        [[nodiscard]] bool Undo(
            nocturne::editor::EditorCommandContext&) override
        {
            if (!value_ || !applied_)
                return false;

            *value_ = before_;
            applied_ = false;
            return true;
        }

        [[nodiscard]] bool Redo(
            nocturne::editor::EditorCommandContext&) override
        {
            if (!value_ || applied_)
                return false;

            before_ = *value_;
            *value_ = (*value_ * 10) + digit_;
            applied_ = true;
            return true;
        }

    private:
        int* value_ = nullptr;
        int digit_ = 0;
        int before_ = 0;
        bool failExecute_ = false;
        bool applied_ = false;
    };
}

bool RunPhase16EditorSessionTests()
{
    NOC_LOG_INFO(
        "Phase16Editor",
        "%s",
        "Editor session/history tests begin");

    noc::MallocAllocator backing;
    noc::DebugAlloc allocator(backing);
    noc::ReflectionRegistry reflection;
    noc::World world;
    nocturne::editor::EditorSession session;

    bool ok = true;

    {
        const noc::Vec3 authoredEuler{
            30.0f,
            -35.0f,
            70.0f
        };

        const noc::Quat authoredQuat =
            nocturne::editor::EditorQuatFromEulerXYZDegrees(
                authoredEuler);

        const noc::Vec3 displayedEuler =
            nocturne::editor::EditorEulerXYZDegreesFromQuat(
                authoredQuat);

        const noc::Quat roundTripQuat =
            nocturne::editor::EditorQuatFromEulerXYZDegrees(
                displayedEuler);

        const float dot =
            authoredQuat.x * roundTripQuat.x
            + authoredQuat.y * roundTripQuat.y
            + authoredQuat.z * roundTripQuat.z
            + authoredQuat.w * roundTripQuat.w;

        ok &= CheckEditorSession(
            std::isfinite(displayedEuler.x)
                && std::isfinite(displayedEuler.y)
                && std::isfinite(displayedEuler.z)
                && std::fabs(std::fabs(dot) - 1.0f) < 1.0e-4f,
            "Editor Euler XYZ quaternion round-trip failed");

        const noc::Vec3 nearSingularEuler{
            12.0f,
            89.0f,
            -27.0f
        };

        const noc::Quat nearSingularQuat =
            nocturne::editor::EditorQuatFromEulerXYZDegrees(
                nearSingularEuler);

        const noc::Vec3 nearSingularDisplay =
            nocturne::editor::EditorEulerXYZDegreesFromQuat(
                nearSingularQuat);

        const noc::Quat nearSingularRoundTrip =
            nocturne::editor::EditorQuatFromEulerXYZDegrees(
                nearSingularDisplay);

        const float nearSingularDot =
            nearSingularQuat.x * nearSingularRoundTrip.x
            + nearSingularQuat.y * nearSingularRoundTrip.y
            + nearSingularQuat.z * nearSingularRoundTrip.z
            + nearSingularQuat.w * nearSingularRoundTrip.w;

        ok &= CheckEditorSession(
            std::fabs(
                std::fabs(nearSingularDot) - 1.0f)
                < 2.0e-4f,
            "Editor Euler XYZ near-gimbal round-trip failed");
    }

    ok &= CheckEditorSession(
        reflection.Init(allocator, 32)
            && noc::RegisterBuiltinReflectionTypes(reflection)
            && noc::RegisterFoundationComponentReflectionTypes(reflection)
            && RegisterEnumDrawerTestReflection(reflection)
            && reflection.Freeze(),
        "Editor-session reflected schema setup failed");

    ok &= CheckEditorSession(
        world.Init(allocator, reflection),
        "Editor-session World init failed");

    ok &= CheckEditorSession(
        session.Init(world, reflection, allocator, 3, 4096),
        "EditorSession init failed");

    const noc::EntityHandle authored = world.CreateEntity();
    const noc::EntityHandle camera = world.CreateEntity();

    ok &= CheckEditorSession(
        authored.IsValid()
            && camera.IsValid()
            && world.AddTransform(authored)
            && world.AddName(authored, "Inspector Entity")
            && world.AddTransform(camera),
        "Editor-session entity setup failed");

    ok &= CheckEditorSession(
        session.SetToolCamera(camera),
        "Tool-camera registration failed");
    ok &= CheckEditorSession(
        session.IsToolOwned(camera)
            && !session.SetSelection(camera),
        "Tool camera became authored selection");

    ok &= CheckEditorSession(
        session.SetSelection(authored)
            && session.SelectedEntity() == authored,
        "EntityHandle selection failed");

    nocturne::editor::EditorCommandContext context =
        session.CommandContext();

    {
        noc::World hierarchyWorld;
        nocturne::editor::EditorHierarchyModel hierarchyModel;

        ok &= CheckEditorSession(
            hierarchyWorld.Init(
                allocator,
                reflection)
                && hierarchyModel.Rebuild(
                    hierarchyWorld)
                && hierarchyModel.RowCount() == 0,
            "Hierarchy empty-world projection failed");

        const noc::EntityHandle rootA =
            hierarchyWorld.CreateEntity();

        ok &= CheckEditorSession(
            rootA.IsValid()
                && hierarchyWorld.AddTransform(rootA)
                && hierarchyWorld.AddName(
                    rootA,
                    "Duplicate Name")
                && hierarchyModel.Rebuild(
                    hierarchyWorld)
                && hierarchyModel.RowCount() == 1
                && hierarchyModel.Rows()[0].entity
                    == rootA
                && hierarchyModel.Rows()[0].depth == 1
                && !hierarchyModel.Rows()[0]
                    .hasAuthoredChildren,
            "Hierarchy one-root projection failed");

        const noc::EntityHandle rootB =
            hierarchyWorld.CreateEntity();
        const noc::EntityHandle rootC =
            hierarchyWorld.CreateEntity();

        ok &= CheckEditorSession(
            rootB.IsValid()
                && rootC.IsValid()
                && hierarchyWorld.AddTransform(rootB)
                && hierarchyWorld.AddTransform(rootC)
                && hierarchyWorld.AddName(
                    rootB,
                    "Duplicate Name")
                && hierarchyWorld.AddName(
                    rootC,
                    "Third Root")
                && hierarchyModel.Rebuild(
                    hierarchyWorld)
                && hierarchyModel.RowCount() == 3,
            "Hierarchy many-root projection failed");

        bool sawRootA = false;
        bool sawRootB = false;
        bool sawRootC = false;
        bool allRootDepths = true;

        for (const auto& row :
             hierarchyModel.Rows())
        {
            sawRootA |= row.entity == rootA;
            sawRootB |= row.entity == rootB;
            sawRootC |= row.entity == rootC;
            allRootDepths &= row.depth == 1;
        }

        ok &= CheckEditorSession(
            sawRootA
                && sawRootB
                && sawRootC
                && allRootDepths
                && std::strcmp(
                    hierarchyWorld.GetName(rootA)->value,
                    hierarchyWorld.GetName(rootB)->value)
                    == 0,
            "Hierarchy duplicate-name identity aliased rows");

        const std::vector<
            nocturne::editor::EditorHierarchyExpansionEntry>
            expansionState{
                { rootA, false },
                { rootB, true }
            };

        ok &= CheckEditorSession(
            !nocturne::editor::EditorHierarchyWasExpanded(
                expansionState,
                rootA,
                true)
                && nocturne::editor::EditorHierarchyWasExpanded(
                    expansionState,
                    rootB,
                    false)
                && !nocturne::editor::EditorHierarchyWasExpanded(
                    expansionState,
                    rootC,
                    false),
            "Hierarchy expand/collapse identity state lookup failed");

        const noc::EntityHandle child =
            hierarchyWorld.CreateEntity();
        const noc::EntityHandle grandChild =
            hierarchyWorld.CreateEntity();

        ok &= CheckEditorSession(
            child.IsValid()
                && grandChild.IsValid()
                && hierarchyWorld.AddTransform(child)
                && hierarchyWorld.AddTransform(grandChild)
                && hierarchyWorld.AddName(
                    child,
                    "Lifecycle Child")
                && hierarchyWorld.AddName(
                    grandChild,
                    "Lifecycle GrandChild")
                && hierarchyWorld.SetParent(
                    child,
                    rootA)
                && hierarchyWorld.SetParent(
                    grandChild,
                    child)
                && hierarchyModel.Rebuild(
                    hierarchyWorld),
            "Hierarchy parented lifecycle setup failed");

        auto findHierarchyRow =
            [&](noc::EntityHandle entity)
                -> const nocturne::editor::EditorHierarchyRow*
            {
                for (const auto& row :
                     hierarchyModel.Rows())
                {
                    if (row.entity == entity)
                        return &row;
                }

                return nullptr;
            };

        const auto* rootARow =
            findHierarchyRow(rootA);
        const auto* childRow =
            findHierarchyRow(child);
        const auto* grandChildRow =
            findHierarchyRow(grandChild);

        ok &= CheckEditorSession(
            rootARow
                && rootARow->depth == 1
                && rootARow->hasAuthoredChildren
                && childRow
                && childRow->depth == 2
                && childRow->hasAuthoredChildren
                && grandChildRow
                && grandChildRow->depth == 3
                && !grandChildRow->hasAuthoredChildren,
            "Hierarchy deep parent projection mismatch");

        ok &= CheckEditorSession(
            hierarchyWorld.SetName(
                rootA,
                "Renamed Root")
                && hierarchyModel.Rebuild(
                    hierarchyWorld)
                && findHierarchyRow(rootA)
                && std::strcmp(
                    hierarchyWorld.GetName(rootA)->value,
                    "Renamed Root") == 0,
            "Hierarchy rename refresh lost entity identity");

        nocturne::editor::EditorCommandContext
            hierarchyContext{
                hierarchyWorld,
                reflection,
                allocator,
                noc::EntityHandle::Invalid()
            };
        nocturne::editor::EditorCommandHistory
            hierarchyHistory;
        hierarchyHistory.Configure(
            32,
            4u * 1024u * 1024u);

        auto reparent =
            std::make_unique<
                nocturne::editor::ReparentEntityCommand>();

        ok &= CheckEditorSession(
            reparent->Init(
                hierarchyContext,
                child,
                rootB)
                && hierarchyHistory.Execute(
                    hierarchyContext,
                    std::move(reparent))
                && hierarchyModel.Rebuild(
                    hierarchyWorld)
                && hierarchyWorld.ParentOf(child)
                    == rootB,
            "Hierarchy reparent refresh failed");

        const auto* rootAAfterReparent =
            findHierarchyRow(rootA);
        const auto* rootBAfterReparent =
            findHierarchyRow(rootB);
        const auto* childAfterReparent =
            findHierarchyRow(child);

        ok &= CheckEditorSession(
            rootAAfterReparent
                && !rootAAfterReparent
                    ->hasAuthoredChildren
                && rootBAfterReparent
                && rootBAfterReparent
                    ->hasAuthoredChildren
                && childAfterReparent
                && childAfterReparent->depth == 2,
            "Hierarchy reparent topology projection mismatch");

        auto cycle =
            std::make_unique<
                nocturne::editor::ReparentEntityCommand>();

        ok &= CheckEditorSession(
            !cycle->Init(
                hierarchyContext,
                rootB,
                grandChild)
                && hierarchyWorld.ParentOf(rootB)
                    == noc::EntityHandle::Invalid(),
            "Hierarchy cycle reparent was not rejected");

        noc::EntityHandle staleRow{};
        for (const auto& row :
             hierarchyModel.Rows())
        {
            if (row.entity == rootC)
            {
                staleRow = row.entity;
                break;
            }
        }

        ok &= CheckEditorSession(
            staleRow == rootC
                && hierarchyWorld.DestroyEntity(rootC)
                && !hierarchyWorld.IsAlive(staleRow)
                && hierarchyModel.Rebuild(
                    hierarchyWorld)
                && !findHierarchyRow(staleRow),
            "Hierarchy stale row survived authoritative rebuild");

        hierarchyHistory.Clear();

        auto deleteCommand =
            std::make_unique<
                nocturne::editor::DeleteEntityCommand>();
        auto* deleteRaw =
            deleteCommand.get();

        ok &= CheckEditorSession(
            deleteCommand->Init(
                hierarchyContext,
                child)
                && hierarchyHistory.Execute(
                    hierarchyContext,
                    std::move(deleteCommand))
                && hierarchyModel.Rebuild(
                    hierarchyWorld)
                && !findHierarchyRow(child)
                && !findHierarchyRow(grandChild),
            "Hierarchy delete refresh retained deleted subtree rows");

        ok &= CheckEditorSession(
            hierarchyHistory.Undo(
                hierarchyContext),
            "Hierarchy delete undo failed");

        const noc::EntityHandle restoredChild =
            deleteRaw->CurrentRoot();
        const noc::EntityHandle restoredGrandChild =
            hierarchyWorld.FirstChildOf(
                restoredChild);

        ok &= CheckEditorSession(
            restoredChild.IsValid()
                && restoredGrandChild.IsValid()
                && restoredChild != child
                && restoredGrandChild != grandChild
                && hierarchyModel.Rebuild(
                    hierarchyWorld)
                && findHierarchyRow(restoredChild)
                && findHierarchyRow(restoredGrandChild)
                && hierarchyWorld.ParentOf(
                    restoredChild) == rootB,
            "Hierarchy undo restore refresh failed");

        ok &= CheckEditorSession(
            hierarchyHistory.Redo(
                hierarchyContext)
                && hierarchyModel.Rebuild(
                    hierarchyWorld)
                && !findHierarchyRow(restoredChild)
                && !findHierarchyRow(restoredGrandChild),
            "Hierarchy delete redo refresh failed");

        hierarchyHistory.Clear();
        hierarchyModel.Clear();
        hierarchyWorld.Shutdown();
    }

    {
        int sequenceValue = 0;

        session.History().Clear();

        ok &= CheckEditorSession(
            session.BeginTransaction(
                "Compound Sequence")
                && session.HasActiveTransaction()
                && !session.BeginTransaction(
                    "Nested Transaction"),
            "Transaction begin/nested policy failed");

        ok &= CheckEditorSession(
            session.AppendTransactionCommand(
                std::make_unique<SequenceProbeCommand>(
                    sequenceValue,
                    1))
                && session.AppendTransactionCommand(
                    std::make_unique<SequenceProbeCommand>(
                        sequenceValue,
                        2))
                && session.ActiveTransactionCommandCount() == 2
                && sequenceValue == 0,
            "Transaction append mutated World/probe before commit");

        ok &= CheckEditorSession(
            session.CommitTransaction()
                && !session.HasActiveTransaction()
                && sequenceValue == 12
                && session.History().CommandCount() == 1
                && session.History().Cursor() == 1
                && session.History().UndoLabel()
                && std::strcmp(
                    session.History().UndoLabel(),
                    "Compound Sequence") == 0,
            "Transaction commit/compound execution order failed");

        ok &= CheckEditorSession(
            session.History().Undo(context)
                && sequenceValue == 0
                && session.History().Redo(context)
                && sequenceValue == 12,
            "Compound reverse undo/forward redo order failed");

        session.History().Clear();
        sequenceValue = 0;

        ok &= CheckEditorSession(
            session.BeginTransaction(
                "Cancelled Transaction")
                && session.AppendTransactionCommand(
                    std::make_unique<SequenceProbeCommand>(
                        sequenceValue,
                        7)),
            "Transaction cancel setup failed");

        session.CancelTransaction();

        ok &= CheckEditorSession(
            !session.HasActiveTransaction()
                && sequenceValue == 0
                && session.History().CommandCount() == 0,
            "Transaction cancel executed or retained pending commands");

        ok &= CheckEditorSession(
            session.BeginTransaction(
                "Rollback Transaction")
                && session.AppendTransactionCommand(
                    std::make_unique<SequenceProbeCommand>(
                        sequenceValue,
                        4))
                && session.AppendTransactionCommand(
                    std::make_unique<SequenceProbeCommand>(
                        sequenceValue,
                        9,
                        true))
                && !session.CommitTransaction()
                && !session.HasActiveTransaction()
                && sequenceValue == 0
                && session.History().CommandCount() == 0
                && session.History().Cursor() == 0,
            "Compound execute failure did not rollback atomically");

        session.History().Clear();
    }

    {
        int probeValue = 0;
        int destructionCount = 0;

        nocturne::editor::EditorCommandHistory history;
        history.Configure(2, 1024);

        ok &= CheckEditorSession(
            !history.CanUndo()
                && !history.CanRedo()
                && !history.Undo(context)
                && !history.Redo(context)
                && history.CommandCount() == 0
                && history.Cursor() == 0,
            "Empty history undo/redo contract failed");

        ok &= CheckEditorSession(
            history.Execute(
                context,
                std::make_unique<HistoryProbeCommand>(
                    probeValue,
                    1,
                    64,
                    destructionCount))
                && history.Execute(
                    context,
                    std::make_unique<HistoryProbeCommand>(
                        probeValue,
                        2,
                        64,
                        destructionCount))
                && history.Execute(
                    context,
                    std::make_unique<HistoryProbeCommand>(
                        probeValue,
                        4,
                        64,
                        destructionCount))
                && probeValue == 7
                && history.CommandCount() == 2
                && history.Cursor() == 2
                && destructionCount == 1,
            "History count-budget eviction failed");

        ok &= CheckEditorSession(
            history.Undo(context)
                && probeValue == 3
                && history.Undo(context)
                && probeValue == 1
                && !history.CanUndo()
                && history.Redo(context)
                && probeValue == 3
                && history.Redo(context)
                && probeValue == 7,
            "History eviction baseline undo/redo semantics failed");

        history.Clear();

        ok &= CheckEditorSession(
            history.CommandCount() == 0
                && history.Cursor() == 0
                && history.UsedBytes() == 0
                && destructionCount == 3,
            "History clear/destruction accounting failed");

        history.Configure(10, 128);

        ok &= CheckEditorSession(
            history.Execute(
                context,
                std::make_unique<HistoryProbeCommand>(
                    probeValue,
                    8,
                    64,
                    destructionCount))
                && history.Execute(
                    context,
                    std::make_unique<HistoryProbeCommand>(
                        probeValue,
                        16,
                        64,
                        destructionCount))
                && history.Execute(
                    context,
                    std::make_unique<HistoryProbeCommand>(
                        probeValue,
                        32,
                        64,
                        destructionCount))
                && history.CommandCount() == 2
                && history.Cursor() == 2
                && history.UsedBytes() == 128,
            "History byte-budget eviction failed");

        history.Clear();

        const int baselineAfterBudgetTests =
            probeValue;

        history.Configure(8, 1024);

        ok &= CheckEditorSession(
            history.Execute(
                context,
                std::make_unique<HistoryProbeCommand>(
                    probeValue,
                    5,
                    64,
                    destructionCount,
                    false,
                    true,
                    false))
                && probeValue
                    == baselineAfterBudgetTests + 5,
            "Failed-undo history setup failed");

        const uint32_t cursorBeforeFailedUndo =
            history.Cursor();

        ok &= CheckEditorSession(
            !history.Undo(context)
                && history.Cursor()
                    == cursorBeforeFailedUndo
                && probeValue
                    == baselineAfterBudgetTests + 5,
            "Failed undo moved history cursor or runtime state");

        history.Clear();

        auto failRedo =
            std::make_unique<HistoryProbeCommand>(
                probeValue,
                7,
                64,
                destructionCount,
                false,
                false,
                true);

        ok &= CheckEditorSession(
            history.Execute(
                context,
                std::move(failRedo))
                && history.Undo(context),
            "Failed-redo history setup failed");

        const uint32_t cursorBeforeFailedRedo =
            history.Cursor();
        const int valueBeforeFailedRedo =
            probeValue;

        ok &= CheckEditorSession(
            !history.Redo(context)
                && history.Cursor()
                    == cursorBeforeFailedRedo
                && probeValue
                    == valueBeforeFailedRedo,
            "Failed redo moved history cursor or runtime state");

        history.Clear();

        const uint32_t countBeforeFailedExecute =
            history.CommandCount();
        const int valueBeforeFailedExecute =
            probeValue;

        ok &= CheckEditorSession(
            !history.Execute(
                context,
                std::make_unique<HistoryProbeCommand>(
                    probeValue,
                    99,
                    64,
                    destructionCount,
                    true,
                    false,
                    false))
                && history.CommandCount()
                    == countBeforeFailedExecute
                && history.Cursor() == 0
                && probeValue == valueBeforeFailedExecute,
            "Failed execute entered history or changed runtime state");
    }

    {
        nocturne::editor::EditorInspectorModel inspectorModel;

        ok &= CheckEditorSession(
            inspectorModel.Refresh(
                context,
                authored),
            "Generic Inspector model refresh failed");

        const auto* nameValue =
            inspectorModel.FindProperty(
                noc::TypeId{
                    noc::kNameComponentTypeId.value },
                noc::MakePropertyId(
                    "Nocturne.Name.value"));
        const auto* translation =
            inspectorModel.FindProperty(
                noc::TypeId{
                    noc::kTransformComponentTypeId.value },
                noc::MakePropertyId(
                    "Nocturne.Transform.localTranslation"));

        ok &= CheckEditorSession(
            nameValue
                && nameValue->editable
                && nameValue->displayValue
                    == "Inspector Entity"
                && translation
                && translation->valueTypeId
                    == noc::BuiltinTypeIds::Vec3,
            "Inspector model did not enumerate reflected properties");

        noc::OwnedReflectedValue reflectedTranslation;
        ok &= CheckEditorSession(
            inspectorModel.ReadValue(
                context,
                authored,
                noc::TypeId{
                    noc::kTransformComponentTypeId.value },
                noc::MakePropertyId(
                    "Nocturne.Transform.localTranslation"),
                reflectedTranslation)
                && reflectedTranslation.Type()
                    == noc::BuiltinTypeIds::Vec3
                && reflectedTranslation.Data()
                && static_cast<const noc::Vec3*>(
                    reflectedTranslation.Data())->x == 0.0f
                && static_cast<const noc::Vec3*>(
                    reflectedTranslation.Data())->y == 0.0f
                && static_cast<const noc::Vec3*>(
                    reflectedTranslation.Data())->z == 0.0f,
            "Inspector generic reflected value read failed");

        noc::OwnedReflectedValue reflectedRotation;
        ok &= CheckEditorSession(
            inspectorModel.ReadValue(
                context,
                authored,
                noc::TypeId{
                    noc::kTransformComponentTypeId.value },
                noc::MakePropertyId(
                    "Nocturne.Transform.localRotation"),
                reflectedRotation)
                && reflectedRotation.Type()
                    == noc::BuiltinTypeIds::Quat
                && reflectedRotation.Data()
                && static_cast<const noc::Quat*>(
                    reflectedRotation.Data())->w == 1.0f,
            "Inspector reflected quaternion read failed");

        {
            const noc::PropertyId translationXPath[] = {
                noc::MakePropertyId(
                    "Nocturne.Vec3.x")
            };

            noc::OwnedReflectedValue translationX;
            ok &= CheckEditorSession(
                inspectorModel.ReadNestedValue(
                    context,
                    authored,
                    noc::TypeId{
                        noc::kTransformComponentTypeId.value },
                    noc::MakePropertyId(
                        "Nocturne.Transform.localTranslation"),
                    translationXPath,
                    1,
                    translationX)
                    && translationX.Type()
                        == noc::BuiltinTypeIds::Float32
                    && translationX.Data()
                    && *static_cast<const float*>(
                        translationX.Data()) == 0.0f,
                "Inspector nested Vec3 leaf read failed");

            session.History().Clear();

            ok &= CheckEditorSession(
                inspectorModel.CommitNestedTextEdit(
                    context,
                    session.History(),
                    authored,
                    noc::TypeId{
                        noc::kTransformComponentTypeId.value },
                    noc::MakePropertyId(
                        "Nocturne.Transform.localTranslation"),
                    translationXPath,
                    1,
                    "4.25")
                    && world.GetTransform(authored)
                    && world.GetTransform(authored)
                        ->localTranslation.x == 4.25f
                    && world.GetTransform(authored)
                        ->localTranslation.y == 0.0f
                    && world.GetTransform(authored)
                        ->localTranslation.z == 0.0f,
                "Inspector nested Vec3 semantic edit failed");

            ok &= CheckEditorSession(
                session.History().Undo(context)
                    && world.GetTransform(authored)
                    && world.GetTransform(authored)
                        ->localTranslation.x == 0.0f,
                "Inspector nested Vec3 edit undo failed");

            session.History().Clear();

            const noc::AABB initialBounds{
                noc::Vec3{ -1.0f, -2.0f, -3.0f },
                noc::Vec3{ 1.0f, 2.0f, 3.0f }
            };

            ok &= CheckEditorSession(
                world.AddRenderable(
                    authored,
                    noc::ResourceHandle{},
                    initialBounds)
                    && inspectorModel.Refresh(
                        context,
                        authored),
                "Inspector nested AABB setup failed");

            const noc::PropertyId minXPath[] = {
                noc::MakePropertyId(
                    "Nocturne.AABB.min"),
                noc::MakePropertyId(
                    "Nocturne.Vec3.x")
            };

            ok &= CheckEditorSession(
                inspectorModel.CommitNestedTextEdit(
                    context,
                    session.History(),
                    authored,
                    noc::TypeId{
                        noc::kRenderableComponentTypeId.value },
                    noc::MakePropertyId(
                        "Nocturne.Renderable.localBounds"),
                    minXPath,
                    2,
                    "-9.5")
                    && world.GetRenderable(authored)
                    && world.GetRenderable(authored)
                        ->localBounds.min.x == -9.5f
                    && world.GetRenderable(authored)
                        ->localBounds.min.y == -2.0f
                    && world.GetRenderable(authored)
                        ->localBounds.max.z == 3.0f,
                "Inspector nested AABB leaf edit failed");

            ok &= CheckEditorSession(
                session.History().Undo(context)
                    && world.GetRenderable(authored)
                    && world.GetRenderable(authored)
                        ->localBounds.min.x == -1.0f
                    && world.RemoveRenderable(authored)
                    && inspectorModel.Refresh(
                        context,
                        authored),
                "Inspector nested AABB edit undo/cleanup failed");

            session.History().Clear();
        }

        ok &= CheckEditorSession(
            world.AddCamera(authored)
                && inspectorModel.Refresh(
                    context,
                    authored),
            "Inspector camera presentation setup failed");

        const auto* fovProperty =
            inspectorModel.FindProperty(
                noc::TypeId{
                    noc::kCameraComponentTypeId.value },
                noc::MakePropertyId(
                    "Nocturne.Camera.fovYRadians"));

        ok &= CheckEditorSession(
            fovProperty
                && fovProperty->editable
                && fovProperty->displayAngleDegrees,
            "Inspector did not preserve reflected radian-angle presentation metadata");

        ok &= CheckEditorSession(
            world.RemoveCamera(authored)
                && inspectorModel.Refresh(
                    context,
                    authored),
            "Inspector camera presentation cleanup failed");

        session.History().Clear();

        ok &= CheckEditorSession(
            inspectorModel.CommitTextEdit(
                context,
                session.History(),
                authored,
                noc::TypeId{
                    noc::kNameComponentTypeId.value },
                noc::MakePropertyId(
                    "Nocturne.Name.value"),
                "Inspector Renamed")
                && world.GetName(authored)
                && std::strcmp(
                    world.GetName(authored)->value,
                    "Inspector Renamed") == 0,
            "Generic Inspector string edit failed");

        ok &= CheckEditorSession(
            session.History().Undo(context)
                && std::strcmp(
                    world.GetName(authored)->value,
                    "Inspector Entity") == 0,
            "Generic Inspector string undo failed");

        session.History().Clear();

        ok &= CheckEditorSession(
            inspectorModel.CommitTextEdit(
                context,
                session.History(),
                authored,
                noc::TypeId{
                    noc::kTransformComponentTypeId.value },
                noc::MakePropertyId(
                    "Nocturne.Transform.localTranslation"),
                "5, 6, 7")
                && world.GetTransform(authored)
                && world.GetTransform(authored)
                    ->localTranslation.x == 5.0f
                && world.GetTransform(authored)
                    ->localTranslation.y == 6.0f
                && world.GetTransform(authored)
                    ->localTranslation.z == 7.0f,
            "Generic Inspector Vec3 edit failed");

        ok &= CheckEditorSession(
            session.History().Undo(context),
            "Generic Inspector Vec3 undo failed");

        session.History().Clear();

        const noc::TypeMetadata* enumDrawerComponentType =
            reflection.FindType(
                kEnumDrawerComponentTypeId);
        const noc::PropertyId enumModePropertyId =
            noc::MakePropertyId(
                "Nocturne.Tests.EnumDrawerComponent.mode");

        ok &= CheckEditorSession(
            enumDrawerComponentType
                && enumDrawerComponentType->componentMetadata
                && enumDrawerComponentType->componentMetadata->add(
                    world,
                    authored)
                && inspectorModel.Refresh(
                    context,
                    authored),
            "Generic enum Inspector setup failed");

        const auto* enumModeProperty =
            inspectorModel.FindProperty(
                kEnumDrawerComponentTypeId,
                enumModePropertyId);

        bool requiredComponentRemovable = true;
        for (const auto& component :
             inspectorModel.Components())
        {
            if (component.typeId
                == kEnumDrawerComponentTypeId)
            {
                requiredComponentRemovable =
                    component.removable;
                break;
            }
        }

        auto requiredRemoval =
            std::make_unique<
                nocturne::editor::RemoveComponentCommand>();

        ok &= CheckEditorSession(
            enumModeProperty
                && enumModeProperty->editable
                && enumModeProperty->valueKind
                    == noc::TypeKind::Enum
                && enumModeProperty->valueTypeId
                    == kEnumDrawerModeTypeId
                && enumModeProperty->displayValue
                    == "Translate"
                && !requiredComponentRemovable
                && !requiredRemoval->Init(
                    context,
                    authored,
                    kEnumDrawerComponentTypeId),
            "Generic enum/required-component Inspector policy failed");

        session.History().Clear();

        ok &= CheckEditorSession(
            inspectorModel.CommitTextEdit(
                context,
                session.History(),
                authored,
                kEnumDrawerComponentTypeId,
                enumModePropertyId,
                "Rotate")
                && gEnumDrawerStore.has
                && gEnumDrawerStore.component.mode
                    == EnumDrawerTestMode::Rotate
                && session.History().CommandCount() == 1
                && session.History().Cursor() == 1,
            "Generic enum Inspector edit did not route through reflected command history");

        ok &= CheckEditorSession(
            inspectorModel.Refresh(
                context,
                authored)
                && inspectorModel.FindProperty(
                    kEnumDrawerComponentTypeId,
                    enumModePropertyId)
                && inspectorModel.FindProperty(
                    kEnumDrawerComponentTypeId,
                    enumModePropertyId)->displayValue
                    == "Rotate",
            "Generic enum Inspector refresh did not format canonical enum value");

        ok &= CheckEditorSession(
            session.History().Undo(context)
                && gEnumDrawerStore.component.mode
                    == EnumDrawerTestMode::Translate,
            "Generic enum Inspector undo failed");

        ok &= CheckEditorSession(
            session.History().Redo(context)
                && gEnumDrawerStore.component.mode
                    == EnumDrawerTestMode::Rotate,
            "Generic enum Inspector redo failed");

        session.History().Clear();

        ok &= CheckEditorSession(
            !inspectorModel.CommitTextEdit(
                context,
                session.History(),
                authored,
                kEnumDrawerComponentTypeId,
                enumModePropertyId,
                "MissingEnumValue")
                && gEnumDrawerStore.component.mode
                    == EnumDrawerTestMode::Rotate
                && session.History().CommandCount() == 0
                && session.History().Cursor() == 0,
            "Invalid generic enum Inspector value mutated state or entered history");

        ok &= CheckEditorSession(
            enumDrawerComponentType->componentMetadata->remove(
                world,
                authored)
                && inspectorModel.Refresh(
                    context,
                    authored)
                && !gEnumDrawerStore.has,
            "Generic enum Inspector cleanup failed");

        session.History().Clear();
    }

    const noc::Vec3 first{ 1.0f, 2.0f, 3.0f };
    auto firstCommand =
        MakeTranslationCommand(context, authored, first);

    ok &= CheckEditorSession(
        firstCommand
            && session.History().Execute(
                context,
                std::move(firstCommand)),
        "Generic property command execute failed");

    ok &= CheckEditorSession(
        world.GetTransform(authored)
            && world.GetTransform(authored)->localTranslation.x == 1.0f
            && session.History().CanUndo()
            && !session.History().CanRedo(),
        "Property command did not persist/history");

    ok &= CheckEditorSession(
        session.History().Undo(context)
            && world.GetTransform(authored)
            && world.GetTransform(authored)->localTranslation.x == 0.0f
            && session.History().CanRedo(),
        "Property-command undo failed");

    ok &= CheckEditorSession(
        session.History().Redo(context)
            && world.GetTransform(authored)
            && world.GetTransform(authored)->localTranslation.x == 1.0f,
        "Property-command redo failed");

    ok &= CheckEditorSession(
        session.History().Undo(context),
        "Second undo failed");

    const noc::Vec3 second{ 9.0f, 0.0f, 0.0f };
    auto secondCommand =
        MakeTranslationCommand(context, authored, second);

    ok &= CheckEditorSession(
        secondCommand
            && session.History().Execute(
                context,
                std::move(secondCommand))
            && !session.History().CanRedo()
            && world.GetTransform(authored)->localTranslation.x == 9.0f,
        "New command after undo did not invalidate redo tail");

    {
        auto create =
            std::make_unique<
                nocturne::editor::CreateEntityCommand>();

        ok &= CheckEditorSession(
            create->Init("Created Entity"),
            "CreateEntityCommand init failed");

        auto* createRaw = create.get();

        ok &= CheckEditorSession(
            session.History().Execute(
                context,
                std::move(create)),
            "CreateEntityCommand execute failed");

        const noc::EntityHandle firstCreated =
            createRaw->CurrentEntity();

        ok &= CheckEditorSession(
            firstCreated.IsValid()
                && world.IsAlive(firstCreated)
                && world.HasName(firstCreated)
                && world.HasTransform(firstCreated)
                && world.GetName(firstCreated)
                && std::strcmp(
                    world.GetName(firstCreated)->value,
                    "Created Entity") == 0,
            "Created entity default component policy mismatch");

        ok &= CheckEditorSession(
            session.History().Undo(context)
                && !world.IsAlive(firstCreated)
                && !createRaw->CurrentEntity().IsValid(),
            "CreateEntityCommand undo failed");

        ok &= CheckEditorSession(
            session.History().Redo(context),
            "CreateEntityCommand redo failed");

        const noc::EntityHandle recreated =
            createRaw->CurrentEntity();

        ok &= CheckEditorSession(
            recreated.IsValid()
                && world.IsAlive(recreated)
                && recreated != firstCreated,
            "CreateEntityCommand redo reused stale runtime identity");

        auto rename =
            std::make_unique<
                nocturne::editor::RenameEntityCommand>();

        ok &= CheckEditorSession(
            rename->Init(
                context,
                recreated,
                "Renamed Entity"),
            "RenameEntityCommand init failed");

        ok &= CheckEditorSession(
            session.History().Execute(
                context,
                std::move(rename))
                && std::strcmp(
                    world.GetName(recreated)->value,
                    "Renamed Entity") == 0,
            "RenameEntityCommand execute failed");

        ok &= CheckEditorSession(
            session.History().Undo(context)
                && std::strcmp(
                    world.GetName(recreated)->value,
                    "Created Entity") == 0,
            "RenameEntityCommand undo failed");

        ok &= CheckEditorSession(
            session.History().Redo(context)
                && std::strcmp(
                    world.GetName(recreated)->value,
                    "Renamed Entity") == 0,
            "RenameEntityCommand redo failed");

        char tooLong[noc::kNameComponentCapacity + 1u]{};
        for (uint32_t i = 0;
             i < noc::kNameComponentCapacity;
             ++i)
        {
            tooLong[i] = 'X';
        }
        tooLong[noc::kNameComponentCapacity] = '\0';

        auto invalidRename =
            std::make_unique<
                nocturne::editor::RenameEntityCommand>();

        ok &= CheckEditorSession(
            !invalidRename->Init(
                context,
                recreated,
                tooLong),
            "Rename command accepted over-limit UTF-8 payload");

        // Clear history before deleting the command-created entity because
        // older create/rename commands intentionally validate stale handles.
        session.History().Clear();
        ok &= CheckEditorSession(
            world.DestroyEntity(recreated),
            "Command-created entity cleanup failed");

        auto createChild =
            std::make_unique<
                nocturne::editor::CreateEntityCommand>();

        ok &= CheckEditorSession(
            createChild->Init(
                "Created Child",
                authored),
            "Parented CreateEntityCommand init failed");

        auto* createChildRaw =
            createChild.get();

        ok &= CheckEditorSession(
            session.History().Execute(
                context,
                std::move(createChild)),
            "Parented CreateEntityCommand execute failed");

        const noc::EntityHandle firstChild =
            createChildRaw->CurrentEntity();

        ok &= CheckEditorSession(
            firstChild.IsValid()
                && world.IsAlive(firstChild)
                && world.ParentOf(firstChild) == authored
                && world.GetName(firstChild)
                && std::strcmp(
                    world.GetName(firstChild)->value,
                    "Created Child") == 0,
            "Create-under-parent hierarchy mismatch");

        ok &= CheckEditorSession(
            session.History().Undo(context)
                && !world.IsAlive(firstChild)
                && !createChildRaw->CurrentEntity().IsValid(),
            "Create-under-parent undo failed");

        ok &= CheckEditorSession(
            session.History().Redo(context),
            "Create-under-parent redo failed");

        const noc::EntityHandle recreatedChild =
            createChildRaw->CurrentEntity();

        ok &= CheckEditorSession(
            recreatedChild.IsValid()
                && recreatedChild != firstChild
                && world.IsAlive(recreatedChild)
                && world.ParentOf(recreatedChild) == authored,
            "Create-under-parent redo did not restore parentage with new runtime identity");

        session.History().Clear();
        ok &= CheckEditorSession(
            world.DestroyEntity(recreatedChild),
            "Create-under-parent cleanup failed");
    }

    {
        session.History().Clear();

        const noc::TransformComponent* beforeAtomic =
            world.GetTransform(authored);
        const noc::Vec3 oldT =
            beforeAtomic->localTranslation;
        const noc::Quat oldR =
            beforeAtomic->localRotation;
        const noc::Vec3 oldS =
            beforeAtomic->localScale;

        const noc::Vec3 newT{ 4.0f, -2.0f, 8.0f };
        const noc::Quat newR{
            0.0f,
            std::sin(0.25f),
            0.0f,
            std::cos(0.25f)
        };
        const noc::Vec3 newS{ 1.25f, 0.75f, 1.5f };

        auto transformCommand =
            std::make_unique<
                nocturne::editor::SetTransformTRSCommand>();

        ok &= CheckEditorSession(
            transformCommand->InitExplicit(
                context,
                authored,
                oldT,
                oldR,
                oldS,
                newT,
                newR,
                newS)
                && session.History().Execute(
                    context,
                    std::move(transformCommand)),
            "Atomic transform command execute failed");

        const noc::TransformComponent* transformed =
            world.GetTransform(authored);

        ok &= CheckEditorSession(
            transformed
                && transformed->localTranslation.x == newT.x
                && transformed->localTranslation.y == newT.y
                && transformed->localTranslation.z == newT.z
                && transformed->localRotation.y == newR.y
                && transformed->localRotation.w == newR.w
                && transformed->localScale.x == newS.x
                && transformed->localScale.y == newS.y
                && transformed->localScale.z == newS.z
                && session.History().CommandCount() == 1,
            "Atomic transform command did not apply full local TRS");

        ok &= CheckEditorSession(
            session.History().Undo(context),
            "Atomic transform command undo failed");

        const noc::TransformComponent* restored =
            world.GetTransform(authored);

        ok &= CheckEditorSession(
            restored
                && restored->localTranslation.x == oldT.x
                && restored->localTranslation.y == oldT.y
                && restored->localTranslation.z == oldT.z
                && restored->localRotation.x == oldR.x
                && restored->localRotation.y == oldR.y
                && restored->localRotation.z == oldR.z
                && restored->localRotation.w == oldR.w
                && restored->localScale.x == oldS.x
                && restored->localScale.y == oldS.y
                && restored->localScale.z == oldS.z,
            "Atomic transform command undo did not restore full local TRS");

        ok &= CheckEditorSession(
            session.History().Redo(context),
            "Atomic transform command redo failed");

        const noc::TransformComponent* redone =
            world.GetTransform(authored);

        ok &= CheckEditorSession(
            redone
                && redone->localTranslation.x == newT.x
                && redone->localRotation.y == newR.y
                && redone->localScale.z == newS.z,
            "Atomic transform command redo did not restore final TRS");

        session.History().Clear();
    }

    {
        session.History().Clear();

        const noc::TransformComponent* before =
            world.GetTransform(authored);
        const noc::Vec3 liveOld =
            before ? before->localTranslation : noc::Vec3::Zero();
        const noc::Vec3 liveNew{ 14.0f, 3.0f, -2.0f };

        ok &= CheckEditorSession(
            world.SetLocalTRS(
                authored,
                liveNew,
                before->localRotation,
                before->localScale),
            "Live transaction setup mutation failed");

        auto liveCommand =
            std::make_unique<
                nocturne::editor::SetReflectedPropertyCommand>();

        ok &= CheckEditorSession(
            liveCommand->InitExplicit(
                context,
                authored,
                noc::TypeId{
                    noc::kTransformComponentTypeId.value },
                noc::MakePropertyId(
                    "Nocturne.Transform.localTranslation"),
                noc::ReflectedConstValueView{
                    noc::BuiltinTypeIds::Vec3,
                    &liveOld },
                noc::ReflectedConstValueView{
                    noc::BuiltinTypeIds::Vec3,
                    &liveNew }),
            "Explicit live property command init failed");

        ok &= CheckEditorSession(
            session.History().RecordExecuted(
                context,
                std::move(liveCommand))
                && session.History().CommandCount() == 1
                && world.GetTransform(authored)
                && world.GetTransform(authored)->localTranslation.x
                    == liveNew.x,
            "Live transaction was not recorded as one history entry");

        ok &= CheckEditorSession(
            session.History().Undo(context)
                && world.GetTransform(authored)
                && world.GetTransform(authored)->localTranslation.x
                    == liveOld.x,
            "Recorded live transaction undo failed");

        ok &= CheckEditorSession(
            session.History().Redo(context)
                && world.GetTransform(authored)
                && world.GetTransform(authored)->localTranslation.x
                    == liveNew.x,
            "Recorded live transaction redo failed");
    }

    {
        session.History().Clear();

        auto addCamera =
            std::make_unique<
                nocturne::editor::AddComponentCommand>();

        ok &= CheckEditorSession(
            addCamera->Init(
                context,
                authored,
                noc::TypeId{
                    noc::kCameraComponentTypeId.value }),
            "AddComponentCommand(Camera) init failed");

        ok &= CheckEditorSession(
            session.History().Execute(
                context,
                std::move(addCamera))
                && world.HasCamera(authored),
            "Generic AddComponentCommand execute failed");

        ok &= CheckEditorSession(
            session.History().Undo(context)
                && !world.HasCamera(authored),
            "Generic AddComponentCommand undo failed");

        ok &= CheckEditorSession(
            session.History().Redo(context)
                && world.HasCamera(authored),
            "Generic AddComponentCommand redo failed");

        ok &= CheckEditorSession(
            world.SetCameraPerspective(
                authored,
                0.9f,
                1.5f,
                1000.0f,
                2000.0f),
            "Camera setup for reflected component snapshot failed");

        session.History().Clear();

        auto removeCamera =
            std::make_unique<
                nocturne::editor::RemoveComponentCommand>();

        ok &= CheckEditorSession(
            removeCamera->Init(
                context,
                authored,
                noc::TypeId{
                    noc::kCameraComponentTypeId.value }),
            "RemoveComponentCommand(Camera) init failed");

        ok &= CheckEditorSession(
            session.History().Execute(
                context,
                std::move(removeCamera))
                && !world.HasCamera(authored),
            "Generic RemoveComponentCommand execute failed");

        ok &= CheckEditorSession(
            session.History().Undo(context)
                && world.HasCamera(authored)
                && world.GetCamera(authored)
                && world.GetCamera(authored)->fovYRadians == 0.9f
                && world.GetCamera(authored)->aspect == 1.5f
                && world.GetCamera(authored)->nearZ == 1000.0f
                && world.GetCamera(authored)->farZ == 2000.0f,
            "Reflected component snapshot restore failed");

        ok &= CheckEditorSession(
            session.History().Redo(context)
                && !world.HasCamera(authored),
            "Generic RemoveComponentCommand redo failed");

        session.History().Clear();
    }

    {
        session.History().Clear();

        const noc::EntityHandle parentEntity =
            world.CreateEntity();
        const noc::EntityHandle childEntity =
            world.CreateEntity();

        ok &= CheckEditorSession(
            parentEntity.IsValid()
                && childEntity.IsValid()
                && world.AddName(parentEntity, "Delete Parent")
                && world.AddTransform(parentEntity)
                && world.AddName(childEntity, "Delete Child")
                && world.AddTransform(childEntity)
                && world.SetLocalTRS(
                    childEntity,
                    noc::Vec3{ 3.0f, 0.0f, 0.0f },
                    noc::Quat::Identity(),
                    noc::Vec3::One())
                && world.SetParent(
                    childEntity,
                    parentEntity),
            "Delete/duplicate subtree setup failed");

        auto deleteCommand =
            std::make_unique<
                nocturne::editor::DeleteEntityCommand>();
        auto* deleteRaw = deleteCommand.get();

        ok &= CheckEditorSession(
            deleteCommand->Init(
                context,
                parentEntity),
            "DeleteEntityCommand snapshot capture failed");

        ok &= CheckEditorSession(
            session.History().Execute(
                context,
                std::move(deleteCommand))
                && !world.IsAlive(parentEntity)
                && !world.IsAlive(childEntity),
            "DeleteEntityCommand did not delete subtree");

        ok &= CheckEditorSession(
            session.History().Undo(context),
            "DeleteEntityCommand undo failed");

        const noc::EntityHandle restoredParent =
            deleteRaw->CurrentRoot();
        const noc::EntityHandle restoredChild =
            world.FirstChildOf(restoredParent);

        ok &= CheckEditorSession(
            restoredParent.IsValid()
                && restoredChild.IsValid()
                && world.IsAlive(restoredParent)
                && world.IsAlive(restoredChild)
                && restoredParent != parentEntity
                && restoredChild != childEntity
                && world.GetName(restoredParent)
                && world.GetName(restoredChild)
                && std::strcmp(
                    world.GetName(restoredParent)->value,
                    "Delete Parent") == 0
                && std::strcmp(
                    world.GetName(restoredChild)->value,
                    "Delete Child") == 0
                && world.ParentOf(restoredChild)
                    == restoredParent
                && world.GetTransform(restoredChild)
                && world.GetTransform(restoredChild)
                    ->localTranslation.x == 3.0f,
            "Delete undo did not restore reflected subtree state");

        ok &= CheckEditorSession(
            session.History().Redo(context)
                && !world.IsAlive(restoredParent)
                && !world.IsAlive(restoredChild),
            "DeleteEntityCommand redo failed");

        ok &= CheckEditorSession(
            session.History().Undo(context),
            "DeleteEntityCommand second undo failed");

        const noc::EntityHandle sourceRoot =
            deleteRaw->CurrentRoot();
        const noc::EntityHandle sourceChild =
            world.FirstChildOf(sourceRoot);

        session.History().Clear();

        auto duplicateCommand =
            std::make_unique<
                nocturne::editor::DuplicateEntityCommand>();
        auto* duplicateRaw =
            duplicateCommand.get();

        ok &= CheckEditorSession(
            duplicateCommand->Init(
                context,
                sourceRoot),
            "DuplicateEntityCommand snapshot capture failed");

        ok &= CheckEditorSession(
            session.History().Execute(
                context,
                std::move(duplicateCommand)),
            "DuplicateEntityCommand execute failed");

        const noc::EntityHandle duplicateRoot =
            duplicateRaw->CurrentRoot();
        const noc::EntityHandle duplicateChild =
            world.FirstChildOf(duplicateRoot);

        ok &= CheckEditorSession(
            duplicateRoot.IsValid()
                && duplicateChild.IsValid()
                && duplicateRoot != sourceRoot
                && duplicateChild != sourceChild
                && world.IsAlive(sourceRoot)
                && world.IsAlive(sourceChild)
                && std::strcmp(
                    world.GetName(duplicateRoot)->value,
                    "Delete Parent") == 0
                && std::strcmp(
                    world.GetName(duplicateChild)->value,
                    "Delete Child") == 0
                && world.ParentOf(duplicateChild)
                    == duplicateRoot,
            "Duplicate subtree state mismatch");

        ok &= CheckEditorSession(
            session.History().Undo(context)
                && !world.IsAlive(duplicateRoot)
                && world.IsAlive(sourceRoot),
            "DuplicateEntityCommand undo failed");

        ok &= CheckEditorSession(
            session.History().Redo(context),
            "DuplicateEntityCommand redo failed");

        const noc::EntityHandle duplicateRoot2 =
            duplicateRaw->CurrentRoot();
        const noc::EntityHandle duplicateChild2 =
            world.FirstChildOf(duplicateRoot2);

        ok &= CheckEditorSession(
            duplicateRoot2.IsValid()
                && duplicateRoot2 != duplicateRoot
                && duplicateChild2.IsValid(),
            "Duplicate redo reused stale runtime identity");

        session.History().Clear();

        ok &= CheckEditorSession(
            world.DestroyEntity(duplicateChild2)
                && world.DestroyEntity(duplicateRoot2)
                && world.DestroyEntity(sourceChild)
                && world.DestroyEntity(sourceRoot),
            "Delete/duplicate test cleanup failed");

        auto toolDelete =
            std::make_unique<
                nocturne::editor::DeleteEntityCommand>();
        ok &= CheckEditorSession(
            !toolDelete->Init(context, camera),
            "Tool-owned editor camera accepted delete command");
    }

    {
        session.History().Clear();

        const noc::EntityHandle oldParent =
            world.CreateEntity();
        const noc::EntityHandle newParent =
            world.CreateEntity();
        const noc::EntityHandle reparentChild =
            world.CreateEntity();

        const float halfAngle = 0.25f * 3.14159265358979323846f;
        const noc::Quat z90{
            0.0f,
            0.0f,
            std::sin(halfAngle),
            std::cos(halfAngle)
        };

        ok &= CheckEditorSession(
            oldParent.IsValid()
                && newParent.IsValid()
                && reparentChild.IsValid()
                && world.AddTransform(oldParent)
                && world.AddTransform(newParent)
                && world.AddTransform(reparentChild)
                && world.SetLocalTRS(
                    oldParent,
                    noc::Vec3{ 10.0f, 0.0f, 0.0f },
                    noc::Quat::Identity(),
                    noc::Vec3::One())
                && world.SetLocalTRS(
                    newParent,
                    noc::Vec3{ -5.0f, 2.0f, 0.0f },
                    noc::Quat::Identity(),
                    noc::Vec3{ 2.0f, 2.0f, 2.0f })
                && world.SetLocalTRS(
                    reparentChild,
                    noc::Vec3{ 2.0f, 1.0f, 0.0f },
                    z90,
                    noc::Vec3::One())
                && world.SetParent(
                    reparentChild,
                    oldParent),
            "Reparent test setup failed");

        world.Update();
        const noc::Mat4 worldBefore =
            world.GetWorldMatrix(reparentChild);

        auto reparent =
            std::make_unique<
                nocturne::editor::ReparentEntityCommand>();

        ok &= CheckEditorSession(
            reparent->Init(
                context,
                reparentChild,
                newParent),
            "Preserve-world reparent init failed");

        ok &= CheckEditorSession(
            session.History().Execute(
                context,
                std::move(reparent))
                && world.ParentOf(reparentChild)
                    == newParent,
            "Preserve-world reparent execute failed");

        const noc::Mat4 worldAfter =
            world.GetWorldMatrix(reparentChild);

        ok &= CheckEditorSession(
            std::fabs(
                worldAfter.m[12]
                    - worldBefore.m[12]) < 1.0e-3f
                && std::fabs(
                    worldAfter.m[13]
                        - worldBefore.m[13]) < 1.0e-3f
                && std::fabs(
                    worldAfter.m[0]
                        - worldBefore.m[0]) < 1.0e-3f
                && std::fabs(
                    worldAfter.m[1]
                        - worldBefore.m[1]) < 1.0e-3f,
            "Reparent did not preserve world pose");

        ok &= CheckEditorSession(
            session.History().Undo(context)
                && world.ParentOf(reparentChild)
                    == oldParent,
            "Reparent undo failed");

        const noc::Mat4 worldUndo =
            world.GetWorldMatrix(reparentChild);
        ok &= CheckEditorSession(
            std::fabs(
                worldUndo.m[12]
                    - worldBefore.m[12]) < 1.0e-3f
                && std::fabs(
                    worldUndo.m[13]
                        - worldBefore.m[13]) < 1.0e-3f,
            "Reparent undo did not preserve original world pose");

        session.History().Clear();

        auto unparent =
            std::make_unique<
                nocturne::editor::ReparentEntityCommand>();

        ok &= CheckEditorSession(
            unparent->Init(
                context,
                reparentChild,
                noc::EntityHandle::Invalid())
                && session.History().Execute(
                    context,
                    std::move(unparent))
                && !world.ParentOf(
                    reparentChild).IsValid(),
            "Preserve-world unparent failed");

        const noc::Mat4 worldUnparent =
            world.GetWorldMatrix(reparentChild);
        ok &= CheckEditorSession(
            std::fabs(
                worldUnparent.m[12]
                    - worldBefore.m[12]) < 1.0e-3f
                && std::fabs(
                    worldUnparent.m[13]
                        - worldBefore.m[13]) < 1.0e-3f,
            "Unparent did not preserve world pose");

        session.History().Clear();

        // Singular target parent must be rejected before hierarchy mutation.
        ok &= CheckEditorSession(
            world.SetLocalTRS(
                newParent,
                noc::Vec3::Zero(),
                noc::Quat::Identity(),
                noc::Vec3{ 0.0f, 1.0f, 1.0f }),
            "Singular parent setup failed");

        auto singular =
            std::make_unique<
                nocturne::editor::ReparentEntityCommand>();

        ok &= CheckEditorSession(
            !singular->Init(
                context,
                reparentChild,
                newParent)
                && !world.ParentOf(
                    reparentChild).IsValid(),
            "Singular-parent reparent was not rejected atomically");

        // Non-uniform target scale + a non-axis-aligned rotated world basis
        // produces shear in target-local space and cannot be represented as
        // a pure T*R*S matrix. 90 degrees would only permute scaled axes and
        // remains representable, so use 45 degrees for the actual shear case.
        const float shearHalfAngle =
            0.125f * 3.14159265358979323846f;
        const noc::Quat z45{
            0.0f,
            0.0f,
            std::sin(shearHalfAngle),
            std::cos(shearHalfAngle)
        };

        ok &= CheckEditorSession(
            world.SetLocalTRS(
                newParent,
                noc::Vec3::Zero(),
                noc::Quat::Identity(),
                noc::Vec3{ 2.0f, 1.0f, 1.0f })
                && world.SetLocalTRS(
                    reparentChild,
                    noc::Vec3::Zero(),
                    z45,
                    noc::Vec3::One()),
            "Shear rejection setup failed");

        auto shear =
            std::make_unique<
                nocturne::editor::ReparentEntityCommand>();

        ok &= CheckEditorSession(
            !shear->Init(
                context,
                reparentChild,
                newParent)
                && !world.ParentOf(
                    reparentChild).IsValid(),
            "Non-representable shear reparent was accepted");

        ok &= CheckEditorSession(
            world.DestroyEntity(reparentChild)
                && world.DestroyEntity(newParent)
                && world.DestroyEntity(oldParent),
            "Reparent test cleanup failed");
    }

    // Failed commands must not enter history.
    const uint32_t historyCountBeforeFailure =
        session.History().CommandCount();

    auto invalidCommand =
        MakeTranslationCommand(
            context,
            noc::EntityHandle{ 999999u, 1u },
            first);

    ok &= CheckEditorSession(
        !invalidCommand
            && session.History().CommandCount()
                == historyCountBeforeFailure,
        "Failed command creation changed history");

    ok &= CheckEditorSession(
        world.DestroyEntity(authored),
        "Selection stale-test destroy failed");
    session.ValidateSelection();

    ok &= CheckEditorSession(
        !session.SelectedEntity().IsValid(),
        "Stale selected EntityHandle was not cleared");

    session.History().Clear();

    ok &= CheckEditorSession(
        session.History().CommandCount() == 0
            && !session.History().CanUndo()
            && !session.History().CanRedo(),
        "History clear failed");

    {
        const noc::EntityHandle resetParent =
            world.CreateEntity();
        const noc::EntityHandle resetChild =
            world.CreateEntity();

        ok &= CheckEditorSession(
            resetParent.IsValid()
                && resetChild.IsValid()
                && world.AddName(
                    resetParent,
                    "Reset Parent")
                && world.AddTransform(resetParent)
                && world.AddName(
                    resetChild,
                    "Reset Child")
                && world.AddTransform(resetChild)
                && world.SetParent(
                    resetChild,
                    resetParent)
                && session.SetSelection(resetChild),
            "New Scene reset setup failed");

        auto resetHistoryCommand =
            MakeTranslationCommand(
                context,
                resetChild,
                noc::Vec3{
                    8.0f,
                    1.0f,
                    -3.0f });

        ok &= CheckEditorSession(
            resetHistoryCommand
                && session.History().Execute(
                    context,
                    std::move(resetHistoryCommand)),
            "New Scene history setup failed");

        session.SetSceneDirty();

        ok &= CheckEditorSession(
            session.SceneDirty()
                && session.History().CanUndo()
                && session.ResetAuthoredScene(),
            "New Scene reset execution failed");

        ok &= CheckEditorSession(
            !world.IsAlive(resetParent)
                && !world.IsAlive(resetChild)
                && world.IsAlive(camera)
                && session.ToolCamera() == camera
                && session.IsToolOwned(camera)
                && !session.SelectedEntity().IsValid()
                && session.History().CommandCount() == 0
                && !session.History().CanUndo()
                && !session.History().CanRedo()
                && !session.SceneDirty(),
            "New Scene reset did not establish a clean tool-camera-only baseline");
    }

    ok &= CheckEditorSession(
        world.DestroyEntity(camera),
        "Tool-camera cleanup failed");

    {
        nocturne::editor::EditorSession shutdownSession;
        int pendingValue = 0;

        ok &= CheckEditorSession(
            shutdownSession.Init(
                world,
                reflection,
                allocator,
                8,
                4096)
                && shutdownSession.BeginTransaction(
                    "Shutdown Pending")
                && shutdownSession.AppendTransactionCommand(
                    std::make_unique<SequenceProbeCommand>(
                        pendingValue,
                        8)),
            "Active-transaction shutdown setup failed");

        shutdownSession.Shutdown();

        ok &= CheckEditorSession(
            !shutdownSession.IsInitialized()
                && !shutdownSession.HasActiveTransaction()
                && pendingValue == 0,
            "Shutdown did not cancel deferred active transaction");
    }

    gEnumDrawerStore = {};
    session.Shutdown();
    world.Shutdown();
    reflection.Shutdown();

    ok &= CheckEditorSession(
        allocator.OutstandingBytes() == 0,
        "Editor session/history leaked allocator memory");

    NOC_LOG_INFO(
        "Phase16Editor",
        "Editor session/history tests %s",
        ok ? "PASS" : "FAIL");

    return ok;
}
