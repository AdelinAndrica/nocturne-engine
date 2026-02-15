#pragma once
#include "Core/Math/MathTypes.h"

namespace noc
{
	struct Camera
	{
		float fovYRadians = 1.04719755f; // ~60 deg
		float aspect = 16.0f / 9.0f;
		float nearZ = 0.1f;
		float farZ = 500.0f;

		Mat4 view = Mat4::Identity();
		Mat4 proj = Mat4::Identity();
		Mat4 viewProj = Mat4::Identity();

		void Rebuild(const Vec3& eye, const Vec3& forward, const Vec3& up)
		{
			view = LookToLH(eye, forward, up);
			proj = PerspectiveFovLH(fovYRadians, aspect, nearZ, farZ);
			viewProj = Mul(proj, view);
		}
	};
}
