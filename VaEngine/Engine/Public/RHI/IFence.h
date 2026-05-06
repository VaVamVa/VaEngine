#pragma once

#include <cstdint>

class IRenderDevice;

/*
Fence는 GPU와 CPU 간의 동기화를 위한 객체입니다.
GPU가 특정 작업을 완료했는지 여부를 확인하거나, CPU가 GPU의 작업이 완료될 때까지 기다리는 데 사용됩니다.
예를 들어, CPU가 GPU에 명령을 제출한 후, 해당 명령이 완료될 때까지 기다려야 하는 경우에 Fence를 사용할 수 있습니다.
Fence는 일반적으로 GPU에서 특정 시점에 신호를 보내고, CPU는 해당 신호를 기다리는 방식으로 동작합니다.
이를 통해 CPU와 GPU 간의 효율적인 작업 분배와 동기화를 가능하게 합니다.
간단하게 말해서 "Flag" 같은 역할을 하는 객체입니다.
*/
class IFence
{
public:
	virtual ~IFence() = default;

	virtual void		Register(IRenderDevice* inDevice) = 0;
	virtual void		SetEventOnComplete(uint64_t value, void* inEventHandle) = 0;
	virtual uint64_t	GetCompletedValue() const = 0;
};