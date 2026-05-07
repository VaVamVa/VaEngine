#pragma once

#include "RHI/Common_RHI.h"

class IExecute {
public:
	virtual ~IExecute() = default;
	
#pragma region Lifecycle
	virtual void OnInitialize(NativeDisplayInfo displayInfo) = 0;
	virtual void OnDestroy() = 0;

	virtual void OnLoop() = 0;
	virtual void OnSuspend() {}
	virtual void OnResume() {}

protected:
	virtual void OnRelease() = 0;

	virtual void OnPreUpdate() = 0;
	virtual void OnUpdate() = 0;
	virtual void OnPreRender() = 0;
	virtual void OnRender() = 0;
	virtual void OnPostRender() = 0;
#pragma endregion Lifecycle
	
};