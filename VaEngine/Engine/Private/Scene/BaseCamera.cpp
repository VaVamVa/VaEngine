#include "Scene/BaseCamera.h"

#include <cmath>

BaseCamera::BaseCamera(float fovY, float aspect, float nearZ, float farZ)
	: fovY(fovY), aspect(aspect), nearZ(nearZ), farZ(farZ)
{
	RebuildProj();
	RebuildView();
}

void BaseCamera::RebuildView()
{
	const float sy = std::sin(yaw), cy = std::cos(yaw);
	const float cp = std::cos(pitch);
	const Vector3 target = position + Vector3{ sy * cp, std::sin(pitch), cy * cp };
	view = Matrix4x4::LookAtLH(position, target, Vector3::Up);
}

void BaseCamera::RebuildProj()
{
	proj = Matrix4x4::PerspectiveFovLH(fovY, aspect, nearZ, farZ);
}
