#pragma once

#include "RHI/Common_RHI.h"

#include <memory>

class ApplicationManager;

class IExecute {
public:
	static std::unique_ptr<IExecute> Create(ApplicationManager* app);

	virtual ~IExecute() = default;
	
#pragma region Lifecycle
	virtual void OnInitialize(NativeDisplayInfo displayInfo) = 0;
	virtual void OnDestroy() = 0;

	virtual void OnLoop() = 0;
	virtual void OnSuspend() {}
	virtual void OnResume() {}
#pragma endregion Lifecycle
	
};