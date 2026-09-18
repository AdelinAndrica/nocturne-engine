#include "../../NocturneEditor/EditorCommands.h"
#include "../../NocturneEditor/EditorInspectorModel.h"
#include "../../NocturneEditor/EditorSession.h"

#include "Core/Log.h"
#include "Core/Memory/Allocator.h"
#include "Core/Memory/DebugAlloc.h"
#include "Runtime/Components/TransformComponent.h"
#include "Runtime/Reflection/BuiltinTypes.h"
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

    ok &= CheckEditorSession(
        reflection.Init(allocator, 32)
            && noc::RegisterBuiltinReflectionTypes(reflection)
            && noc::RegisterFoundationComponentReflectionTypes(reflection)
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

        // Non-uniform target scale + rotated world basis produces shear in
        // target-local space and therefore cannot be represented as TRS.
        ok &= CheckEditorSession(
            world.SetLocalTRS(
                newParent,
                noc::Vec3::Zero(),
                noc::Quat::Identity(),
                noc::Vec3{ 2.0f, 1.0f, 1.0f })
                && world.SetLocalTRS(
                    reparentChild,
                    noc::Vec3::Zero(),
                    z90,
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

    ok &= CheckEditorSession(
        world.DestroyEntity(camera),
        "Tool-camera cleanup failed");

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
