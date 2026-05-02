#pragma once

#include <cstdint>
#include "Common_Display.h"

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
	NativeDisplayInfo	displayInfo;
	uint32_t			width;
	uint32_t			height;
	uint32_t			format;
	uint32_t			bufferCount = 2;
	EPixelFormat		pixelFormat = EPixelFormat::R8G8B8A8_UNORM;
	bool				bIsWindowed = true;
};

enum class ECommandQueueType : uint8_t
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