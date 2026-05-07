#pragma once

#include <cstddef>
#include <cstdint>

class ICommandList;

class IConstantBuffer
{
public:
	virtual ~IConstantBuffer() = default;

	virtual void SetData(const void* data, size_t size) = 0;
	// DX12: SetGraphicsRootConstantBufferView(slot, gpuAddress)
	// Vulkan: vkUpdateDescriptorSets + descriptor set bind
	virtual void Bind(ICommandList* cmdList, uint32_t slot) = 0;
};
