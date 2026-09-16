#pragma once
#include "Core/Math/MathTypes.h"
#include "Runtime/Bounds.h"
#include <cmath>

namespace noc
{
	struct Plane
	{
		// ax + by + cz + d >= 0 is inside
		float a = 0, b = 0, c = 0, d = 0;
	};

	struct Frustum
	{
		// 0..5: left,right,bottom,top,near,far
		Plane p[6]{};
	};

	inline void NormalizePlane(Plane& pl)
	{
		const float len = std::sqrtf(pl.a * pl.a + pl.b * pl.b + pl.c * pl.c);
		if (len > 1e-6f)
		{
			const float inv = 1.0f / len;
			pl.a *= inv; pl.b *= inv; pl.c *= inv; pl.d *= inv;
		}
	}

	inline Frustum FrustumFromViewProj(const Mat4& m)
	{
		// Mat4 is stored column-major in m.m[col*4 + row]
		auto at = [&](int row, int col) -> float { return m.m[col * 4 + row]; };

		Frustum f{};

		// Left   = row3 + row0
		f.p[0] = Plane{ at(3,0) + at(0,0), at(3,1) + at(0,1), at(3,2) + at(0,2), at(3,3) + at(0,3) };
		// Right  = row3 - row0
		f.p[1] = Plane{ at(3,0) - at(0,0), at(3,1) - at(0,1), at(3,2) - at(0,2), at(3,3) - at(0,3) };
		// Bottom = row3 + row1
		f.p[2] = Plane{ at(3,0) + at(1,0), at(3,1) + at(1,1), at(3,2) + at(1,2), at(3,3) + at(1,3) };
		// Top    = row3 - row1
		f.p[3] = Plane{ at(3,0) - at(1,0), at(3,1) - at(1,1), at(3,2) - at(1,2), at(3,3) - at(1,3) };

		// D3D depth 0..1:
		// Near = row2
		f.p[4] = Plane{ at(2,0), at(2,1), at(2,2), at(2,3) };
		// Far  = row3 - row2
		f.p[5] = Plane{ at(3,0) - at(2,0), at(3,1) - at(2,1), at(3,2) - at(2,2), at(3,3) - at(2,3) };

		for (int i = 0; i < 6; ++i) NormalizePlane(f.p[i]);
		return f;
	}




	inline bool AabbInsidePlane(const AABB& a, const Plane& p)
	{
		// Positive vertex test
		Vec3 v;
		v.x = (p.a >= 0) ? a.max.x : a.min.x;
		v.y = (p.b >= 0) ? a.max.y : a.min.y;
		v.z = (p.c >= 0) ? a.max.z : a.min.z;

		const float dist = p.a * v.x + p.b * v.y + p.c * v.z + p.d;
		return dist >= 0.0f;
	}

	inline bool AabbIntersectsFrustum(const AABB& a, const Frustum& f)
	{
		for (int i = 0; i < 6; ++i)
		{
			if (!AabbInsidePlane(a, f.p[i]))
				return false;
		}
		return true;
	}
}
