#include "../../NocturneEditor/EditorCommands.h"
#include "../../NocturneEditor/EditorSession.h"

#include "Core/Log.h"
#include "Core/Memory/Allocator.h"
#include "Core/Memory/DebugAlloc.h"
#include "Runtime/Components/TransformComponent.h"
#include "Runtime/Reflection/BuiltinTypes.h"
#include "Runtime/Reflection/FoundationComponents.h"
#include "Runtime/Reflection/ReflectionRegistry.h"
#include "Runtime/World.h"

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
