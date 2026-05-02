#pragma once

#include <memory>
#include "Common_RHI.h"

class ISwapChain;
class ICommandQueue;
class ICommandList;
class IFence;

// Factory
class IRenderDevice
{
public:
	virtual ~IRenderDevice() = default;

	virtual void Initialize() = 0;
	virtual void Shutdown() = 0;

	virtual std::unique_ptr<ISwapChain>		CreateSwapChain(const SwapChainDesc& desc) = 0;
	virtual std::unique_ptr<ICommandQueue>	CreateCommandQueue(const CommandQueueDesc& desc) = 0;
	virtual std::unique_ptr<IFence>			CreateFence() = 0;
	virtual std::unique_ptr<ICommandList>	CreateCommandList(const CommandListDesc& desc) = 0;


};