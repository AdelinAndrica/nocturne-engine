#include "Runtime/ComponentRegistry.h"

#include "Core/Log.h"
#include "Core/Memory/Allocator.h"
#include "Core/Memory/DebugAlloc.h"
#include "Runtime/Components/CameraComponent.h"
#include "Runtime/Components/NameComponent.h"
#include "Runtime/Components/RenderableComponent.h"
#include "Runtime/Components/TransformComponent.h"
#include "Runtime/Reflection/BuiltinTypes.h"
#include "Runtime/Reflection/FoundationComponents.h"
#include "Runtime/Reflection/ReflectionRegistry.h"

#include <cstring>

namespace
{
    bool CheckComponentRegistry(bool condition, const char* message)
    {
        if (!condition)
        {
            NOC_LOG_ERROR("Phase15", "%s", message);
            return false;
        }
        return true;
    }
}

bool RunPhase15ComponentRegistryTests()
{
    NOC_LOG_INFO(
        "Phase15",
        "%s",
        "Component registry compatibility-facade tests begin");

    noc::MallocAllocator backing;
    noc::DebugAlloc allocator(backing);
    noc::ReflectionRegistry reflection;
    noc::ComponentRegistry registry;

    bool ok = true;

    ok &= CheckComponentRegistry(
        reflection.Init(allocator, 4),
        "ReflectionRegistry::Init failed");
    ok &= CheckComponentRegistry(
        noc::RegisterBuiltinReflectionTypes(reflection)
            && noc::RegisterFoundationComponentReflectionTypes(reflection),
        "Foundation component reflection registration failed");
    ok &= CheckComponentRegistry(
        reflection.Freeze(),
        "Foundation reflection Freeze failed");

    ok &= CheckComponentRegistry(
        registry.Init(reflection, allocator),
        "ComponentRegistry facade Init failed");
    ok &= CheckComponentRegistry(
        registry.Count() == 4,
        "ComponentRegistry facade count mismatch");

    const auto* transform =
        registry.Find(noc::kTransformComponentTypeId);
    ok &= CheckComponentRegistry(
        transform
            && transform->version == noc::kTransformComponentVersion
            && transform->size == sizeof(noc::TransformComponent)
            && transform->alignment == alignof(noc::TransformComponent),
        "Transform compatibility metadata mismatch");

    ok &= CheckComponentRegistry(
        transform
            && noc::HasFlag(
                transform->flags,
                noc::ComponentTypeFlags::EditorVisible)
            && noc::HasFlag(
                transform->flags,
                noc::ComponentTypeFlags::Serializable),
        "Transform compatibility flags mismatch");

    const auto* renderable =
        registry.FindByName(
            noc::kRenderableComponentCanonicalName);
    ok &= CheckComponentRegistry(
        renderable
            && renderable->typeId == noc::kRenderableComponentTypeId,
        "FindByName(Renderable) failed");

    ok &= CheckComponentRegistry(
        registry.MetadataAt(0)
            && registry.MetadataAt(0)->typeId
                == noc::kTransformComponentTypeId
            && registry.MetadataAt(1)
            && registry.MetadataAt(1)->typeId
                == noc::kRenderableComponentTypeId
            && registry.MetadataAt(2)
            && registry.MetadataAt(2)->typeId
                == noc::kCameraComponentTypeId
            && registry.MetadataAt(3)
            && registry.MetadataAt(3)->typeId
                == noc::kNameComponentTypeId,
        "Facade component order must mirror reflected TypeId order");

    const noc::TypeMetadata* reflectedTransform =
        reflection.ComponentTypeAt(0);

    ok &= CheckComponentRegistry(
        reflectedTransform
            && transform
            && transform->canonicalName
                == reflectedTransform->canonicalName,
        "Facade must borrow canonical names from ReflectionRegistry");

    ok &= CheckComponentRegistry(
        registry.Find(noc::ComponentTypeId{ 999 }) == nullptr
            && registry.FindByName("Nocturne.Unknown") == nullptr
            && registry.MetadataAt(4) == nullptr,
        "Facade invalid lookup policy mismatch");

    registry.Shutdown();

    ok &= CheckComponentRegistry(
        reflection.FindType(
            noc::TypeId{
                noc::kTransformComponentTypeId.value }) != nullptr,
        "Facade shutdown mutated ReflectionRegistry authority");

    reflection.Shutdown();

    ok &= CheckComponentRegistry(
        allocator.OutstandingBytes() == 0,
        "ComponentRegistry facade leaked allocator-owned memory");

    NOC_LOG_INFO(
        "Phase15",
        "Component registry facade tests %s",
        ok ? "PASS" : "FAIL");
    return ok;
}
