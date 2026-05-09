#pragma once

#include "Interfaces/IActivate.h"
#include "Math/Container.h"

class BaseCamera : public IActivate
{
public:
	virtual ~BaseCamera() = default;

	virtual void Update(float dt) = 0;

	void SetEnabled(bool value) override { enabled = value; }
	bool IsEnabled()      const override { return enabled; }

	Matrix4x4 GetView()       const { return view; }
	Matrix4x4 GetProjection() const { return proj; }
	Vector3   GetPosition()   const { return position; }

protected:
	BaseCamera(float fovY, float aspect, float nearZ, float farZ);

	bool      enabled  = false;
	Vector3   position = { 0.0f, 1.5f, -3.0f };
	float     yaw      = 0.0f;
	float     pitch    = 0.0f;

	float     fovY;
	float     aspect;
	float     nearZ;
	float     farZ;

	Matrix4x4 view;
	Matrix4x4 proj;

	void RebuildView();
	void RebuildProj();
};
