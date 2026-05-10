#pragma once

#include "RHI/Pipeline/IPipelineState.h"
#include "RHI/Pipeline/ComputePipelineDesc.h"
#include "RHI/DirectX/Common_DirectX.h"

class PipelineState_DirectX : public IPipelineState
{
public:
	void Create(ID3D12Device* device, const PipelineStateDesc& desc);
	void Create(ID3D12Device* device, const ComputePipelineStateDesc& desc);

	void Bind(ICommandList* cmdList) override;
	bool IsCompute() const override { return isCompute; }

private:
	ComPtr<ID3D12RootSignature> rootSignature;
	ComPtr<ID3D12PipelineState> pipelineState;
	bool                        isCompute = false;
};
