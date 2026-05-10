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
	D3D12_BLEND_DESC blendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	if (desc.blendMode == EBlendMode::AlphaBlend)
	{
		auto& rt                = blendDesc.RenderTarget[0];
		rt.BlendEnable          = TRUE;
		rt.SrcBlend             = D3D12_BLEND_SRC_ALPHA;
		rt.DestBlend            = D3D12_BLEND_INV_SRC_ALPHA;
		rt.BlendOp              = D3D12_BLEND_OP_ADD;
		rt.SrcBlendAlpha        = D3D12_BLEND_ONE;
		rt.DestBlendAlpha       = D3D12_BLEND_ZERO;
		rt.BlendOpAlpha         = D3D12_BLEND_OP_ADD;
		rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	}
	else if (desc.blendMode == EBlendMode::Additive)
	{
		auto& rt                = blendDesc.RenderTarget[0];
		rt.BlendEnable          = TRUE;
		rt.SrcBlend             = D3D12_BLEND_SRC_ALPHA;
		rt.DestBlend            = D3D12_BLEND_ONE;
		rt.BlendOp              = D3D12_BLEND_OP_ADD;
		rt.SrcBlendAlpha        = D3D12_BLEND_ONE;
		rt.DestBlendAlpha       = D3D12_BLEND_ZERO;
		rt.BlendOpAlpha         = D3D12_BLEND_OP_ADD;
		rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	}
	psoDesc.BlendState = blendDesc;
	psoDesc.DepthStencilState = desc.depthEnable
		? CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT)   // DepthEnable=TRUE, DepthFunc=LESS, WriteAll
		: CD3DX12_DEPTH_STENCIL_DESC();               // 전부 비활성
	psoDesc.DSVFormat = (desc.depthEnable && desc.dsvFormat != EPixelFormat::Unknown)
		? static_cast<DXGI_FORMAT>(desc.dsvFormat)
		: DXGI_FORMAT_UNKNOWN;
	psoDesc.SampleMask                          = UINT_MAX;
	switch (desc.topologyType)
	{
	case EPrimitiveTopologyType::Line:  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;  break;
	case EPrimitiveTopologyType::Point: psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT; break;
	default:                            psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; break;
	}
	psoDesc.NumRenderTargets                    = 1;
	psoDesc.RTVFormats[0]                       = static_cast<DXGI_FORMAT>(desc.rtvFormat);
	psoDesc.SampleDesc.Count                    = 1;

	if (FAILED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState))))
	{
		throw std::runtime_error("Failed to create pipeline state");
	}
}

void PipelineState_DirectX::Create(ID3D12Device* device, const ComputePipelineStateDesc& desc)
{
	isCompute = true;

	auto* shader  = static_cast<Shader_DirectX*>(desc.shader);
	auto* layout  = static_cast<BindingLayout_DirectX*>(desc.bindingLayout);

	if (shader == nullptr || !shader->IsCompute())
		throw std::runtime_error("ComputePipelineStateDesc.shader must be a compute shader (csPath set)");

	if (layout == nullptr || !layout->IsCompute())
		throw std::runtime_error("ComputePipelineStateDesc.bindingLayout must be created with isCompute=true");

	rootSignature = layout->GetRootSignature();

	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = rootSignature.Get();
	psoDesc.CS             = CD3DX12_SHADER_BYTECODE(shader->GetCS());
	psoDesc.NodeMask       = 0;
	psoDesc.Flags          = D3D12_PIPELINE_STATE_FLAG_NONE;

	if (FAILED(device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState))))
	{
		throw std::runtime_error("Failed to create compute pipeline state");
	}
}

void PipelineState_DirectX::Bind(ICommandList* cmdList)
{
	auto* d3d12Cmd = static_cast<CommandList_DirectX*>(cmdList)->GetHandle();
	if (isCompute)
	{
		d3d12Cmd->SetComputeRootSignature(rootSignature.Get());
	}
	else
	{
		d3d12Cmd->SetGraphicsRootSignature(rootSignature.Get());
	}
	d3d12Cmd->SetPipelineState(pipelineState.Get());
}
