#pragma once

#include <cstdint>

class IResourceView;

class ICommandList
{
public:
	virtual ~ICommandList() = default;

	virtual void Register(class IRenderDevice* device, const struct CommandListDesc& desc) = 0;

	// 명령 리스트에 명령을 기록하기 시작하는 함수
	virtual void Begin(class ICommandAlloc* inAllocator) = 0;

	// 명령 리스트에 명령 기록을 완료하는 함수
	virtual void Close() = 0;

#pragma region Commands
	// 상태 전이
	virtual void SetResourceBarrier(uint32_t numBarriers, const struct ResourceBarrier* inBarriers) = 0;

	// 화면을 특정 색으로 지우는 명령
	virtual void ClearRenderTargetView(IResourceView* inViewHandle, const float inColor[4]) = 0;

	// 뷰포트 설정
	// virtual void SetViewports(uint32_t inCount, const Viewport* inViewports) = 0;

#pragma endregion


};