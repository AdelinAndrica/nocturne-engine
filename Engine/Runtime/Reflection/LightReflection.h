#pragma once

#include "Runtime/Reflection/ReflectionIds.h"

namespace noc
{
    class ReflectionRegistry;

    namespace FoundationTypeIds
    {
        // Design choice (not directly from the book): non-component foundation
        // value types occupy a stable reflected-ID range separate from the
        // 32-bit ComponentTypeId compatibility range.
        inline constexpr TypeId LightType{ 0x2000000000000001ull };
    }

    [[nodiscard]] bool RegisterLightReflectionTypes(
        ReflectionRegistry& registry);
}
