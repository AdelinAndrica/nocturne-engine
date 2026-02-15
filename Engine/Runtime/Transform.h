#pragma once
#include "Core/Math/MathTypes.h"

namespace noc
{
    // Minimal camera for frustum tests (Phase 10)
    struct Camera
    {
        math::Vec3 position = math::Vec3::Zero();
        math::Mat4 view = math::Mat4::Identity();
        math::Mat4 proj = math::Mat4::Identity();

        math::Mat4 ViewProj() const { return math::Mul(proj, view); }
    };
}
