#pragma once

#include <memory>
#include "Common_RHI.h"

#include "RHI/ICommandQueue.h"
#include "RHI/IFence.h"
#include "RHI/ISwapChain.h"
#include "RHI/ICommandAlloc.h"
#include "RHI/ICommandList.h"

/*
RenderDevice는 그래픽 API와 상호 작용하는 핵심 클래스입니다.
이 클래스는 DirectX 12 장치(ID3D12Device)를 관리하고, 스왑 체인, 명령 큐, 명령 리스트, 펜스 등의 리소스를 생성하는 팩토리의 역할을 합니다.
*/
class IRenderDevice
{
public:
	virtual ~IRenderDevice() = default;

	virtual void Initialize() = 0;
	virtual void Shutdown() = 0;

	[[nodiscard]] virtual std::unique_ptr<ICommandQueue>	CreateCommandQueue(const CommandQueueDesc& desc) = 0;
	[[nodiscard]] virtual std::unique_ptr<IFence>			CreateFence() = 0;
	[[nodiscard]] virtual std::unique_ptr<ISwapChain>		CreateSwapChain(const SwapChainDesc& desc) = 0;
	[[nodiscard]] virtual std::unique_ptr<ICommandAlloc>	CreateCommandAllocator(const CommandAllocDesc& desc) = 0;
	[[nodiscard]] virtual std::unique_ptr<ICommandList>		CreateCommandList(const CommandListDesc& desc) = 0;

};