#pragma once

#include "Runtime/Entity.h"
#include "Runtime/World.h"

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

    // Design choice (not directly from the book): the editor hierarchy is a
    // transient projection of the authoritative World. Keeping traversal
    // separate from Win32 presentation gives production code and stress tests
    // one implementation without introducing a second scene authority.
    class EditorHierarchyModel final
    {
    public:
        [[nodiscard]] bool Rebuild(
            const noc::World& world,
            noc::EntityHandle toolOwnedEntity =
                noc::EntityHandle::Invalid())
        {
            std::vector<EditorHierarchyRow> nextRows;
            std::vector<noc::EntityHandle> roots;

            struct PendingRow
            {
                noc::EntityHandle entity{};
                int depth = 1;
            };

            std::vector<PendingRow> stack;
            std::vector<noc::EntityHandle> children;

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

                nextRows.reserve(expected);
                roots.reserve(expected);
                stack.reserve(expected);
                children.reserve(expected);

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
                        roots.push_back(entity);
                }

                for (auto it = roots.rbegin();
                     it != roots.rend();
                     ++it)
                {
                    stack.push_back({
                        *it,
                        1
                    });
                }

                while (!stack.empty())
                {
                    const PendingRow row =
                        stack.back();
                    stack.pop_back();

                    children.clear();

                    noc::EntityHandle child =
                        world.FirstChildOf(row.entity);

                    while (child.IsValid())
                    {
                        if (isAuthored(child))
                            children.push_back(child);

                        child =
                            world.NextSiblingOf(child);
                    }

                    nextRows.push_back({
                        row.entity,
                        row.depth,
                        !children.empty()
                    });

                    for (auto it = children.rbegin();
                         it != children.rend();
                         ++it)
                    {
                        stack.push_back({
                            *it,
                            row.depth + 1
                        });
                    }
                }
            }
            catch (const std::bad_alloc&)
            {
                return false;
            }

            rows_.swap(nextRows);
            return true;
        }

        void Clear() noexcept
        {
            rows_.clear();
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

    private:
        std::vector<EditorHierarchyRow> rows_;
    };
}
