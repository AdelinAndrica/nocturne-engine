#pragma once
#include <cmath>
#include <cstdint>

namespace noc
{
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

		friend constexpr Vec3 operator+(const Vec3& a, const Vec3& b)
		{
			return { a.x + b.x, a.y + b.y, a.z + b.z };
		}

		friend constexpr Vec3 operator-(const Vec3& a, const Vec3& b)
		{
			return { a.x - b.x, a.y - b.y, a.z - b.z };
		}

		friend constexpr Vec3 operator*(const Vec3& v, float s)
		{
			return { v.x * s, v.y * s, v.z * s };
		}

		friend constexpr Vec3 operator*(float s, const Vec3& v)
		{
			return v * s;
		}
	};

	inline float Dot(const Vec3& a, const Vec3& b)
	{
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}

	inline Vec3 Cross(const Vec3& a, const Vec3& b)
	{
		return {
			a.y * b.z - a.z * b.y,
			a.z * b.x - a.x * b.z,
			a.x * b.y - a.y * b.x
		};
	}

	inline float LengthSq(const Vec3& v)
	{
		return Dot(v, v);
	}

	inline float Length(const Vec3& v)
	{
		return std::sqrt(LengthSq(v));
	}

	inline Vec3 Normalize(const Vec3& v)
	{
		float len = Length(v);
		if (len <= 1e-6f) return Vec3::Zero();
		return v * (1.0f / len);
	}

	// ============================================================
	// Quaternion
	// ============================================================

	struct Quat
	{
		float x{}, y{}, z{}, w{ 1.0f };

		constexpr Quat() = default;
		constexpr Quat(float X, float Y, float Z, float W)
			: x(X), y(Y), z(Z), w(W) {
		}

		static constexpr Quat Identity()
		{
			return { 0,0,0,1 };
		}
	};

	// ============================================================
	// Mat4 (ROW-MAJOR)
	// ============================================================

	struct Mat4
	{
		// Row-major 4x4
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

		float& operator()(int r, int c)
		{
			return m[r * 4 + c];
		}

		const float& operator()(int r, int c) const
		{
			return m[r * 4 + c];
		}
	};

	// ============================================================
	// Matrix multiply (row-major)
	// ============================================================

	inline Mat4 Mul(const Mat4& a, const Mat4& b)
	{
		Mat4 r{};

		for (int i = 0; i < 4; ++i)
		{
			for (int j = 0; j < 4; ++j)
			{
				float s = 0.0f;
				for (int k = 0; k < 4; ++k)
					s += a(i, k) * b(k, j);
				r(i, j) = s;
			}
		}

		return r;
	}

	// ============================================================
	// Transform helpers
	// ============================================================

	inline Vec3 TransformPoint(const Mat4& m, const Vec3& p)
	{
		return {
			p.x * m(0,0) + p.y * m(0,1) + p.z * m(0,2) + m(0,3),
			p.x * m(1,0) + p.y * m(1,1) + p.z * m(1,2) + m(1,3),
			p.x * m(2,0) + p.y * m(2,1) + p.z * m(2,2) + m(2,3)
		};
	}

	inline Mat4 Translation(const Vec3& t)
	{
		Mat4 r = Mat4::Identity();
		r(0, 3) = t.x;
		r(1, 3) = t.y;
		r(2, 3) = t.z;
		return r;
	}

	inline Mat4 Scale(const Vec3& s)
	{
		Mat4 r = Mat4::Identity();
		r(0, 0) = s.x;
		r(1, 1) = s.y;
		r(2, 2) = s.z;
		return r;
	}

	inline Mat4 RotationFromQuat(const Quat& q)
	{
		const float x2 = q.x + q.x;
		const float y2 = q.y + q.y;
		const float z2 = q.z + q.z;

		const float xx = q.x * x2;
		const float yy = q.y * y2;
		const float zz = q.z * z2;
		const float xy = q.x * y2;
		const float xz = q.x * z2;
		const float yz = q.y * z2;
		const float wx = q.w * x2;
		const float wy = q.w * y2;
		const float wz = q.w * z2;

		Mat4 r = Mat4::Identity();

		r(0, 0) = 1.0f - (yy + zz);
		r(0, 1) = xy - wz;
		r(0, 2) = xz + wy;

		r(1, 0) = xy + wz;
		r(1, 1) = 1.0f - (xx + zz);
		r(1, 2) = yz - wx;

		r(2, 0) = xz - wy;
		r(2, 1) = yz + wx;
		r(2, 2) = 1.0f - (xx + yy);

		return r;
	}

	inline Mat4 TRS(const Vec3& t, const Quat& r, const Vec3& s)
	{
		return Mul(Mul(Translation(t), RotationFromQuat(r)), Scale(s));
	}

	// ============================================================
	// Camera
	// ============================================================

	inline Mat4 LookToLH(const Vec3& eye, const Vec3& dir, const Vec3& up)
	{
		const Vec3 zaxis = Normalize(dir);
		const Vec3 xaxis = Normalize(Cross(up, zaxis));
		const Vec3 yaxis = Cross(zaxis, xaxis);

		Mat4 m = Mat4::Identity();

		m(0, 0) = xaxis.x; m(0, 1) = xaxis.y; m(0, 2) = xaxis.z; m(0, 3) = -Dot(xaxis, eye);
		m(1, 0) = yaxis.x; m(1, 1) = yaxis.y; m(1, 2) = yaxis.z; m(1, 3) = -Dot(yaxis, eye);
		m(2, 0) = zaxis.x; m(2, 1) = zaxis.y; m(2, 2) = zaxis.z; m(2, 3) = -Dot(zaxis, eye);

		return m;
	}

	inline Mat4 PerspectiveFovLH(float fovY, float aspect, float zn, float zf)
	{
		Mat4 r{};

		const float yScale = 1.0f / std::tan(fovY * 0.5f);
		const float xScale = yScale / aspect;

		r(0, 0) = xScale;
		r(1, 1) = yScale;
		r(2, 2) = zf / (zf - zn);
		r(2, 3) = 1.0f;
		r(3, 2) = (-zn * zf) / (zf - zn);

		return r;
	}
}
