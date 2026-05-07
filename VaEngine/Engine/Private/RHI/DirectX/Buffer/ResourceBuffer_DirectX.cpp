#include "ResourceBuffer_DirectX.h"

#include <stdexcept>
#include <cstring>

void ResourceBuffer_DX12::Create(ID3D12Device* device, const ResourceBufferDesc& desc)
{
	D3D12_HEAP_PROPERTIES heapProps = {};
	switch (desc.access)
	{
	case EMemoryAccess::Upload:		heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;	break;
	case EMemoryAccess::Readback:	heapProps.Type = D3D12_HEAP_TYPE_READBACK;	break;
	default:						heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;	break;
	}

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension			= D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width				= desc.size;
	resourceDesc.Height				= 1;
	resourceDesc.DepthOrArraySize	= 1;
	resourceDesc.MipLevels			= 1;
	resourceDesc.Format				= DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count	= 1;
	resourceDesc.Layout				= D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags				= D3D12_RESOURCE_FLAG_NONE;

	D3D12_RESOURCE_STATES initialState = (desc.access == EMemoryAccess::Upload)
		? D3D12_RESOURCE_STATE_GENERIC_READ
		: D3D12_RESOURCE_STATE_COMMON;

	ID3D12Resource* rawResource = nullptr;
	if (FAILED(device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		initialState,
		nullptr,
		IID_PPV_ARGS(&rawResource)
	)))
	{
		throw std::runtime_error("Failed to create GPU buffer");
	}

	bufferResource = std::make_unique<TRHIResource<ID3D12Resource>>(
		rawResource,
		[](ID3D12Resource* r) { r->Release(); }
	);
}

void ResourceBuffer_DX12::Upload(const void* data, size_t size)
{
	ID3D12Resource* resource = bufferResource->GetResource();

	void* mappedData = nullptr;
	D3D12_RANGE readRange = { 0, 0 };
	if (FAILED(resource->Map(0, &readRange, &mappedData)))
	{
		throw std::runtime_error("Failed to map GPU buffer");
	}
	std::memcpy(mappedData, data, size);
	resource->Unmap(0, nullptr);
}

IRHIResource* ResourceBuffer_DX12::GetResource() const
{
	return bufferResource.get();
}
