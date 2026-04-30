#pragma once

class IExecute {
public:
	virtual ~IExecute() = default;
	virtual void Execute() = 0;

#pragma region Lifecycle
	virtual void OnInitialize() = 0;
	virtual void OnUpdate() = 0;
	virtual void OnRender() = 0;
	virtual void OnPostRender() = 0;
	virtual void OnDestroy() = 0;

	virtual void OnSuspend() {}
	virtual void OnResume() {}
#pragma endregion Lifecycle
};