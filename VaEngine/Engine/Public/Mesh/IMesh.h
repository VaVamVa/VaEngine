#pragma once

#include <memory>

class IRenderDevice;
class ICommandList;

class IMesh
{
public:
	virtual ~IMesh() = default;

	virtual void Initialize(IRenderDevice* device) = 0;
	// DX12: IASetVertexBuffers + IASetIndexBuffer + DrawIndexedInstanced
	// Vulkan: vkCmdBindVertexBuffers + vkCmdBindIndexBuffer + vkCmdDrawIndexed
	virtual void Draw(ICommandList* cmdList) = 0;
};
