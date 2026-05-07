#pragma once

#include "RHI/Pipeline/IPipelineState.h"
#include "RHI/DirectX/Common_DirectX.h"

class PipelineState_DirectX : public IPipelineState
{
public:
	void Create(ID3D12Device* device, const PipelineStateDesc& desc);

	void Bind(ICommandList* cmdList) override;

private:
	ComPtr<ID3D12RootSignature> rootSignature;
	ComPtr<ID3D12PipelineState> pipelineState;
};
