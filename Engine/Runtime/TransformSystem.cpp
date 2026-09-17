#include "Runtime/TransformSystem.h"

#include "Runtime/EntityRegistry.h"

namespace noc
{
    TransformSystem::~TransformSystem()
    {
        Shutdown();
    }

    bool TransformSystem::Init(
        EntityRegistry& entities,
        IAllocator& allocator,
        uint32_t initialCapacity)
    {
        if (entities_)
            return true;

        if (!transforms_.Init(
                allocator,
                initialCapacity,
                entities.Capacity() > initialCapacity
                    ? entities.Capacity()
                    : initialCapacity))
        {
            return false;
        }

        entities_ = &entities;
        return true;
    }

    void TransformSystem::Shutdown()
    {
        transforms_.Shutdown();
        entities_ = nullptr;
    }

    TransformComponent* TransformSystem::Add(EntityHandle entity)
    {
        if (!IsUsableEntity_(entity))
            return nullptr;

        return transforms_.Emplace(entity);
    }

    bool TransformSystem::Remove(EntityHandle entity)
    {
        TransformComponent* transform = transforms_.Get(entity);
        if (!transform)
            return false;

        // Detach direct children one by one. Preserve each child's local TRS.
        EntityHandle child = transform->firstChild;
        while (child.IsValid())
        {
            TransformComponent* childTransform = transforms_.Get(child);
            if (!childTransform)
                break;

            const EntityHandle next = childTransform->nextSibling;

            childTransform->parent = EntityHandle::Invalid();
            childTransform->previousSibling = EntityHandle::Invalid();
            childTransform->nextSibling = EntityHandle::Invalid();

            MarkDirtySubtree_(child);
            child = next;
        }

        transform->firstChild = EntityHandle::Invalid();
        transform->lastChild = EntityHandle::Invalid();

        Detach_(entity);
        return transforms_.Remove(entity);
    }

    bool TransformSystem::Has(EntityHandle entity) const
    {
        return transforms_.Has(entity);
    }

    const TransformComponent* TransformSystem::Get(EntityHandle entity) const
    {
        return transforms_.Get(entity);
    }

    uint32_t TransformSystem::Count() const
    {
        return transforms_.Count();
    }

    bool TransformSystem::SetLocalTRS(
        EntityHandle entity,
        const Vec3& translation,
        const Quat& rotation,
        const Vec3& scale)
    {
        if (!IsUsableEntity_(entity))
            return false;

        TransformComponent* transform = transforms_.Get(entity);
        if (!transform)
            return false;

        transform->localTranslation = translation;
        transform->localRotation = rotation;
        transform->localScale = scale;

        MarkDirtySubtree_(entity);
        return true;
    }

    bool TransformSystem::SetParent(EntityHandle child, EntityHandle parent)
    {
        if (!IsUsableEntity_(child) || !transforms_.Has(child))
            return false;

        const bool detachToRoot = !parent.IsValid();

        if (!detachToRoot)
        {
            if (!IsUsableEntity_(parent) || !transforms_.Has(parent))
                return false;

            if (child == parent || WouldCreateCycle_(child, parent))
                return false;
        }

        TransformComponent* childTransform = transforms_.Get(child);
        if (!childTransform)
            return false;

        const EntityHandle targetParent =
            detachToRoot ? EntityHandle::Invalid() : parent;

        if (childTransform->parent == targetParent)
            return true;

        Detach_(child);

        if (targetParent.IsValid())
            AttachAsLastChild_(child, targetParent);

        MarkDirtySubtree_(child);
        return true;
    }

    EntityHandle TransformSystem::ParentOf(EntityHandle entity) const
    {
        const TransformComponent* transform = transforms_.Get(entity);
        return transform ? transform->parent : EntityHandle::Invalid();
    }

    EntityHandle TransformSystem::FirstChildOf(EntityHandle entity) const
    {
        const TransformComponent* transform = transforms_.Get(entity);
        return transform ? transform->firstChild : EntityHandle::Invalid();
    }

    EntityHandle TransformSystem::NextSiblingOf(EntityHandle entity) const
    {
        const TransformComponent* transform = transforms_.Get(entity);
        return transform ? transform->nextSibling : EntityHandle::Invalid();
    }

    bool TransformSystem::IsDirty(EntityHandle entity) const
    {
        const TransformComponent* transform = transforms_.Get(entity);
        return transform && transform->dirty;
    }

    void TransformSystem::Update()
    {
        if (!entities_)
            return;

        // Deterministic order is entity-slot order, not dense-storage order.
        // This keeps swap-remove from changing root processing order.
        const uint32_t capacity = entities_->Capacity();

        for (uint32_t index = 0; index < capacity; ++index)
        {
            const EntityHandle entity = entities_->EntityAtIndex(index);
            if (!entity.IsValid())
                continue;

            const TransformComponent* transform = transforms_.Get(entity);
            if (!transform || transform->parent.IsValid())
                continue;

            UpdateSubtree_(entity);
        }
    }

