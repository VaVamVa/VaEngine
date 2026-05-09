#pragma once

#include "Scene/RenderScene.h"
#include "RHI/IRenderDevice.h"

class ApplicationManager
{
public:
	virtual ~ApplicationManager() = default;

	virtual void OnInitialize(IRenderDevice* device) {}
	virtual void OnUpdate(float deltaTime) = 0;
	virtual void SubmitRenderState(RenderScene* scene) = 0;
	virtual void OnDestroy() {}
};
