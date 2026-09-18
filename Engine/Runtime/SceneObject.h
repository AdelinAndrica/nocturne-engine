#pragma once

#include "Runtime/Entity.h"

namespace noc
{
    // Compatibility alias for Phase 14/editor call sites.
    //
    // Phase 15 establishes EntityHandle as the single runtime identity type.
    // SceneObjectHandle must not become a second handle authority again.
    using SceneObjectHandle = EntityHandle;
}
