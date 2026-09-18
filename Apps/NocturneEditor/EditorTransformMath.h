#pragma once

#include "Core/Math/MathTypes.h"

#include <algorithm>
#include <cmath>

namespace nocturne::editor
{
    [[nodiscard]] inline noc::Quat EditorNormalizeQuaternion(
        const noc::Quat& value) noexcept
    {
        const float lengthSq =
            value.x * value.x
            + value.y * value.y
            + value.z * value.z
            + value.w * value.w;

        if (!std::isfinite(lengthSq)
            || lengthSq <= 1.0e-12f)
        {
            return noc::Quat::Identity();
        }

        const float invLength =
            1.0f / std::sqrt(lengthSq);

        return {
            value.x * invLength,
            value.y * invLength,
            value.z * invLength,
            value.w * invLength
        };
    }

    [[nodiscard]] inline noc::Quat EditorMultiplyQuaternions(
        const noc::Quat& a,
        const noc::Quat& b) noexcept
    {
        return {
            a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
            a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
        };
    }

    // Design choice (not directly from the book): editor Euler XYZ means
    // rotations are authored X, then Y, then Z for column vectors. Runtime
    // authority remains quaternion; Euler values are presentation only.
    [[nodiscard]] inline noc::Quat EditorQuatFromEulerXYZDegrees(
        const noc::Vec3& degrees) noexcept
    {
        constexpr float kDegreesToRadians =
            0.017453292519943295f;

        const float hx =
            degrees.x * kDegreesToRadians * 0.5f;
        const float hy =
            degrees.y * kDegreesToRadians * 0.5f;
        const float hz =
            degrees.z * kDegreesToRadians * 0.5f;

        const noc::Quat qx{
            std::sin(hx), 0.0f, 0.0f, std::cos(hx)
        };
        const noc::Quat qy{
            0.0f, std::sin(hy), 0.0f, std::cos(hy)
        };
        const noc::Quat qz{
            0.0f, 0.0f, std::sin(hz), std::cos(hz)
        };

        return EditorNormalizeQuaternion(
            EditorMultiplyQuaternions(
                qz,
                EditorMultiplyQuaternions(qy, qx)));
    }

    [[nodiscard]] inline noc::Vec3 EditorEulerXYZDegreesFromQuat(
        const noc::Quat& source) noexcept
    {
        constexpr float kRadiansToDegrees =
            57.29577951308232f;

        const noc::Quat q =
            EditorNormalizeQuaternion(source);

        const float sinX =
            2.0f * (q.w * q.x + q.y * q.z);
        const float cosX =
            1.0f - 2.0f * (q.x * q.x + q.y * q.y);

        const float sinY =
            (std::clamp)(
                2.0f * (q.w * q.y - q.z * q.x),
                -1.0f,
                1.0f);

        const float sinZ =
            2.0f * (q.w * q.z + q.x * q.y);
        const float cosZ =
            1.0f - 2.0f * (q.y * q.y + q.z * q.z);

        return {
            std::atan2(sinX, cosX) * kRadiansToDegrees,
            std::asin(sinY) * kRadiansToDegrees,
            std::atan2(sinZ, cosZ) * kRadiansToDegrees
        };
    }
}
