#include "EditorEntitySnapshot.h"

#include "Runtime/Reflection/ReflectionRegistry.h"
#include "Runtime/World.h"

#include <new>
#include <utility>

namespace nocturne::editor
{
    bool ReflectedEntitySubtreeSnapshot::Capture(
        EditorCommandContext& context,
        noc::EntityHandle root)
    {
        if (captured_
            || !context.world.IsAlive(root)
            || context.IsToolOwned(root))
        {
            return false;
        }

        struct Pending
        {
            noc::EntityHandle entity{};
            int32_t parentIndex = -1;
        };

        std::vector<Pending> pending;

        try
        {
            nodes_.reserve(context.world.AliveCount());
            pending.reserve(context.world.AliveCount());
            pending.push_back({ root, -1 });
        }
        catch (const std::bad_alloc&)
        {
            Clear();
            return false;
        }

        originalRootParent_ = context.world.ParentOf(root);

        while (!pending.empty())
        {
            const Pending current = pending.back();
            pending.pop_back();

            if (!context.world.IsAlive(current.entity)
                || context.IsToolOwned(current.entity))
            {
                Clear();
                return false;
            }

            Node node{};
            node.sourceEntity = current.entity;
            node.currentEntity = current.entity;
            node.parentIndex = current.parentIndex;

            const uint32_t componentTypeCount =
                context.reflection.ComponentTypeCount();

            try
            {
                node.components.reserve(componentTypeCount);
            }
            catch (const std::bad_alloc&)
            {
                Clear();
                return false;
            }

            for (uint32_t i = 0; i < componentTypeCount; ++i)
            {
                const noc::TypeMetadata* type =
                    context.reflection.ComponentTypeAt(i);

                if (!type
                    || !type->componentMetadata
                    || !type->componentMetadata->has(
                        context.world,
                        current.entity))
                {
                    continue;
                }

                ReflectedComponentSnapshot component;
                if (!component.Capture(
                        context,
                        current.entity,
                        type->typeId))
                {
                    Clear();
                    return false;
                }

                try
                {
                    node.components.push_back(
                        std::move(component));
                }
                catch (const std::bad_alloc&)
                {
                    Clear();
                    return false;
                }
            }

            const int32_t nodeIndex =
                static_cast<int32_t>(nodes_.size());

            try
            {
                nodes_.push_back(std::move(node));
            }
            catch (const std::bad_alloc&)
            {
                Clear();
                return false;
            }

            std::vector<noc::EntityHandle> children;
            noc::EntityHandle child =
                context.world.FirstChildOf(current.entity);

            try
            {
                while (child.IsValid())
                {
                    if (context.IsToolOwned(child))
                    {
                        Clear();
                        return false;
                    }

                    children.push_back(child);
                    child =
                        context.world.NextSiblingOf(child);
                }

                for (auto it = children.rbegin();
                     it != children.rend();
                     ++it)
                {
                    pending.push_back({
                        *it,
                        nodeIndex
                    });
                }
            }
            catch (const std::bad_alloc&)
            {
                Clear();
                return false;
            }
        }

        captured_ = !nodes_.empty();
        return captured_;
    }

    bool ReflectedEntitySubtreeSnapshot::DestroyCurrent(
        EditorCommandContext& context)
    {
        if (!captured_ || nodes_.empty())
            return false;

        for (const Node& node : nodes_)
        {
            if (!node.currentEntity.IsValid()
                || !context.world.IsAlive(node.currentEntity)
                || context.IsToolOwned(node.currentEntity))
            {
                return false;
            }
        }

        // Children first so World's Transform removal policy never promotes a
        // subtree node that is about to be deleted.
        for (auto it = nodes_.rbegin();
             it != nodes_.rend();
             ++it)
        {
            if (!context.world.DestroyEntity(
                    it->currentEntity))
            {
                return false;
            }

            it->currentEntity =
                noc::EntityHandle::Invalid();
        }

        return true;
    }

