#pragma once

#include "Runtime/Entity.h"
#include "Runtime/World.h"

#include <cstddef>
#include <cstdint>
#include <new>
#include <vector>

namespace nocturne::editor
{
    struct EditorHierarchyRow
    {
        noc::EntityHandle entity{};
        int depth = 1;
        bool hasAuthoredChildren = false;
    };

    struct EditorHierarchyExpansionEntry
    {
        noc::EntityHandle entity{};
        bool expanded = true;
    };

    [[nodiscard]] inline bool EditorHierarchyWasExpanded(
        const std::vector<EditorHierarchyExpansionEntry>& entries,
        noc::EntityHandle entity,
        bool fallback = true) noexcept
    {
        for (const EditorHierarchyExpansionEntry& entry : entries)
        {
            if (entry.entity == entity)
                return entry.expanded;
        }

        return fallback;
    }

    // Design choice (not directly from the book): the editor hierarchy is a
    // transient projection of the authoritative World. Scratch buffers persist
    // across rebuilds so a warmed hierarchy does not allocate per row/refresh.
    // Capacity growth is observable for performance/allocation tests.
    class EditorHierarchyModel final
    {
    public:
        [[nodiscard]] bool Rebuild(
            const noc::World& world,
            noc::EntityHandle toolOwnedEntity =
                noc::EntityHandle::Invalid())
        {
            lastCapacityGrowthCount_ = 0;

            nextRows_.clear();
            roots_.clear();
            stack_.clear();
            children_.clear();

            const auto isAuthored =
                [&](noc::EntityHandle entity) noexcept
            {
                return entity.IsValid()
                    && world.IsAlive(entity)
                    && entity != toolOwnedEntity;
            };

            try
            {
                const std::size_t expected =
                    static_cast<std::size_t>(
                        world.AliveCount());

                Reserve_(
                    nextRows_,
                    expected);
                Reserve_(
                    roots_,
                    expected);
                Reserve_(
                    stack_,
                    expected);
                Reserve_(
                    children_,
                    expected);

                for (uint32_t i = 0;
                     i < world.EntityCapacity();
                     ++i)
                {
                    const noc::EntityHandle entity =
                        world.EntityAtIndex(i);

                    if (!isAuthored(entity))
                        continue;

                    const noc::EntityHandle parent =
                        world.ParentOf(entity);

                    if (!isAuthored(parent))
                        roots_.push_back(entity);
                }

                for (auto it = roots_.rbegin();
                     it != roots_.rend();
                     ++it)
                {
                    stack_.push_back({
                        *it,
                        1
                    });
                }

                while (!stack_.empty())
                {
                    const PendingRow row =
                        stack_.back();
                    stack_.pop_back();

                    children_.clear();

                    noc::EntityHandle child =
                        world.FirstChildOf(row.entity);

                    while (child.IsValid())
                    {
                        if (isAuthored(child))
                            children_.push_back(child);

                        child =
                            world.NextSiblingOf(child);
                    }

                    nextRows_.push_back({
                        row.entity,
                        row.depth,
                        !children_.empty()
                    });

                    for (auto it = children_.rbegin();
                         it != children_.rend();
                         ++it)
                    {
                        stack_.push_back({
                            *it,
                            row.depth + 1
                        });
                    }
                }
            }
            catch (const std::bad_alloc&)
            {
                nextRows_.clear();
                return false;
            }

            rows_.swap(nextRows_);
            return true;
        }

        void Clear() noexcept
        {
            rows_.clear();
            nextRows_.clear();
            roots_.clear();
            stack_.clear();
            children_.clear();
            lastCapacityGrowthCount_ = 0;
        }

        [[nodiscard]] const std::vector<EditorHierarchyRow>&
        Rows() const noexcept
        {
            return rows_;
        }

        [[nodiscard]] std::size_t RowCount() const noexcept
        {
            return rows_.size();
        }

        [[nodiscard]] uint32_t
        LastCapacityGrowthCount() const noexcept
        {
            return lastCapacityGrowthCount_;
        }

        [[nodiscard]] std::size_t
        EstimatedRetainedBytes() const noexcept
        {
            return rows_.capacity()
                    * sizeof(EditorHierarchyRow)
                + nextRows_.capacity()
                    * sizeof(EditorHierarchyRow)
                + roots_.capacity()
                    * sizeof(noc::EntityHandle)
                + stack_.capacity()
                    * sizeof(PendingRow)
                + children_.capacity()
                    * sizeof(noc::EntityHandle);
        }

    private:
        struct PendingRow
        {
            noc::EntityHandle entity{};
            int depth = 1;
        };

        template <typename T>
        void Reserve_(
            std::vector<T>& storage,
            std::size_t required)
        {
            if (storage.capacity() >= required)
                return;

            storage.reserve(required);
            ++lastCapacityGrowthCount_;
        }

        std::vector<EditorHierarchyRow> rows_;
        std::vector<EditorHierarchyRow> nextRows_;
        std::vector<noc::EntityHandle> roots_;
        std::vector<PendingRow> stack_;
        std::vector<noc::EntityHandle> children_;
        uint32_t lastCapacityGrowthCount_ = 0;
    };
}
