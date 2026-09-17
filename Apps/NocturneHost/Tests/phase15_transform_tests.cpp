#include "Runtime/TransformSystem.h"

#include "Core/Log.h"
#include "Core/Memory/Allocator.h"
#include "Core/Memory/DebugAlloc.h"
#include "Runtime/EntityRegistry.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace
{
    bool CheckTransform(bool condition, const char* message)
    {
        if (!condition)
        {
            NOC_LOG_ERROR("Phase15", "%s", message);
            return false;
        }
        return true;
    }

    bool Near(float a, float b, float epsilon = 1e-4f)
    {
        return std::fabs(a - b) <= epsilon;
    }

    bool TranslationIs(const noc::Mat4& matrix, float x, float y, float z)
    {
        return Near(matrix.m[12], x)
            && Near(matrix.m[13], y)
            && Near(matrix.m[14], z);
    }
}

bool RunPhase15TransformTests()
{
    NOC_LOG_INFO("Phase15", "%s", "Transform hierarchy tests begin");

    noc::MallocAllocator backing;
    noc::DebugAlloc allocator(backing);
    noc::EntityRegistry entities;
    noc::TransformSystem transforms;

    bool ok = true;

    ok &= CheckTransform(entities.Init(allocator, 8), "EntityRegistry init failed");
    ok &= CheckTransform(transforms.Init(entities, allocator, 8), "TransformSystem init failed");

    const noc::EntityHandle rootA = entities.Create();
    const noc::EntityHandle child = entities.Create();
    const noc::EntityHandle grandchild = entities.Create();
    const noc::EntityHandle sibling = entities.Create();
    const noc::EntityHandle rootB = entities.Create();

    ok &= CheckTransform(transforms.Add(rootA) != nullptr, "Add rootA failed");
    ok &= CheckTransform(transforms.Add(child) != nullptr, "Add child failed");
    ok &= CheckTransform(transforms.Add(grandchild) != nullptr, "Add grandchild failed");
    ok &= CheckTransform(transforms.Add(sibling) != nullptr, "Add sibling failed");
    ok &= CheckTransform(transforms.Add(rootB) != nullptr, "Add rootB failed");
    ok &= CheckTransform(transforms.Add(rootA) == nullptr, "Duplicate Transform add must fail");
    ok &= CheckTransform(transforms.Count() == 5, "Transform count mismatch");

    const noc::TransformComponent* defaultTransform = transforms.Get(rootA);
    ok &= CheckTransform(defaultTransform != nullptr, "Default transform lookup failed");
    ok &= CheckTransform(
        defaultTransform
            && Near(defaultTransform->localTranslation.x, 0.0f)
            && Near(defaultTransform->localScale.x, 1.0f)
            && !defaultTransform->parent.IsValid(),
        "Default TransformComponent state is invalid");

    ok &= CheckTransform(
        transforms.SetLocalTRS(
            rootA,
            noc::Vec3{ 10.0f, 0.0f, 0.0f },
            noc::Quat::Identity(),
            noc::Vec3::One()),
        "SetLocalTRS(rootA) failed");

    ok &= CheckTransform(
        transforms.SetLocalTRS(
            child,
            noc::Vec3{ 2.0f, 0.0f, 0.0f },
            noc::Quat::Identity(),
            noc::Vec3::One()),
        "SetLocalTRS(child) failed");

    ok &= CheckTransform(
        transforms.SetLocalTRS(
            grandchild,
            noc::Vec3{ 3.0f, 0.0f, 0.0f },
            noc::Quat::Identity(),
            noc::Vec3::One()),
        "SetLocalTRS(grandchild) failed");

    ok &= CheckTransform(
        transforms.SetLocalTRS(
            sibling,
            noc::Vec3{ 0.0f, 4.0f, 0.0f },
            noc::Quat::Identity(),
            noc::Vec3::One()),
        "SetLocalTRS(sibling) failed");

    ok &= CheckTransform(
        transforms.SetLocalTRS(
            rootB,
            noc::Vec3{ -5.0f, 0.0f, 0.0f },
            noc::Quat::Identity(),
            noc::Vec3::One()),
        "SetLocalTRS(rootB) failed");

    ok &= CheckTransform(transforms.SetParent(child, rootA), "Parent child->rootA failed");
    ok &= CheckTransform(
        transforms.SetParent(grandchild, child),
        "Parent grandchild->child failed");
    ok &= CheckTransform(transforms.SetParent(sibling, rootA), "Parent sibling->rootA failed");

    ok &= CheckTransform(transforms.ParentOf(child) == rootA, "ParentOf(child) mismatch");
    ok &= CheckTransform(transforms.ParentOf(grandchild) == child, "ParentOf(grandchild) mismatch");
    ok &= CheckTransform(transforms.FirstChildOf(rootA) == child, "First child order mismatch");
    ok &= CheckTransform(transforms.NextSiblingOf(child) == sibling, "Sibling order mismatch");

    // Re-applying the same parent is a successful no-op.
    ok &= CheckTransform(transforms.SetParent(child, rootA), "Same-parent no-op failed");

    // Cycles and self-parenting must be rejected without damaging the tree.
    ok &= CheckTransform(!transforms.SetParent(rootA, rootA), "Self-parenting was accepted");
    ok &= CheckTransform(
        !transforms.SetParent(rootA, grandchild),
        "Indirect hierarchy cycle was accepted");
    ok &= CheckTransform(!transforms.ParentOf(rootA).IsValid(), "Cycle rejection mutated rootA");

    transforms.Update();

    noc::Mat4 world{};
    ok &= CheckTransform(
        transforms.GetWorldMatrix(rootA, world) && TranslationIs(world, 10.0f, 0.0f, 0.0f),
        "rootA world transform incorrect");
    ok &= CheckTransform(
        transforms.GetWorldMatrix(child, world) && TranslationIs(world, 12.0f, 0.0f, 0.0f),
        "child inherited translation incorrectly");
    ok &= CheckTransform(
        transforms.GetWorldMatrix(grandchild, world) && TranslationIs(world, 15.0f, 0.0f, 0.0f),
        "grandchild inherited translation incorrectly");
    ok &= CheckTransform(
        transforms.GetWorldMatrix(sibling, world) && TranslationIs(world, 10.0f, 4.0f, 0.0f),
        "sibling inherited translation incorrectly");

    ok &= CheckTransform(
        !transforms.IsDirty(rootA)
            && !transforms.IsDirty(child)
            && !transforms.IsDirty(grandchild),
        "Update did not clear dirty flags");

    // Parent mutation must dirty the full descendant subtree.
    ok &= CheckTransform(
        transforms.SetLocalTRS(
            rootA,
            noc::Vec3{ 20.0f, 0.0f, 0.0f },
            noc::Quat::Identity(),
            noc::Vec3::One()),
        "Second SetLocalTRS(rootA) failed");
    ok &= CheckTransform(
        transforms.IsDirty(rootA)
            && transforms.IsDirty(child)
            && transforms.IsDirty(grandchild)
            && transforms.IsDirty(sibling),
        "Dirty propagation did not reach the full subtree");

    ok &= CheckTransform(
        transforms.GetWorldMatrix(grandchild, world)
            && TranslationIs(world, 25.0f, 0.0f, 0.0f),
        "On-demand world update did not refresh dirty ancestor subtree");

    // Reparent child subtree to rootB.
    ok &= CheckTransform(transforms.SetParent(child, rootB), "Reparent child->rootB failed");
    ok &= CheckTransform(transforms.ParentOf(child) == rootB, "Reparent did not update parent");
    ok &= CheckTransform(
        transforms.FirstChildOf(rootA) == sibling,
        "Reparent did not repair old parent's child list");
    ok &= CheckTransform(
        transforms.FirstChildOf(rootB) == child,
        "Reparent did not attach to new parent");

    transforms.Update();
    ok &= CheckTransform(
        transforms.GetWorldMatrix(child, world) && TranslationIs(world, -3.0f, 0.0f, 0.0f),
        "Reparented child world transform incorrect");
    ok &= CheckTransform(
        transforms.GetWorldMatrix(grandchild, world) && TranslationIs(world, 0.0f, 0.0f, 0.0f),
        "Reparented grandchild world transform incorrect");

    // Detach back to root.
    ok &= CheckTransform(
        transforms.SetParent(child, noc::EntityHandle::Invalid()),
        "Detach child to root failed");
    transforms.Update();
    ok &= CheckTransform(!transforms.ParentOf(child).IsValid(), "Detached child still has parent");
    ok &= CheckTransform(
        transforms.GetWorldMatrix(child, world) && TranslationIs(world, 2.0f, 0.0f, 0.0f),
        "Detached child did not preserve local TRS");

    // Remove-parent policy: direct children become roots and keep local TRS.
    ok &= CheckTransform(transforms.SetParent(child, rootA), "Reattach child->rootA failed");
    ok &= CheckTransform(transforms.SetParent(sibling, rootA), "Sibling same-parent no-op failed");
    transforms.Update();

    ok &= CheckTransform(transforms.Remove(rootA), "Remove(rootA) failed");
    ok &= CheckTransform(!transforms.Has(rootA), "Removed transform is still present");
    ok &= CheckTransform(!transforms.ParentOf(child).IsValid(), "Child was not promoted to root");
    ok &= CheckTransform(!transforms.ParentOf(sibling).IsValid(), "Sibling was not promoted to root");
    ok &= CheckTransform(transforms.IsDirty(child), "Promoted child was not marked dirty");

    transforms.Update();
    ok &= CheckTransform(
        transforms.GetWorldMatrix(child, world) && TranslationIs(world, 2.0f, 0.0f, 0.0f),
        "Promoted child did not preserve local TRS");
    ok &= CheckTransform(
        transforms.GetWorldMatrix(grandchild, world) && TranslationIs(world, 5.0f, 0.0f, 0.0f),
        "Descendant world after parent removal is incorrect");

    // Invalid/stale handles must be rejected.
    ok &= CheckTransform(
        !transforms.SetLocalTRS(
            noc::EntityHandle::Invalid(),
            noc::Vec3::Zero(),
            noc::Quat::Identity(),
            noc::Vec3::One()),
        "Invalid handle mutation was accepted");

    const noc::EntityHandle stale = entities.Create();
    ok &= CheckTransform(transforms.Add(stale) != nullptr, "Add stale-test transform failed");
    ok &= CheckTransform(transforms.Remove(stale), "Remove stale-test transform failed");
    ok &= CheckTransform(entities.Destroy(stale), "Destroy stale-test entity failed");
    ok &= CheckTransform(
        transforms.Add(stale) == nullptr,
        "Stale entity handle was accepted for transform add");

    // Deep hierarchy validates stackless traversal. Each local transform adds
    // +1 on X, so the tail must resolve to depth on X.
    constexpr uint32_t kDepth = 1024;
    std::vector<noc::EntityHandle> deep;
    deep.reserve(kDepth);

    for (uint32_t i = 0; i < kDepth; ++i)
    {
        const noc::EntityHandle entity = entities.Create();
        deep.push_back(entity);

        ok &= CheckTransform(transforms.Add(entity) != nullptr, "Deep hierarchy add failed");
        ok &= CheckTransform(
            transforms.SetLocalTRS(
                entity,
                noc::Vec3{ 1.0f, 0.0f, 0.0f },
                noc::Quat::Identity(),
                noc::Vec3::One()),
            "Deep hierarchy local transform failed");

        if (i > 0)
        {
            ok &= CheckTransform(
                transforms.SetParent(entity, deep[i - 1u]),
                "Deep hierarchy parent link failed");
        }
    }

    transforms.Update();
    ok &= CheckTransform(
        transforms.GetWorldMatrix(deep.back(), world)
            && TranslationIs(world, static_cast<float>(kDepth), 0.0f, 0.0f),
        "Deep hierarchy world propagation failed");

    transforms.Shutdown();

    // Entity registry remains independent and can be destroyed afterwards.
    entities.Shutdown();

    ok &= CheckTransform(
        allocator.OutstandingBytes() == 0,
        "Transform tests leaked allocator-owned memory");

    NOC_LOG_INFO("Phase15", "Transform hierarchy tests %s", ok ? "PASS" : "FAIL");
    return ok;
}
