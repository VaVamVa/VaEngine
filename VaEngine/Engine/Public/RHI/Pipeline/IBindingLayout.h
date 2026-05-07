#pragma once

#include "PipelineDesc.h"

class IBindingLayout
{
public:
	virtual ~IBindingLayout() = default;
	// DX12: ID3D12RootSignature
	// Vulkan: VkDescriptorSetLayout + VkPipelineLayout
};
