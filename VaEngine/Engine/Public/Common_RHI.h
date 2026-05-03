#pragma once

#include <cstdint>

struct NativeDisplayInfo
{
	enum class EPlatform { None, Windows, Android, iOS, Max };

	EPlatform platform;
	union
	{
		// Windows : HWND (렌더링 대상 창 핸들)
		// Android : ANativeWindow* (android_app->window)
		void*      Handle;
		uintptr_t  RawData;
	};

	// Windows : HINSTANCE (현재 프로세스 모듈 핸들)
	// Android : AAssetManager* (android_app->activity->assetManager)
	//           셰이더 등 APK assets 파일을 읽기 위해 사용
	void* Display;
};

enum EPixelFormat : uint32_t
{
	Unknown = 0,
	R8G8B8A8_UNORM = 28,
	D24_UNORM_S8_UINT = 45,
	B8G8R8A8_UNORM = 87,
	Max
};

struct SwapChainDesc
{
	NativeDisplayInfo		displayInfo;
	class ICommandQueue*	RegisteredQueue;
	uint32_t				bufferCount;
	uint32_t				width;
	uint32_t				height;
	EPixelFormat			pixelFormat;
	bool					bIsWindowed;
};

enum class ECommandQueueType : uint32_t
{
	None = 0,
	Graphics = 1,
	Compute = 2,
	Copy = 3,
	Max
};

struct CommandQueueDesc
{
	uint32_t type;
	uint32_t flags;
};

struct CommandListDesc
{
	uint32_t type;
	uint32_t flags;
};