#include "BindingLayout_DirectX.h"

#include <stdexcept>
#include <vector>

void BindingLayout_DirectX::Create(ID3D12Device* device, const BindingEntry* entries, uint32_t count, bool inIsCompute)
{
	isCompute = inIsCompute;

	std::vector<CD3DX12_ROOT_PARAMETER>   rootParams(count);
	std::vector<CD3DX12_DESCRIPTOR_RANGE> ranges;
	ranges.reserve(count);  // 재할당 방지 (root param이 range 포인터를 가리킴)

	bool hasTexture = false;

	for (uint32_t i = 0; i < count; ++i)
	{
		switch (entries[i].type)
		{
		case EBindingType::ConstantBuffer:
			rootParams[i].InitAsConstantBufferView(entries[i].slot);
			break;
		case EBindingType::Texture:
			ranges.push_back({});
			ranges.back().Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, entries[i].slot);
			rootParams[i].InitAsDescriptorTable(1, &ranges.back());
			hasTexture = true;
			break;
		case EBindingType::UAV:
			// root UAV — buffer 전용 (RWByteAddressBuffer / RWStructuredBuffer)
			rootParams[i].InitAsUnorderedAccessView(entries[i].slot);
			break;
		case EBindingType::TextureUAV:
			// descriptor table UAV — RWTexture / RWTexture2DArray 등
			ranges.push_back({});
			ranges.back().Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, entries[i].slot);
			rootParams[i].InitAsDescriptorTable(1, &ranges.back());
			break;
		case EBindingType::BufferSRV:
			// root SRV — StructuredBuffer / ByteAddressBuffer 입력
			rootParams[i].InitAsShaderResourceView(entries[i].slot);
			break;
		default:
			throw std::runtime_error("Unsupported binding type");
		}
	}

	// 텍스처 바인딩이 있으면 s0에 static linear-wrap sampler 추가
	CD3DX12_STATIC_SAMPLER_DESC staticSampler(0);

	// Compute 전용 root signature는 IA flag 없음 — 그래픽스만 IA flag 가짐
	D3D12_ROOT_SIGNATURE_FLAGS flags = isCompute
		? D3D12_ROOT_SIGNATURE_FLAG_NONE
		: D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	CD3DX12_ROOT_SIGNATURE_DESC desc;
	desc.Init((UINT)count, rootParams.data(),
		hasTexture ? 1u : 0u,
		hasTexture ? &staticSampler : nullptr,
		flags);

	ComPtr<ID3DBlob> signature, error;
	if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)))
		throw std::runtime_error("Failed to serialize root signature");

	if (FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature))))
		throw std::runtime_error("Failed to create root signature");
}
