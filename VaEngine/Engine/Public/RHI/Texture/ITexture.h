#pragma once

#include <cstdint>

class IRenderDevice;
class ICommandList;

class ITexture
{
public:
	virtual ~ITexture() = default;

	virtual void LoadFromFile(IRenderDevice* device, const char* path) = 0;
	virtual void LoadFromMemory(IRenderDevice* device, const void* pixels, uint32_t width, uint32_t height) = 0;

	// isCompute=true 시 SetComputeRootDescriptorTable, 기본은 graphics
	virtual void Bind(ICommandList* cmdList, uint32_t slot, bool isCompute = false) = 0;

	virtual bool HasAlpha() const = 0;
};
