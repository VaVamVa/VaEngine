#pragma once

#include <vector>
#include <cstdint>
#include "Common_RHI.h"

class IFence;
class ICommandList;
class IRenderDevice;

/*
큐는 GPU에 명령을 제출하는 역할을 합니다.
일반적으로 그래픽, 컴퓨트, 복사 명령 큐로 구분됩니다.
각 큐는 특정 유형의 명령을 처리하도록 설계되어 있습니다.
그래픽 명령 큐는 렌더링 명령을, 컴퓨트 명령 큐는 계산 작업을, 복사 명령 큐는 리소스 간 데이터 전송을 처리합니다.
이러한 큐를 통해 CPU는 GPU에 작업을 효율적으로 전달할 수 있으며, 동시에 여러 작업을 병렬로 처리할 수 있습니다.*/
class ICommandQueue
{
public:
	virtual ~ICommandQueue() = default;

	virtual void Register(IRenderDevice* inDevice, const CommandQueueDesc& inDesc) = 0;

	virtual void ExecuteCommandLists(const std::vector<ICommandList*>& commandLists) = 0;
	// GPU가 작업이 끝나면 Fence의 값을 Value로 올리라는 신호를 보내는 함수
	virtual void Signal(IFence* fence, uint64_t value) = 0;
	// GPU에게 Fence의 값이 Value에 도달할 때까지 기다리고 있는다는 신호를 보내는 함수
	virtual void Wait(IFence* fence, uint64_t value) = 0;
};