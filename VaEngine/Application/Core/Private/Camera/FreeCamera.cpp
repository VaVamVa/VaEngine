#include "Camera/FreeCamera.h"

#include "System/InputSystem.h"
#include "Utilities/Locator.h"

#include <cmath>

FreeCamera::FreeCamera(float fovY, float aspect, float nearZ, float farZ)
	: BaseCamera(fovY, aspect, nearZ, farZ)
{
}

void FreeCamera::Update(float deltaTime)
{
	if (!enabled)
		return;

	InputSystem& input = Locator<InputSystem>::Get();

	yaw   += input.GetAxis("LookX") * lookSensitivity;
	pitch -= input.GetAxis("LookY") * lookSensitivity;
	pitch  = Math::Clamp(pitch, -Math::PI * 0.45f, Math::PI * 0.45f);

	const float fwdVal = input.GetAxis("MoveForward");
	const float rgtVal = input.GetAxis("MoveRight");
	const float upVal  = input.GetAxis("MoveUp");

	const float sy = std::sin(yaw), cy = std::cos(yaw);
	const float cp = std::cos(pitch);

	const Vector3 fwd = { sy * cp, std::sin(pitch), cy * cp };
	const Vector3 rgt = { cy, 0.0f, -sy };

	position += fwd         * (fwdVal * moveSpeed * deltaTime);
	position += rgt         * (rgtVal * moveSpeed * deltaTime);
	position += Vector3::Up * (upVal  * moveSpeed * deltaTime);

	RebuildView();
}
