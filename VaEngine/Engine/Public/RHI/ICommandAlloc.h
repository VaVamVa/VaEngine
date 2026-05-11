#pragma once

class ICommandAlloc
{
public:
	virtual ~ICommandAlloc() = default;

	virtual void Register(class IRenderDevice* device, const CommandAllocDesc& desc) = 0;

	// 할당된 명령 리스트를 재사용하기 위해 리셋하는 함수
	// (단, GPU에서 실행 중인 명령 리스트가 없는 경우에만 호출해야 합니다.)
	virtual void Reset() = 0;
};