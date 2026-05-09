#include "PipelineState_DirectX.h"
#include "BindingLayout_DirectX.h"
#include "RHI/DirectX/CommandList_DirectX.h"
#include "RHI/DirectX/Shader/Shader_DirectX.h"

#include <stdexcept>
#include <vector>

void PipelineState_DirectX::Create(ID3D12Device* device, const PipelineStateDesc& desc)
{
	auto* shader = static_cast<Shader_DirectX*>(desc.shader);

	std::vector<D3D12_INPUT_ELEMENT_DESC> inputElems(desc.vertexInputCount);
	for (uint32_t i = 0; i < desc.vertexInputCount; ++i)
	{
		inputElems[i] = {
			desc.vertexInputs[i].semantic,
			desc.vertexInputs[i].semanticIndex,
			static_cast<DXGI_FORMAT>(desc.vertexInputs[i].format),
			desc.vertexInputs[i].inputSlot,
			desc.vertexInputs[i].byteOffset,
			desc.vertexInputs[i].perInstance
				? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA
				: D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			desc.vertexInputs[i].perInstance ? 1u : 0u
		};
	}

	auto rasterDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	switch (desc.cullMode)
	{
	case ECullMode::None:  rasterDesc.CullMode = D3D12_CULL_MODE_NONE;  break;
	case ECullMode::Front: rasterDesc.CullMode = D3D12_CULL_MODE_FRONT; break;
	case ECullMode::Back:  rasterDesc.CullMode = D3D12_CULL_MODE_BACK;  break;
	}

	rootSignature = static_cast<BindingLayout_DirectX*>(desc.bindingLayout)->GetRootSignature();

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout                         = { inputElems.data(), (UINT)inputElems.size() };
	psoDesc.pRootSignature                      = rootSignature.Get();
	psoDesc.VS                                  = CD3DX12_SHADER_BYTECODE(shader->GetVS());
	psoDesc.PS                                  = CD3DX12_SHADER_BYTECODE(shader->GetPS());
	psoDesc.RasterizerState                     = rasterDesc;
	psoDesc.BlendState                          = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = desc.depthEnable
		? CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT)   // DepthEnable=TRUE, DepthFunc=LESS, WriteAll
		: CD3DX12_DEPTH_STENCIL_DESC();               // 전부 비활성
	psoDesc.DSVFormat = (desc.depthEnable && desc.dsvFormat != EPixelFormat::Unknown)
		? static_cast<DXGI_FORMAT>(desc.dsvFormat)
		: DXGI_FORMAT_UNKNOWN;
	psoDesc.SampleMask                          = UINT_MAX;
	psoDesc.PrimitiveTopologyType               = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets                    = 1;
	psoDesc.RTVFormats[0]                       = static_cast<DXGI_FORMAT>(desc.rtvFormat);
	psoDesc.SampleDesc.Count                    = 1;

	if (FAILED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState))))
	{
		throw std::runtime_error("Failed to create pipeline state");
	}
}

void PipelineState_DirectX::Bind(ICommandList* cmdList)
{
	auto* d3d12Cmd = static_cast<CommandList_DirectX*>(cmdList)->GetHandle();
	d3d12Cmd->SetGraphicsRootSignature(rootSignature.Get());
	d3d12Cmd->SetPipelineState(pipelineState.Get());
}
