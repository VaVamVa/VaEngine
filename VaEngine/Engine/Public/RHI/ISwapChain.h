#pragma once

#include <cstdint>

class IRHIResource;
class IRenderDevice;
class IResourceView;
struct SwapChainDesc;

class ISwapChain
{
public:
	virtual ~ISwapChain() = default;

	virtual void Register(IRenderDevice* inDevice, const SwapChainDesc& desc) = 0;
	virtual void Present(bool bVsync) = 0;
	virtual void Resize(uint32_t width, uint32_t height) = 0;

	virtual IRHIResource* GetCurrentBackBuffer() const = 0;
	virtual IResourceView* GetCurrentBackBufferView() const = 0;

protected:
	virtual uint32_t GetCurrentBackBufferIndex() const = 0;
};