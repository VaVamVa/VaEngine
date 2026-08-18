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

	// 텍스처 바인딩이 있으면 s0(wrap) + s1(point-wrap) + s2(shadow comparison) static sampler 추가.
	// EBindingType::Sampler를 통한 범용 커스터마이징은 Refactoring_At260711.md 항목 2로 분리 —
	// 지금은 실제로 쓰이는 슬롯만 실용적으로 배선. s1(PointSampler, Sampler.hlsli에 선언은
	// 되어 있었으나 여기 연결이 안 되어 있었음)은 SSAO 노이즈 텍스처 타일링에 필요해 추가.
	CD3DX12_STATIC_SAMPLER_DESC staticSamplers[3];
	staticSamplers[0].Init(0);  // s0 — wrap (기본 필터: anisotropic)
	staticSamplers[1].Init(1, D3D12_FILTER_MIN_MAG_MIP_POINT);  // s1 — point-wrap
	staticSamplers[2] = CD3DX12_STATIC_SAMPLER_DESC(
		2,                                                    // shaderRegister s2
		D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		0.0f, 16,
		D3D12_COMPARISON_FUNC_LESS_EQUAL,
		D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE);

	// Compute 전용 root signature는 IA flag 없음 — 그래픽스만 IA flag 가짐
	D3D12_ROOT_SIGNATURE_FLAGS flags = isCompute
		? D3D12_ROOT_SIGNATURE_FLAG_NONE
		: D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	CD3DX12_ROOT_SIGNATURE_DESC desc;
	desc.Init((UINT)count, rootParams.data(),
		hasTexture ? 3u : 0u,
		hasTexture ? staticSamplers : nullptr,
		flags);

	ComPtr<ID3DBlob> signature, error;
	if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)))
		throw std::runtime_error("Failed to serialize root signature");

	if (FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature))))
		throw std::runtime_error("Failed to create root signature");
}
