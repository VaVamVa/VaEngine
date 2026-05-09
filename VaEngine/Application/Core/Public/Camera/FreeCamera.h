#pragma once

#include "Scene/BaseCamera.h"

class FreeCamera : public BaseCamera
{
public:
	FreeCamera(float fovY, float aspect, float nearZ, float farZ);

	void Update(float deltaTime) override;

private:
	float moveSpeed       = 5.0f;
	float lookSensitivity = 0.002f;
};
