#pragma once
#include <cstdint>

struct FNativeDisplayInfo
{
	enum class EPlatform { None, Windows, Android, iOS, Max };

	EPlatform platform;
	union
	{
		// HWND(Windows) || ANativeWindow(Android)
		void*		Handle;
		uint32_t	RawData;
	};

	// HINSTANCE(Windows) || nullptr(Android)
	void* Display;
};

class IExecute {
public:
	virtual ~IExecute() = default;
	
#pragma region Lifecycle
	virtual void OnInitialize(FNativeDisplayInfo displayInfo) = 0;
	virtual void OnDestroy() = 0;

	virtual void OnLoop() = 0;
	virtual void OnSuspend() {}
	virtual void OnResume() {}

protected:
	virtual void OnUpdate() = 0;
	virtual void OnRender() = 0;
	virtual void OnPostRender() = 0;
#pragma endregion Lifecycle
	
};