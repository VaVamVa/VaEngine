#pragma once

#include "PipelineDesc.h"

class ICommandList;

class IPipelineState
{
public:
	virtual ~IPipelineState() = default;
	// DX12: SetGraphicsRootSignature + SetPipelineState
	// Vulkan: vkCmdBindPipeline
	virtual void Bind(ICommandList* cmdList) = 0;
};
