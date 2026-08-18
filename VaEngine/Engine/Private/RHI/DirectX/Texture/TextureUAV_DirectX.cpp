#include "TextureUAV_DirectX.h"
#include "RHI/DirectX/RenderDevice_DirectX.h"
#include "RHI/DirectX/CommandList_DirectX.h"
#include "RHI/DirectX/ResourceView_DirectX.h"

#include <stdexcept>

void TextureUAV_DirectX::Create(IRenderDevice* device,
                                EPixelFormat   format,
                                uint32_t       width,
                                uint32_t       height,
                                uint32_t       arraySize,
                                uint32_t       mipLevels)
{
	auto* rdDevice = static_cast<RenderDevice_DirectX*>(device);
	ID3D12Device* d3dDevice = rdDevice->GetDevice();

	const DXGI_FORMAT dxgiFormat = static_cast<DXGI_FORMAT>(format);

	// 1. Default heap RWTexture2DArray 리소스 (UAV flag 필수)
	D3D12_RESOURCE_DESC texDesc = {};
	texDesc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Width              = width;
	texDesc.Height             = height;
	texDesc.DepthOrArraySize   = static_cast<UINT16>(arraySize);
	texDesc.MipLevels          = static_cast<UINT16>(mipLevels);
	texDesc.Format             = dxgiFormat;
	texDesc.SampleDesc.Count   = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags              = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE clearVal = {};
	clearVal.Format = dxgiFormat;
	clearVal.Color[0] = 0.f;
	clearVal.Color[1] = 0.f;
	clearVal.Color[2] = 0.f;
	clearVal.Color[3] = 1.f;
	auto defaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	if (FAILED(d3dDevice->CreateCommittedResource(
		&defaultHeap, D3D12_HEAP_FLAG_NONE, &texDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, &clearVal, IID_PPV_ARGS(&textureResource))))
	{
		throw std::runtime_error("Failed to create RWTexture resource");
	}
	SetTrackedState(EResourceState::UnorderedAccess);  // CreateCommittedResource가 실제로 이 상태로 생성

	globalSrvHeap = rdDevice->GetGlobalSRVHeap();

	// 2. UAV 생성 — 밉당 1개(D3D12 제약: UAV는 단일 서브리소스만 참조 가능)
	uavGpuHandles.resize(mipLevels);
	for (uint32_t mip = 0; mip < mipLevels; ++mip)
	{
		auto uavSlot = rdDevice->AllocateSRVDescriptor();
		uavGpuHandles[mip] = uavSlot.gpu;

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = dxgiFormat;
		if (arraySize > 1)
		{
			uavDesc.ViewDimension                  = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
			uavDesc.Texture2DArray.MipSlice        = mip;
			uavDesc.Texture2DArray.FirstArraySlice = 0;
			uavDesc.Texture2DArray.ArraySize       = arraySize;
		}
		else
		{
			uavDesc.ViewDimension      = D3D12_UAV_DIMENSION_TEXTURE2D;
			uavDesc.Texture2D.MipSlice = mip;
		}
		d3dDevice->CreateUnorderedAccessView(textureResource.Get(), nullptr, &uavDesc, uavSlot.cpu);
	}

	// 3. SRV 생성 (graphics/compute 셰이더가 결과를 읽을 때 사용) — 전체 밉 체인을 커버해
	//    SampleLevel로 밉 간 하드웨어 보간이 가능하게 한다.
	auto srvSlot = rdDevice->AllocateSRVDescriptor();
	srvGpuHandle = srvSlot.gpu;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format                  = dxgiFormat;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	if (arraySize > 1)
	{
		srvDesc.ViewDimension                  = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		srvDesc.Texture2DArray.MipLevels       = mipLevels;
		srvDesc.Texture2DArray.FirstArraySlice = 0;
		srvDesc.Texture2DArray.ArraySize       = arraySize;
	}
	else
	{
		srvDesc.ViewDimension       = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = mipLevels;
	}
	d3dDevice->CreateShaderResourceView(textureResource.Get(), &srvDesc, srvSlot.cpu);

	// 4. RTV — 1-slot private heap (SkyPass / TransparentPass에서 RT로 사용)
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors             = 1;
	rtvHeapDesc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	if (FAILED(d3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap))))
	{
		throw std::runtime_error("Failed to create RTV descriptor heap for TextureUAV");
	}

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format             = dxgiFormat;
	rtvDesc.ViewDimension      = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
	d3dDevice->CreateRenderTargetView(textureResource.Get(), &rtvDesc, rtvHandle);

	ResourceViewDesc rtvViewDesc = { EResourceViewType::RenderTargetView };
	rtvView = std::make_unique<ResourceView_DirectX>(rtvViewDesc, rtvHandle, this);
}

void TextureUAV_DirectX::BindUAV(ICommandList* cmdList, uint32_t slot, bool isCompute, uint32_t mipSlice)
{
	auto* d3dCmd = static_cast<CommandList_DirectX*>(cmdList)->GetHandle();
	ID3D12DescriptorHeap* heaps[] = { globalSrvHeap };
	d3dCmd->SetDescriptorHeaps(1, heaps);
	const D3D12_GPU_DESCRIPTOR_HANDLE& handle = uavGpuHandles[mipSlice];
	if (isCompute)
		d3dCmd->SetComputeRootDescriptorTable(slot, handle);
	else
		d3dCmd->SetGraphicsRootDescriptorTable(slot, handle);
}

void TextureUAV_DirectX::BindSRV(ICommandList* cmdList, uint32_t slot, bool isCompute)
{
	auto* d3dCmd = static_cast<CommandList_DirectX*>(cmdList)->GetHandle();
	ID3D12DescriptorHeap* heaps[] = { globalSrvHeap };
	d3dCmd->SetDescriptorHeaps(1, heaps);
	if (isCompute)
		d3dCmd->SetComputeRootDescriptorTable(slot, srvGpuHandle);
	else
		d3dCmd->SetGraphicsRootDescriptorTable(slot, srvGpuHandle);
}
