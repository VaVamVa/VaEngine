#pragma once

#include "PipelineDesc.h"

class ICommandList;

class IPipelineState
{
public:
	virtual ~IPipelineState() = default;
	// DX12: graphics PSO → SetGraphicsRootSignature+SetPipelineState
	//       compute PSO  → SetComputeRootSignature +SetPipelineState
	// Vulkan: vkCmdBindPipeline (bind point가 GRAPHICS / COMPUTE 분기)
	virtual void Bind(ICommandList* cmdList) = 0;

	// 호출부에서 PSO 종류 검사용 (디버그 단정 등). 그래픽스 호출 → false, 컴퓨트 → true.
	virtual bool IsCompute() const = 0;
};
