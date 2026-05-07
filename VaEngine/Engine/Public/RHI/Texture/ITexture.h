#pragma once

#include <cstdint>

class IRenderDevice;
class ICommandList;

class ITexture
{
public:
	virtual ~ITexture() = default;

	virtual void LoadFromFile(IRenderDevice* device, const wchar_t* path) = 0;
	virtual void LoadFromMemory(IRenderDevice* device, const void* pixels, uint32_t width, uint32_t height) = 0;
	virtual void Bind(ICommandList* cmdList, uint32_t slot) = 0;
};
