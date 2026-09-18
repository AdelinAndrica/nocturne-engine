#pragma once
#include <cmath>
#include <cstdint>

namespace noc
{
    // ============================================================
    // Vec2
    // ============================================================

    struct Vec2
    {
        float x{}, y{};

        constexpr Vec2() = default;
        constexpr Vec2(float X, float Y) : x(X), y(Y) {}

        static constexpr Vec2 Zero() { return { 0,0 }; }
        static constexpr Vec2 One() { return { 1,1 }; }
    };

    // ============================================================
    // Vec3
    // ============================================================

    struct Vec3
    {
        float x{}, y{}, z{};

        constexpr Vec3() = default;
        constexpr Vec3(float X, float Y, float Z) : x(X), y(Y), z(Z) {}

        static constexpr Vec3 Zero() { return { 0,0,0 }; }
        static constexpr Vec3 One() { return { 1,1,1 }; }

        friend constexpr Vec3 operator+(const Vec3& a, const Vec3& b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
        friend constexpr Vec3 operator-(const Vec3& a, const Vec3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
        friend constexpr Vec3 operator*(const Vec3& v, float s) { return { v.x * s, v.y * s, v.z * s }; }
        friend constexpr Vec3 operator*(float s, const Vec3& v) { return v * s; }
    };

    inline float Dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

    inline Vec3 Cross(const Vec3& a, const Vec3& b)
    {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }

    inline float LengthSq(const Vec3& v) { return Dot(v, v); }
    inline float Length(const Vec3& v) { return std::sqrt(LengthSq(v)); }

    inline Vec3 Normalize(const Vec3& v)
    {
        const float len = Length(v);
        if (len <= 1e-6f) return Vec3::Zero();
        return v * (1.0f / len);
    }

    // ============================================================
    // Vec4
    // ============================================================

    struct Vec4
    {
        float x{}, y{}, z{}, w{};

        constexpr Vec4() = default;
        constexpr Vec4(float X, float Y, float Z, float W)
            : x(X), y(Y), z(Z), w(W) {}

        static constexpr Vec4 Zero() { return { 0,0,0,0 }; }
        static constexpr Vec4 One() { return { 1,1,1,1 }; }
    };

    // ============================================================
    // Quat
    // ============================================================

    struct Quat
    {
        float x{}, y{}, z{}, w{ 1.0f };

        constexpr Quat() = default;
        constexpr Quat(float X, float Y, float Z, float W) : x(X), y(Y), z(Z), w(W) {}

        static constexpr Quat Identity() { return { 0,0,0,1 }; }
    };

    // Rotate vector by unit quaternion (no matrices)
    inline Vec3 Rotate(const Quat& q, const Vec3& v)
    {
        Vec3 qv{ q.x, q.y, q.z };
        Vec3 t = Cross(qv, v) * 2.0f;
        return v + t * q.w + Cross(qv, t);
    }

    // ============================================================
    // Mat4 (COLUMN-MAJOR, m[col*4 + row])
    // ============================================================

    struct Mat4
    {
        float m[16]{};

        static Mat4 Identity()
        {
            Mat4 r{};
            r.m[0] = 1.0f;
            r.m[5] = 1.0f;
            r.m[10] = 1.0f;
            r.m[15] = 1.0f;
            return r;
        }
    };

    // Access helper: element at (row, col)
    inline float& M(Mat4& m, int row, int col) { return m.m[col * 4 + row]; }
    inline float  M(const Mat4& m, int row, int col) { return m.m[col * 4 + row]; }

    // Matrix multiply (column-major, column vectors): r = a * b
    inline Mat4 Mul(const Mat4& a, const Mat4& b)
    {
        Mat4 r{};
        for (int c = 0; c < 4; ++c)
        {
            for (int rrow = 0; rrow < 4; ++rrow)
            {
                M(r, rrow, c) =
                    M(a, rrow, 0) * M(b, 0, c) +
                    M(a, rrow, 1) * M(b, 1, c) +
                    M(a, rrow, 2) * M(b, 2, c) +
                    M(a, rrow, 3) * M(b, 3, c);
            }
        }
        return r;
    }

    // Transform point (column vector): p' = M * [p,1]
    inline Vec3 TransformPoint(const Mat4& m, const Vec3& p)
    {
        const float x = M(m, 0, 0) * p.x + M(m, 0, 1) * p.y + M(m, 0, 2) * p.z + M(m, 0, 3) * 1.0f;
        const float y = M(m, 1, 0) * p.x + M(m, 1, 1) * p.y + M(m, 1, 2) * p.z + M(m, 1, 3) * 1.0f;
        const float z = M(m, 2, 0) * p.x + M(m, 2, 1) * p.y + M(m, 2, 2) * p.z + M(m, 2, 3) * 1.0f;
        return { x,y,z };
    }

    inline Mat4 Translation(const Vec3& t)
    {
        Mat4 r = Mat4::Identity();
        r.m[12] = t.x;
        r.m[13] = t.y;
        r.m[14] = t.z;
        return r;
    }

    inline Mat4 Scale(const Vec3& s)
    {
        Mat4 r{};
        r.m[0] = s.x;
        r.m[5] = s.y;
        r.m[10] = s.z;
        r.m[15] = 1.0f;
        return r;
    }

    inline Mat4 RotationFromQuat(const Quat& q)
    {
        const float x = q.x, y = q.y, z = q.z, w = q.w;
        const float xx = x * x, yy = y * y, zz = z * z;
        const float xy = x * y, xz = x * z, yz = y * z;
        const float wx = w * x, wy = w * y, wz = w * z;

        Mat4 r = Mat4::Identity();

        // column-major rotation matrix
        r.m[0] = 1.0f - 2.0f * (yy + zz);
        r.m[1] = 2.0f * (xy + wz);
        r.m[2] = 2.0f * (xz - wy);

        r.m[4] = 2.0f * (xy - wz);
        r.m[5] = 1.0f - 2.0f * (xx + zz);
        r.m[6] = 2.0f * (yz + wx);

        r.m[8] = 2.0f * (xz + wy);
        r.m[9] = 2.0f * (yz - wx);
        r.m[10] = 1.0f - 2.0f * (xx + yy);

        return r;
    }

    inline Mat4 TRS(const Vec3& t, const Quat& r, const Vec3& s)
    {
        // Column-vector convention: M = T * R * S
        return Mul(Translation(t), Mul(RotationFromQuat(r), Scale(s)));
    }

    inline bool IsFiniteMath(const Vec3& v)
    {
        return std::isfinite(v.x)
            && std::isfinite(v.y)
            && std::isfinite(v.z);
    }

    inline bool IsFiniteMath(const Quat& q)
    {
        return std::isfinite(q.x)
            && std::isfinite(q.y)
            && std::isfinite(q.z)
            && std::isfinite(q.w);
    }

    inline bool IsFiniteMath(const Mat4& m)
    {
        for (float value : m.m)
        {
            if (!std::isfinite(value))
                return false;
        }
        return true;
    }

    // Affine inverse for column-vector transforms. Returns false for
    // non-affine/singular/non-finite input instead of manufacturing a matrix.
    inline bool TryInverseAffine(
        const Mat4& input,
        Mat4& outInverse,
        float epsilon = 1.0e-6f)
    {
        if (!IsFiniteMath(input)
            || std::fabs(M(input, 3, 0)) > epsilon
            || std::fabs(M(input, 3, 1)) > epsilon
            || std::fabs(M(input, 3, 2)) > epsilon
            || std::fabs(M(input, 3, 3) - 1.0f) > epsilon)
        {
            return false;
        }

        const float a00 = M(input, 0, 0);
        const float a01 = M(input, 0, 1);
        const float a02 = M(input, 0, 2);
        const float a10 = M(input, 1, 0);
        const float a11 = M(input, 1, 1);
        const float a12 = M(input, 1, 2);
        const float a20 = M(input, 2, 0);
        const float a21 = M(input, 2, 1);
        const float a22 = M(input, 2, 2);

        const float c00 = a11 * a22 - a12 * a21;
        const float c01 = a02 * a21 - a01 * a22;
        const float c02 = a01 * a12 - a02 * a11;
        const float c10 = a12 * a20 - a10 * a22;
        const float c11 = a00 * a22 - a02 * a20;
        const float c12 = a02 * a10 - a00 * a12;
        const float c20 = a10 * a21 - a11 * a20;
        const float c21 = a01 * a20 - a00 * a21;
        const float c22 = a00 * a11 - a01 * a10;

        const float determinant =
            a00 * c00 + a01 * c10 + a02 * c20;

        if (!std::isfinite(determinant)
            || std::fabs(determinant) <= epsilon)
        {
            return false;
        }

        const float invDet = 1.0f / determinant;
        Mat4 result = Mat4::Identity();

        M(result, 0, 0) = c00 * invDet;
        M(result, 0, 1) = c01 * invDet;
        M(result, 0, 2) = c02 * invDet;
        M(result, 1, 0) = c10 * invDet;
        M(result, 1, 1) = c11 * invDet;
        M(result, 1, 2) = c12 * invDet;
        M(result, 2, 0) = c20 * invDet;
        M(result, 2, 1) = c21 * invDet;
        M(result, 2, 2) = c22 * invDet;

        const Vec3 translation{
            M(input, 0, 3),
            M(input, 1, 3),
            M(input, 2, 3)
        };

        const Vec3 inverseTranslation{
            -(M(result, 0, 0) * translation.x
                + M(result, 0, 1) * translation.y
                + M(result, 0, 2) * translation.z),
            -(M(result, 1, 0) * translation.x
                + M(result, 1, 1) * translation.y
                + M(result, 1, 2) * translation.z),
            -(M(result, 2, 0) * translation.x
                + M(result, 2, 1) * translation.y
                + M(result, 2, 2) * translation.z)
        };

        M(result, 0, 3) = inverseTranslation.x;
        M(result, 1, 3) = inverseTranslation.y;
        M(result, 2, 3) = inverseTranslation.z;

        if (!IsFiniteMath(result)
            return false;

        outInverse = result;
        return true;
    }

    inline Quat QuatFromRotationMatrix(const Mat4& m)
    {
        Quat q{};
        const float r00 = M(m, 0, 0);
        const float r11 = M(m, 1, 1);
        const float r22 = M(m, 2, 2);
        const float trace = r00 + r11 + r22;

        if (trace > 0.0f)
        {
            const float s =
                std::sqrt(trace + 1.0f) * 2.0f;
            q.w = 0.25f * s;
            q.x = (M(m, 2, 1) - M(m, 1, 2)) / s;
            q.y = (M(m, 0, 2) - M(m, 2, 0)) / s;
            q.z = (M(m, 1, 0) - M(m, 0, 1)) / s;
        }
        else if (r00 > r11 && r00 > r22)
        {
            const float s =
                std::sqrt(1.0f + r00 - r11 - r22) * 2.0f;
            q.w = (M(m, 2, 1) - M(m, 1, 2)) / s;
            q.x = 0.25f * s;
            q.y = (M(m, 0, 1) + M(m, 1, 0)) / s;
            q.z = (M(m, 0, 2) + M(m, 2, 0)) / s;
        }
        else if (r11 > r22)
        {
            const float s =
                std::sqrt(1.0f + r11 - r00 - r22) * 2.0f;
            q.w = (M(m, 0, 2) - M(m, 2, 0)) / s;
            q.x = (M(m, 0, 1) + M(m, 1, 0)) / s;
            q.y = 0.25f * s;
            q.z = (M(m, 1, 2) + M(m, 2, 1)) / s;
        }
        else
        {
            const float s =
                std::sqrt(1.0f + r22 - r00 - r11) * 2.0f;
            q.w = (M(m, 1, 0) - M(m, 0, 1)) / s;
            q.x = (M(m, 0, 2) + M(m, 2, 0)) / s;
            q.y = (M(m, 1, 2) + M(m, 2, 1)) / s;
            q.z = 0.25f * s;
        }

        const float lengthSq =
            q.x * q.x
            + q.y * q.y
            + q.z * q.z
            + q.w * q.w;

        if (!std::isfinite(lengthSq)
            || lengthSq <= 1.0e-12f)
        {
            return Quat::Identity();
        }

        const float invLength =
            1.0f / std::sqrt(lengthSq);

        return {
            q.x * invLength,
            q.y * invLength,
            q.z * invLength,
            q.w * invLength
        };
    }

    // Decomposes only matrices representable as T*R*S. Design choice
    // (not directly from the book): shear is rejected rather than silently
    // approximated because editor reparent must preserve world pose exactly.
    inline bool TryDecomposeTRS(
        const Mat4& input,
        Vec3& outTranslation,
        Quat& outRotation,
        Vec3& outScale,
        float epsilon = 1.0e-5f,
        float shearEpsilon = 1.0e-4f)
    {
        if (!IsFiniteMath(input)
            || std::fabs(M(input, 3, 0)) > epsilon
            || std::fabs(M(input, 3, 1)) > epsilon
            || std::fabs(M(input, 3, 2)) > epsilon
            || std::fabs(M(input, 3, 3) - 1.0f) > epsilon)
        {
            return false;
        }

        Vec3 c0{
            M(input, 0, 0),
            M(input, 1, 0),
            M(input, 2, 0)
        };
        Vec3 c1{
            M(input, 0, 1),
            M(input, 1, 1),
            M(input, 2, 1)
        };
        Vec3 c2{
            M(input, 0, 2),
            M(input, 1, 2),
            M(input, 2, 2)
        };

        float sx = Length(c0);
        float sy = Length(c1);
        float sz = Length(c2);

        if (!std::isfinite(sx)
            || !std::isfinite(sy)
            || !std::isfinite(sz)
            || sx <= epsilon
            || sy <= epsilon
            || sz <= epsilon)
        {
            return false;
        }

        Vec3 n0 = c0 * (1.0f / sx);
        Vec3 n1 = c1 * (1.0f / sy);
        Vec3 n2 = c2 * (1.0f / sz);

        if (std::fabs(Dot(n0, n1)) > shearEpsilon
            || std::fabs(Dot(n0, n2)) > shearEpsilon
            || std::fabs(Dot(n1, n2)) > shearEpsilon)
        {
            return false;
        }

        float handedness =
            Dot(Cross(n0, n1), n2);

        if (!std::isfinite(handedness)
            || std::fabs(handedness) <= epsilon)
        {
            return false;
        }

        if (handedness < 0.0f)
        {
            sx = -sx;
            n0 = n0 * -1.0f;
        }

        Mat4 rotationMatrix = Mat4::Identity();
        M(rotationMatrix, 0, 0) = n0.x;
        M(rotationMatrix, 1, 0) = n0.y;
        M(rotationMatrix, 2, 0) = n0.z;
        M(rotationMatrix, 0, 1) = n1.x;
        M(rotationMatrix, 1, 1) = n1.y;
        M(rotationMatrix, 2, 1) = n1.z;
        M(rotationMatrix, 0, 2) = n2.x;
        M(rotationMatrix, 1, 2) = n2.y;
        M(rotationMatrix, 2, 2) = n2.z;

        const Vec3 translation{
            M(input, 0, 3),
            M(input, 1, 3),
            M(input, 2, 3)
        };
        const Quat rotation =
            QuatFromRotationMatrix(rotationMatrix);
        const Vec3 scale{ sx, sy, sz };

        if (!IsFiniteMath(translation)
            || !IsFiniteMath(rotation)
            || !IsFiniteMath(scale))
        {
            return false;
        }

        const Mat4 recomposed =
            TRS(translation, rotation, scale);

        for (int i = 0; i < 16; ++i)
        {
            const float tolerance =
                5.0e-4f
                * (1.0f + std::fabs(input.m[i]));

            if (std::fabs(
                    recomposed.m[i] - input.m[i])
                > tolerance)
            {
                return false;
            }
        }

        outTranslation = translation;
        outRotation = rotation;
        outScale = scale;
        return true;
    }

    // ============================================================
    // Camera matrices
    // ============================================================

    inline Mat4 LookToLH(const Vec3& eye, const Vec3& dir, const Vec3& up)
    {
        const Vec3 zaxis = Normalize(dir);
        const Vec3 xaxis = Normalize(Cross(up, zaxis));
        const Vec3 yaxis = Cross(zaxis, xaxis);

        Mat4 r = Mat4::Identity();

        // Column-major storage + column vectors means the camera basis belongs
        // in matrix rows. This keeps CPU projection helpers and GPU rendering
        // on the same view transform.
        r.m[0] = xaxis.x; r.m[4] = xaxis.y; r.m[8]  = xaxis.z;
        r.m[1] = yaxis.x; r.m[5] = yaxis.y; r.m[9]  = yaxis.z;
        r.m[2] = zaxis.x; r.m[6] = zaxis.y; r.m[10] = zaxis.z;

        // translation
        r.m[12] = -Dot(xaxis, eye);
        r.m[13] = -Dot(yaxis, eye);
        r.m[14] = -Dot(zaxis, eye);

        return r;
    }

    // D3D-style LH perspective, depth 0..1
    inline Mat4 PerspectiveFovLH(float fovY, float aspect, float zn, float zf)
    {
        Mat4 r{};
        const float yScale = 1.0f / std::tan(fovY * 0.5f);
        const float xScale = yScale / aspect;

        r.m[0] = xScale;
        r.m[5] = yScale;
        r.m[10] = zf / (zf - zn);
        r.m[11] = 1.0f;
        r.m[14] = (-zn * zf) / (zf - zn);
        return r;
    }
}
