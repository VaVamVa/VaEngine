#include "PipelineState_DirectX.h"
#include "BindingLayout_DirectX.h"
#include "RHI/DirectX/CommandList_DirectX.h"

#include <d3dcompiler.h>
#include <stdexcept>
#include <vector>

void PipelineState_DirectX::Create(ID3D12Device* device, const PipelineStateDesc& desc)
{
	// 셰이더 컴파일
	UINT compileFlags = 0;
#if defined(_DEBUG)
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	ComPtr<ID3DBlob> vs, vsErr;
	if (FAILED(D3DCompileFromFile(desc.vsPath, nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vs, &vsErr)))
	{
		std::string msg = vsErr ? static_cast<const char*>(vsErr->GetBufferPointer()) : "File not found";
		throw std::runtime_error("Failed to compile vertex shader: " + msg);
	}

	ComPtr<ID3DBlob> ps, psErr;
	if (FAILED(D3DCompileFromFile(desc.psPath, nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &ps, &psErr)))
	{
		std::string msg = psErr ? static_cast<const char*>(psErr->GetBufferPointer()) : "File not found";
		throw std::runtime_error("Failed to compile pixel shader: " + msg);
	}

	// 정점 입력 레이아웃
	std::vector<D3D12_INPUT_ELEMENT_DESC> inputElems(desc.vertexInputCount);
	for (uint32_t i = 0; i < desc.vertexInputCount; ++i)
	{
		inputElems[i] = {
			desc.vertexInputs[i].semantic,
			desc.vertexInputs[i].semanticIndex,
			static_cast<DXGI_FORMAT>(desc.vertexInputs[i].format),
			0,
			desc.vertexInputs[i].byteOffset,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		};
	}

	// 래스터라이저
	auto rasterDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	switch (desc.cullMode)
	{
	case ECullMode::None:  rasterDesc.CullMode = D3D12_CULL_MODE_NONE;  break;
	case ECullMode::Front: rasterDesc.CullMode = D3D12_CULL_MODE_FRONT; break;
	case ECullMode::Back:  rasterDesc.CullMode = D3D12_CULL_MODE_BACK;  break;
	}

	// Root Signature는 BindingLayout에서 가져와 저장 (ComPtr AddRef)
	rootSignature = static_cast<BindingLayout_DirectX*>(desc.bindingLayout)->GetRootSignature();

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout             = { inputElems.data(), (UINT)inputElems.size() };
	psoDesc.pRootSignature          = rootSignature.Get();
	psoDesc.VS                      = CD3DX12_SHADER_BYTECODE(vs.Get());
	psoDesc.PS                      = CD3DX12_SHADER_BYTECODE(ps.Get());
	psoDesc.RasterizerState         = rasterDesc;
	psoDesc.BlendState              = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState.DepthEnable   = desc.depthEnable ? TRUE : FALSE;
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.SampleMask              = UINT_MAX;
	psoDesc.PrimitiveTopologyType   = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets        = 1;
	psoDesc.RTVFormats[0]           = static_cast<DXGI_FORMAT>(desc.rtvFormat);
	psoDesc.SampleDesc.Count        = 1;

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
