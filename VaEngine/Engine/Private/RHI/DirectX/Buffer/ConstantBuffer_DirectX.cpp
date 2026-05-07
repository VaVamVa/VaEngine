#include "ConstantBuffer_DirectX.h"
#include "RHI/DirectX/CommandList_DirectX.h"

#include <stdexcept>
#include <cstring>

void ConstantBuffer_DirectX::Create(ID3D12Device* device, size_t size)
{
	// DX12 CBV는 256바이트 정렬 필요
	size_t alignedSize = (size + 255) & ~static_cast<size_t>(255);

	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension			= D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width				= alignedSize;
	resourceDesc.Height				= 1;
	resourceDesc.DepthOrArraySize	= 1;
	resourceDesc.MipLevels			= 1;
	resourceDesc.Format				= DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count	= 1;
	resourceDesc.Layout				= D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags				= D3D12_RESOURCE_FLAG_NONE;

	ID3D12Resource* rawResource = nullptr;
	if (FAILED(device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&rawResource)
	)))
	{
		throw std::runtime_error("Failed to create constant buffer");
	}

	bufferResource = std::make_unique<TRHIResource<ID3D12Resource>>(
		rawResource,
		[](ID3D12Resource* r) { r->Release(); }
	);
}

void ConstantBuffer_DirectX::SetData(const void* data, size_t size)
{
	void* mappedData = nullptr;
	D3D12_RANGE readRange = { 0, 0 };
	if (FAILED(bufferResource->GetResource()->Map(0, &readRange, &mappedData)))
	{
		throw std::runtime_error("Failed to map constant buffer");
	}
	std::memcpy(mappedData, data, size);
	bufferResource->GetResource()->Unmap(0, nullptr);
}

void ConstantBuffer_DirectX::Bind(ICommandList* cmdList, uint32_t slot)
{
	CommandList_DirectX* dxCmd = static_cast<CommandList_DirectX*>(cmdList);
	D3D12_GPU_VIRTUAL_ADDRESS address = bufferResource->GetResource()->GetGPUVirtualAddress();
	dxCmd->GetHandle()->SetGraphicsRootConstantBufferView(slot, address);
}
