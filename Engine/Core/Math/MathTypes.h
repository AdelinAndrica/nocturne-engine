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