    bool TransformSystem::GetWorldMatrix(EntityHandle entity, Mat4& outWorld)
    {
        if (!IsUsableEntity_(entity))
            return false;

        TransformComponent* transform = transforms_.Get(entity);
        if (!transform)
            return false;

        if (transform->dirty)
        {
            EntityHandle highestDirty = entity;
            EntityHandle cursor = transform->parent;

            while (cursor.IsValid())
            {
                TransformComponent* ancestor = transforms_.Get(cursor);
                if (!ancestor)
                    return false;

                if (ancestor->dirty)
                    highestDirty = cursor;

                cursor = ancestor->parent;
            }

            UpdateSubtree_(highestDirty);
            transform = transforms_.Get(entity);
        }

        outWorld = transform->world;
        return true;
    }

    bool TransformSystem::IsUsableEntity_(EntityHandle entity) const
    {
        return entities_ && entities_->IsAlive(entity);
    }

    bool TransformSystem::WouldCreateCycle_(
        EntityHandle child,
        EntityHandle candidateParent) const
    {
        EntityHandle cursor = candidateParent;

        while (cursor.IsValid())
        {
            if (cursor == child)
                return true;

            if (!entities_ || !entities_->IsAlive(cursor))
                return true;

            const TransformComponent* transform = transforms_.Get(cursor);
            if (!transform)
                return true;

            cursor = transform->parent;
        }

        return false;
    }

    void TransformSystem::Detach_(EntityHandle child)
    {
        TransformComponent* childTransform = transforms_.Get(child);
        if (!childTransform)
            return;

        const EntityHandle parent = childTransform->parent;
        const EntityHandle previous = childTransform->previousSibling;
        const EntityHandle next = childTransform->nextSibling;

        if (previous.IsValid())
        {
            if (TransformComponent* previousTransform = transforms_.Get(previous))
                previousTransform->nextSibling = next;
        }
        else if (parent.IsValid())
        {
            if (TransformComponent* parentTransform = transforms_.Get(parent))
                parentTransform->firstChild = next;
        }

        if (next.IsValid())
        {
            if (TransformComponent* nextTransform = transforms_.Get(next))
                nextTransform->previousSibling = previous;
        }
        else if (parent.IsValid())
        {
            if (TransformComponent* parentTransform = transforms_.Get(parent))
                parentTransform->lastChild = previous;
        }

        childTransform->parent = EntityHandle::Invalid();
        childTransform->previousSibling = EntityHandle::Invalid();
        childTransform->nextSibling = EntityHandle::Invalid();
    }

    void TransformSystem::AttachAsLastChild_(
        EntityHandle child,
        EntityHandle parent)
    {
        TransformComponent* childTransform = transforms_.Get(child);
        TransformComponent* parentTransform = transforms_.Get(parent);
        if (!childTransform || !parentTransform)
            return;

        childTransform->parent = parent;
        childTransform->previousSibling = parentTransform->lastChild;
        childTransform->nextSibling = EntityHandle::Invalid();

        if (parentTransform->lastChild.IsValid())
        {
            if (TransformComponent* previousLast =
                    transforms_.Get(parentTransform->lastChild))
            {
                previousLast->nextSibling = child;
            }
        }
        else
        {
            parentTransform->firstChild = child;
        }

        parentTransform->lastChild = child;
    }

    void TransformSystem::MarkDirtySubtree_(EntityHandle root)
    {
        EntityHandle current = root;

        while (current.IsValid())
        {
            TransformComponent* transform = transforms_.Get(current);
            if (!transform)
                return;

            transform->dirty = true;

            if (transform->firstChild.IsValid())
            {
                current = transform->firstChild;
                continue;
            }

            while (current != root)
            {
                TransformComponent* currentTransform = transforms_.Get(current);
                if (!currentTransform)
                    return;

                if (currentTransform->nextSibling.IsValid())
                {
                    current = currentTransform->nextSibling;
                    break;
                }

                current = currentTransform->parent;
            }

            if (current == root)
                return;
        }
    }

    void TransformSystem::UpdateSubtree_(EntityHandle root)
    {
        EntityHandle current = root;

        while (current.IsValid())
        {
            UpdateOne_(current);

            TransformComponent* transform = transforms_.Get(current);
            if (!transform)
                return;

            if (transform->firstChild.IsValid())
            {
                current = transform->firstChild;
                continue;
            }

            while (current != root)
            {
                TransformComponent* currentTransform = transforms_.Get(current);
                if (!currentTransform)
                    return;

                if (currentTransform->nextSibling.IsValid())
                {
                    current = currentTransform->nextSibling;
                    break;
                }

                current = currentTransform->parent;
            }

            if (current == root)
                return;
        }
    }

    void TransformSystem::UpdateOne_(EntityHandle entity)
    {
        TransformComponent* transform = transforms_.Get(entity);
        if (!transform || !transform->dirty)
            return;

        const Mat4 local = TRS(
            transform->localTranslation,
            transform->localRotation,
            transform->localScale);

        if (transform->parent.IsValid())
        {
            const TransformComponent* parent =
                transforms_.Get(transform->parent);

            if (parent)
                transform->world = Mul(parent->world, local);
            else
                transform->world = local;
        }
        else
        {
            transform->world = local;
        }

        transform->dirty = false;
    }
}