    bool ReflectedEntitySubtreeSnapshot::Instantiate(
        EditorCommandContext& context,
        noc::EntityHandle rootParent)
    {
        if (!captured_ || nodes_.empty())
            return false;

        for (const Node& node : nodes_)
        {
            if (node.currentEntity.IsValid())
                return false;
        }

        if (rootParent.IsValid()
            && (!context.world.IsAlive(rootParent)
                || context.IsToolOwned(rootParent)))
        {
            return false;
        }

        auto rollback = [&]()
        {
            for (auto it = nodes_.rbegin();
                 it != nodes_.rend();
                 ++it)
            {
                if (it->currentEntity.IsValid()
                    && context.world.IsAlive(
                        it->currentEntity))
                {
                    (void)context.world.DestroyEntity(
                        it->currentEntity);
                }

                it->currentEntity =
                    noc::EntityHandle::Invalid();
            }
        };

        for (Node& node : nodes_)
        {
            node.currentEntity =
                context.world.CreateEntity();

            if (!node.currentEntity.IsValid())
            {
                rollback();
                return false;
            }
        }

        for (Node& node : nodes_)
        {
            for (ReflectedComponentSnapshot& component :
                 node.components)
            {
                const noc::TypeMetadata* type =
                    context.reflection.FindType(
                        component.ComponentType());

                if (!type
                    || !type->componentMetadata
                    || type->componentMetadata->has(
                        context.world,
                        node.currentEntity)
                    || !type->componentMetadata->add(
                        context.world,
                        node.currentEntity)
                    || !component.Restore(
                        context,
                        node.currentEntity))
                {
                    rollback();
                    return false;
                }
            }
        }

        for (uint32_t i = 0;
             i < static_cast<uint32_t>(nodes_.size());
             ++i)
        {
            Node& node = nodes_[i];

            noc::EntityHandle parent =
                noc::EntityHandle::Invalid();

            if (node.parentIndex >= 0)
            {
                const uint32_t parentIndex =
                    static_cast<uint32_t>(
                        node.parentIndex);

                if (parentIndex >= nodes_.size())
                {
                    rollback();
                    return false;
                }

                parent =
                    nodes_[parentIndex].currentEntity;
            }
            else
            {
                parent = rootParent;
            }

            if (parent.IsValid()
                && !context.world.SetParent(
                    node.currentEntity,
                    parent))
            {
                rollback();
                return false;
            }
        }

        context.world.Update();
        return true;
    }

    void ReflectedEntitySubtreeSnapshot::ForgetCurrentInstances() noexcept
    {
        InvalidateCurrent_();
    }

    void ReflectedEntitySubtreeSnapshot::Clear() noexcept
    {
        nodes_.clear();
        originalRootParent_ =
            noc::EntityHandle::Invalid();
        captured_ = false;
    }

    bool ReflectedEntitySubtreeSnapshot::IsValid() const noexcept
    {
        return captured_ && !nodes_.empty();
    }

    uint32_t ReflectedEntitySubtreeSnapshot::EntityCount() const noexcept
    {
        return static_cast<uint32_t>(nodes_.size());
    }

    noc::EntityHandle
    ReflectedEntitySubtreeSnapshot::OriginalRootParent() const noexcept
    {
        return originalRootParent_;
    }

    noc::EntityHandle
    ReflectedEntitySubtreeSnapshot::CurrentRoot() const noexcept
    {
        return nodes_.empty()
            ? noc::EntityHandle::Invalid()
            : nodes_.front().currentEntity;
    }

    noc::EntityHandle
    ReflectedEntitySubtreeSnapshot::CurrentEntityForSource(
        noc::EntityHandle sourceEntity) const noexcept
    {
        if (!sourceEntity.IsValid())
            return noc::EntityHandle::Invalid();

        for (const Node& node : nodes_)
        {
            if (node.sourceEntity == sourceEntity)
                return node.currentEntity;
        }

        return noc::EntityHandle::Invalid();
    }

    std::size_t
    ReflectedEntitySubtreeSnapshot::MemoryCostBytes() const noexcept
    {
        std::size_t result =
            sizeof(*this)
            + nodes_.capacity() * sizeof(Node);

        for (const Node& node : nodes_)
        {
            result += node.components.capacity()
                * sizeof(ReflectedComponentSnapshot);

            for (const ReflectedComponentSnapshot& component :
                 node.components)
            {
                result += component.MemoryCostBytes();
            }
        }

        return result;
    }

    void ReflectedEntitySubtreeSnapshot::InvalidateCurrent_() noexcept
    {
        for (Node& node : nodes_)
            node.currentEntity =
                noc::EntityHandle::Invalid();
    }
}
