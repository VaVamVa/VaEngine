#pragma once

class ICommandList;

class IMesh
{
public:
	virtual ~IMesh() = default;

	// DX12: IASetVertexBuffers + IASetIndexBuffer + DrawIndexedInstanced(1)
	// Vulkan: vkCmdBindVertexBuffers + vkCmdBindIndexBuffer + vkCmdDrawIndexed
	virtual void Draw(ICommandList* cmdList) = 0;

	// instanceCount > 1 인스턴스 드로우 (slot 1 인스턴스 버퍼는 호출자가 미리 바인딩)
	virtual void DrawInstanced(ICommandList* cmdList, uint32_t instanceCount) = 0;
};
