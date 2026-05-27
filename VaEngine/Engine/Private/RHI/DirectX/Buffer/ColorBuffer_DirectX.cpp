#include "ColorBuffer_DirectX.h"
#include "RHI/DirectX/RenderDevice_DirectX.h"
#include "RHI/DirectX/CommandList_DirectX.h"
#include "RHI/DirectX/ResourceView_DirectX.h"

#include <stdexcept>

void ColorBuffer_DirectX::Create(IRenderDevice* device,
                                  EPixelFormat   format,
                                  uint32_t       width,
                                  uint32_t       height)
{
	auto* rdDevice  = static_cast<RenderDevice_DirectX*>(device);
	auto* d3dDevice = rdDevice->GetDevice();

	const DXGI_FORMAT dxgiFormat = static_cast<DXGI_FORMAT>(format);

	// 1. 리소스 생성 — ALLOW_RENDER_TARGET
	auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	auto resDesc   = CD3DX12_RESOURCE_DESC::Tex2D(
		dxgiFormat, width, height,
		1, 1, 1, 0,
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
	);

	D3D12_CLEAR_VALUE clearVal = {};
	clearVal.Format            = dxgiFormat;

	if (FAILED(d3dDevice->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&clearVal,
		IID_PPV_ARGS(&textureResource))))
	{
		throw std::runtime_error("Failed to create ColorBuffer resource");
	}

	// 2. RTV — 1-slot private heap (DepthBuffer_DirectX 패턴)
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors             = 1;
	heapDesc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heapDesc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	if (FAILED(d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeap))))
	{
		throw std::runtime_error("Failed to create RTV descriptor heap");
	}

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format             = dxgiFormat;
	rtvDesc.ViewDimension      = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
	d3dDevice->CreateRenderTargetView(textureResource.Get(), &rtvDesc, rtvHandle);

	ResourceViewDesc viewDesc = { EResourceViewType::RenderTargetView };
	rtvView = std::make_unique<ResourceView_DirectX>(viewDesc, rtvHandle, this);

	// 3. SRV — 전역 힙 (TextureUAV_DirectX 패턴)
	globalSrvHeap = rdDevice->GetGlobalSRVHeap();

	auto srvSlot = rdDevice->AllocateSRVDescriptor();
	srvGpuHandle = srvSlot.gpu;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format                  = dxgiFormat;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels     = 1;

	d3dDevice->CreateShaderResourceView(textureResource.Get(), &srvDesc, srvSlot.cpu);
}

void ColorBuffer_DirectX::BindSRV(ICommandList* cmdList, uint32_t slot, bool isCompute)
{
	auto* d3dCmd = static_cast<CommandList_DirectX*>(cmdList)->GetHandle();
	ID3D12DescriptorHeap* heaps[] = { globalSrvHeap };
	d3dCmd->SetDescriptorHeaps(1, heaps);
	if (isCompute)
		d3dCmd->SetComputeRootDescriptorTable(slot, srvGpuHandle);
	else
		d3dCmd->SetGraphicsRootDescriptorTable(slot, srvGpuHandle);
}
