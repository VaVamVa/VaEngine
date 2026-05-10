#pragma once

#include "Manager/ApplicationManager.h"
#include "Camera/FreeCamera.h"

class CameraManager : public ApplicationManager
{
public:
	~CameraManager();

	void OnInitialize(IRenderDevice* device) override;
	void OnUpdate(float deltaTime) override;
	void SubmitRenderState(RenderScene* scene) override;
	void OnDestroy() override;

	Matrix4x4 GetView()       const;
	Matrix4x4 GetProjection() const;

private:
	std::unique_ptr<FreeCamera> camera;
};