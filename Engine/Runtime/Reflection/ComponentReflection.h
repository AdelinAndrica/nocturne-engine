#pragma once

#include "Runtime/Entity.h"
#include "Runtime/Reflection/ReflectionRegistry.h"

namespace noc
{
    class World;

    struct ComponentPropertyRuntimeContext
    {
        World* world = nullptr;
        EntityHandle entity{};
    };

    [[nodiscard]] inline PropertyAccessContext
    MakeComponentPropertyAccessContext(
        ComponentPropertyRuntimeContext& runtime) noexcept
    {
        PropertyAccessContext context{};
        context.userContext = &runtime;
        return context;
    }

    // Design choice (not directly from the book): generic component
    // membership enumeration scans frozen reflected component schemas and
    // invokes their Has adapters. No archetype/bitmask ECS migration is
    // introduced solely for editor reflection.
    [[nodiscard]] inline uint32_t ReflectedComponentCountForEntity(
        const ReflectionRegistry& registry,
        const World& world,
        EntityHandle entity)
    {
        uint32_t count = 0;
        const uint32_t typeCount = registry.ComponentTypeCount();

        for (uint32_t i = 0; i < typeCount; ++i)
        {
            const TypeMetadata* type = registry.ComponentTypeAt(i);
            if (type
                && type->componentMetadata
                && type->componentMetadata->has(world, entity))
            {
                ++count;
            }
        }

        return count;
    }

    [[nodiscard]] inline const TypeMetadata* ReflectedComponentAtForEntity(
        const ReflectionRegistry& registry,
        const World& world,
        EntityHandle entity,
        uint32_t componentIndex)
    {
        uint32_t seen = 0;
        const uint32_t typeCount = registry.ComponentTypeCount();

        for (uint32_t i = 0; i < typeCount; ++i)
        {
            const TypeMetadata* type = registry.ComponentTypeAt(i);
            if (!type
                || !type->componentMetadata
                || !type->componentMetadata->has(world, entity))
            {
                continue;
            }

            if (seen == componentIndex)
                return type;

            ++seen;
        }

        return nullptr;
    }
}
