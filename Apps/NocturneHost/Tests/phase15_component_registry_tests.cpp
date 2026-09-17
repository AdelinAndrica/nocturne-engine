#include "Runtime/ComponentRegistry.h"

#include "Core/Log.h"
#include "Core/Memory/Allocator.h"
#include "Core/Memory/DebugAlloc.h"

#include <cstdint>
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

    struct TestTransform
    {
        float values[16]{};
    };

    struct alignas(64) TestRenderable
    {
        uint64_t payload[8]{};
    };

    struct TestName
    {
        char name[32]{};
    };
}

bool RunPhase15ComponentRegistryTests()
{
    NOC_LOG_INFO("Phase15", "%s", "Component registry tests begin");

    noc::MallocAllocator backing;
    noc::DebugAlloc allocator(backing);
    noc::ComponentRegistry registry;

    bool ok = true;
    ok &= CheckComponentRegistry(registry.Init(allocator, 1), "ComponentRegistry::Init failed");
    ok &= CheckComponentRegistry(registry.Count() == 0, "Registry must begin empty");

    constexpr noc::ComponentTypeId kTransformId{ 10 };
    constexpr noc::ComponentTypeId kRenderableId{ 30 };
    constexpr noc::ComponentTypeId kNameId{ 20 };

    char mutableName[] = "Test.Transform";

    const noc::ComponentTypeMetadata transform =
        noc::MakeComponentTypeMetadata<TestTransform>(
            kTransformId,
            mutableName,
            1,
            noc::ComponentTypeFlags::EditorVisible
                | noc::ComponentTypeFlags::Serializable);

    const noc::ComponentTypeMetadata renderable =
        noc::MakeComponentTypeMetadata<TestRenderable>(
            kRenderableId,
            "Test.Renderable",
            3,
            noc::ComponentTypeFlags::EditorVisible);

    const noc::ComponentTypeMetadata name =
        noc::MakeComponentTypeMetadata<TestName>(
            kNameId,
            "Test.Name",
            2,
            noc::ComponentTypeFlags::Serializable);

    // Register deliberately out of type-ID order; enumeration must still be
    // deterministic and sorted by explicit ID.
    ok &= CheckComponentRegistry(registry.Register(renderable), "Renderable registration failed");
    ok &= CheckComponentRegistry(registry.Register(transform), "Transform registration failed");
    ok &= CheckComponentRegistry(registry.Register(name), "Name registration failed");
    ok &= CheckComponentRegistry(registry.Count() == 3, "Registry count mismatch");

    // The registry owns canonical-name memory. Mutating the source buffer must
    // not change registered metadata.
    mutableName[0] = 'X';

    const auto* foundTransform = registry.Find(kTransformId);
    ok &= CheckComponentRegistry(foundTransform != nullptr, "Find(transform) failed");
    ok &= CheckComponentRegistry(
        foundTransform && std::strcmp(foundTransform->canonicalName, "Test.Transform") == 0,
        "Registry did not own/copy canonical name");
    ok &= CheckComponentRegistry(
        foundTransform && foundTransform->version == 1,
        "Transform version mismatch");
    ok &= CheckComponentRegistry(
        foundTransform && foundTransform->size == sizeof(TestTransform),
        "Transform size metadata mismatch");
    ok &= CheckComponentRegistry(
        foundTransform && foundTransform->alignment == alignof(TestTransform),
        "Transform alignment metadata mismatch");
    ok &= CheckComponentRegistry(
        foundTransform && noc::HasFlag(
            foundTransform->flags,
            noc::ComponentTypeFlags::EditorVisible),
        "Transform EditorVisible flag missing");
    ok &= CheckComponentRegistry(
        foundTransform && noc::HasFlag(
            foundTransform->flags,
            noc::ComponentTypeFlags::Serializable),
        "Transform Serializable flag missing");

    const auto* foundRenderable = registry.FindByName("Test.Renderable");
    ok &= CheckComponentRegistry(
        foundRenderable && foundRenderable->typeId == kRenderableId,
        "FindByName(renderable) failed");
    ok &= CheckComponentRegistry(
        foundRenderable && foundRenderable->alignment == 64,
        "Over-aligned component metadata was truncated");

    ok &= CheckComponentRegistry(
        registry.MetadataAt(0) && registry.MetadataAt(0)->typeId == kTransformId,
        "Metadata enumeration index 0 is not sorted");
    ok &= CheckComponentRegistry(
        registry.MetadataAt(1) && registry.MetadataAt(1)->typeId == kNameId,
        "Metadata enumeration index 1 is not sorted");
    ok &= CheckComponentRegistry(
        registry.MetadataAt(2) && registry.MetadataAt(2)->typeId == kRenderableId,
        "Metadata enumeration index 2 is not sorted");

    ok &= CheckComponentRegistry(
        registry.Find(noc::ComponentTypeId{ 999 }) == nullptr,
        "Unknown type ID must return null");
    ok &= CheckComponentRegistry(
        registry.FindByName("Test.Unknown") == nullptr,
        "Unknown canonical name must return null");
    ok &= CheckComponentRegistry(
        registry.MetadataAt(3) == nullptr,
        "Out-of-range metadata access must return null");

    ok &= CheckComponentRegistry(
        !registry.Register(transform),
        "Duplicate type ID must be rejected");

    const auto duplicateName = noc::MakeComponentTypeMetadata<TestName>(
        noc::ComponentTypeId{ 40 },
        "Test.Name",
        1);
    ok &= CheckComponentRegistry(
        !registry.Register(duplicateName),
        "Duplicate canonical name must be rejected");

    noc::ComponentTypeMetadata invalidId = name;
    invalidId.typeId = noc::ComponentTypeId::Invalid();
    ok &= CheckComponentRegistry(
        !registry.Register(invalidId),
        "Invalid type ID must be rejected");

    noc::ComponentTypeMetadata invalidVersion = name;
    invalidVersion.typeId = noc::ComponentTypeId{ 41 };
    invalidVersion.version = 0;
    invalidVersion.canonicalName = "Test.InvalidVersion";
    ok &= CheckComponentRegistry(
        !registry.Register(invalidVersion),
        "Version zero must be rejected");

    noc::ComponentTypeMetadata invalidAlignment = name;
    invalidAlignment.typeId = noc::ComponentTypeId{ 42 };
    invalidAlignment.version = 1;
    invalidAlignment.canonicalName = "Test.InvalidAlignment";
    invalidAlignment.alignment = 3;
    ok &= CheckComponentRegistry(
        !registry.Register(invalidAlignment),
        "Non-power-of-two alignment must be rejected");

    registry.Shutdown();
    ok &= CheckComponentRegistry(
        allocator.OutstandingBytes() == 0,
        "ComponentRegistry leaked allocator-owned memory");

    NOC_LOG_INFO("Phase15", "Component registry tests %s", ok ? "PASS" : "FAIL");
    return ok;
}
