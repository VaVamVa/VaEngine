#pragma once

#include "Manager/ApplicationManager.h"

#include <memory>

class CameraManager;
class WO_Cube;
class WO_Tower;
class WO_Kachujin;
class LightManager;

class VaProgramName : public ApplicationManager
{
public:
	void OnInitialize(IRenderDevice* device) override;
	void OnUpdate(float deltaTime) override;
	void SubmitRenderState(RenderScene* scene) override;
	void OnDestroy() override;

private:
	CameraManager*	cameraManager;
	LightManager* 	lightManager;

	WO_Cube*	  	cube;
	WO_Tower*		tower;
	WO_Kachujin*	kachujin;
	uint32_t		clipIndex = 0;

	float time = 0.0f;
};
