#pragma once

#include <cstdint>

class ISwapChain
{
public:
	virtual ~ISwapChain() = default;

	virtual void Present() = 0;
	virtual void Resize(uint32_t width, uint32_t height) = 0;

	virtual uint32_t GetCurrentBackBufferIndex() const = 0;
};