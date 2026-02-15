#pragma once
#include "Core/Math/MathTypes.h"

namespace noc
{
	struct AABB
	{
		Vec3 min;
		Vec3 max;
	};

	inline AABB AabbInvalid()
	{
		return AABB{ Vec3(1e30f, 1e30f, 1e30f), Vec3(-1e30f,-1e30f,-1e30f) };
	}

	inline void AabbExpand(AABB& a, const Vec3& p)
	{
		if (p.x < a.min.x) a.min.x = p.x;
		if (p.y < a.min.y) a.min.y = p.y;
		if (p.z < a.min.z) a.min.z = p.z;
		if (p.x > a.max.x) a.max.x = p.x;
		if (p.y > a.max.y) a.max.y = p.y;
		if (p.z > a.max.z) a.max.z = p.z;
	}

	inline AABB TransformAabb(const AABB& local, const Mat4& world)
	{
		// Conservative: transform all 8 corners.
		const Vec3 c[8] = {
			{local.min.x, local.min.y, local.min.z},
			{local.max.x, local.min.y, local.min.z},
			{local.min.x, local.max.y, local.min.z},
			{local.max.x, local.max.y, local.min.z},
			{local.min.x, local.min.y, local.max.z},
			{local.max.x, local.min.y, local.max.z},
			{local.min.x, local.max.y, local.max.z},
			{local.max.x, local.max.y, local.max.z},
		};

		AABB out = AabbInvalid();
		for (int i = 0; i < 8; ++i)
			AabbExpand(out, TransformPoint(world, c[i]));
		return out;
	}
}
