#include "Buffer_DirectX.h"

#include <stdexcept>

void Buffer_DirectX::Create(ID3D12Device* device, const BufferDesc& desc)
{
	size_t allocSize = static_cast<size_t>(desc.size);

	// CBV는 256바이트 정렬 필요
	if (static_cast<uint32_t>(desc.usage) & static_cast<uint32_t>(EBufferUsage::ConstantBuffer))
		allocSize = (allocSize + 255) & ~size_t(255);

	D3D12_HEAP_PROPERTIES heapProps = {};
	switch (desc.access)
	{
		case EMemoryAccess::Upload:   heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;   break;
		case EMemoryAccess::Readback: heapProps.Type = D3D12_HEAP_TYPE_READBACK; break;
		default:                      heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;  break;
	}

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width              = allocSize;
	resourceDesc.Height             = 1;
	resourceDesc.DepthOrArraySize   = 1;
	resourceDesc.MipLevels          = 1;
	resourceDesc.Format             = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count   = 1;
	resourceDesc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags              = D3D12_RESOURCE_FLAG_NONE;

	D3D12_RESOURCE_STATES initialState = (desc.access == EMemoryAccess::Upload)
		? D3D12_RESOURCE_STATE_GENERIC_READ
		: D3D12_RESOURCE_STATE_COMMON;

	if (FAILED(device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		initialState,
		nullptr,
		IID_PPV_ARGS(&bufferResource)
	)))
	{
		throw std::runtime_error("Failed to create buffer");
	}
}

void* Buffer_DirectX::Map()
{
	void* mappedData = nullptr;
	D3D12_RANGE readRange = { 0, 0 };
	if (FAILED(bufferResource->Map(0, &readRange, &mappedData)))
		throw std::runtime_error("Failed to map buffer");
	return mappedData;
}

void Buffer_DirectX::Unmap()
{
	bufferResource->Unmap(0, nullptr);
}

void* Buffer_DirectX::GetNativeResource() const
{
	return bufferResource.Get();
}
