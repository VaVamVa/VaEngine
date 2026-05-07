#pragma once

#include <cstdint>
#include "Common_RHI.h"

class IResourceView;
class IResourceBuffer;

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

	// 렌더 타겟 바인딩 (Vulkan: render pass attachment, DX12: OMSetRenderTargets)
	virtual void SetRenderTarget(IResourceView* rtv) = 0;

	// 뷰포트 설정
	virtual void SetViewport(float x, float y, float width, float height, float minDepth, float maxDepth) = 0;

	// 시저 렉트 설정
	virtual void SetScissorRect(int32_t left, int32_t top, int32_t right, int32_t bottom) = 0;

	// 버텍스 버퍼 바인딩 (DX12: IASetVertexBuffers, Vulkan: vkCmdBindVertexBuffers)
	virtual void SetVertexBuffer(IResourceBuffer* vb, uint32_t stride, uint32_t totalSize) = 0;

	// 인덱스 버퍼 바인딩 (DX12: IASetIndexBuffer, Vulkan: vkCmdBindIndexBuffer)
	virtual void SetIndexBuffer(IResourceBuffer* ib, EIndexFormat format, uint32_t totalSize) = 0;

	// 인덱스 드로우 (DX12: DrawIndexedInstanced, Vulkan: vkCmdDrawIndexed)
	virtual void DrawIndexed(uint32_t indexCount, uint32_t startIndex = 0, int32_t baseVertex = 0) = 0;

#pragma endregion


};