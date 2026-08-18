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
	float     GetYaw()        const { return yaw; }
	float     GetPitch()      const { return pitch; }

	// 투영 파라미터 — Debug/GUI에서 런타임 조정 가능하도록 노출(예: CSM near/far 튜닝).
	// Setter는 값 변경 즉시 proj 행렬을 재계산한다.
	float GetNearZ()  const { return nearZ; }
	float GetFarZ()   const { return farZ; }
	float GetFovY()   const { return fovY; }
	float GetAspect() const { return aspect; }

	void SetNearZ(float value)  { nearZ  = value; RebuildProj(); }
	void SetFarZ(float value)   { farZ   = value; RebuildProj(); }
	void SetFovY(float value)   { fovY   = value; RebuildProj(); }
	void SetAspect(float value) { aspect = value; RebuildProj(); }

	Vector3 SetPosition(const Vector3& pos);

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
