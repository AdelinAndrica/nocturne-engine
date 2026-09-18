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
