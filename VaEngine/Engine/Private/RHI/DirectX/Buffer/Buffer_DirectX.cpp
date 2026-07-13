#include "Buffer_DirectX.h"

#include <stdexcept>

void Buffer_DirectX::Create(ID3D12Device* device, const BufferDesc& desc)
{
	size_t allocSize = static_cast<size_t>(desc.size);

	const uint32_t usageBits = static_cast<uint32_t>(desc.usage);
	const bool     isCBV     = usageBits & static_cast<uint32_t>(EBufferUsage::ConstantBuffer);
	const bool     isUAV     = usageBits & static_cast<uint32_t>(EBufferUsage::UnorderedAccess);

	// CBV는 256바이트 정렬 필요
	if (isCBV)
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
	resourceDesc.Flags              = isUAV
		? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
		: D3D12_RESOURCE_FLAG_NONE;

	D3D12_RESOURCE_STATES initialState;
	if (desc.access == EMemoryAccess::Upload)
		initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
	else if (isUAV)
		initialState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	else
		initialState = D3D12_RESOURCE_STATE_COMMON;

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

	// D3D12는 버퍼 리소스에 한해 initialState 파라미터를 무시하고 항상 COMMON으로 생성한다
	// (디버그 레이어 경고: "Buffers are effectively created in state D3D12_RESOURCE_STATE_COMMON").
	// 요청한 usage(UAV 여부 등)와 무관하게 실제 상태는 항상 Common — 이전에는 UAV 버퍼를
	// UnorderedAccess로 잘못 가정해, 최초 프레임의 Common→UnorderedAccess 배리어가 누락되어 있었다.
	SetTrackedState(EResourceState::Common);
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
