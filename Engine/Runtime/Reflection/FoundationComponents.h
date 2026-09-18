#pragma once

namespace noc
{
    class ReflectionRegistry;

    [[nodiscard]] bool RegisterFoundationComponentReflectionTypes(
        ReflectionRegistry& registry);
}
