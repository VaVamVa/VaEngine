#pragma once

#include <cstdint>
#include "RHI/Common_RHI.h"

class IBindingLayout;
class IShader;

// 셰이더 스테이지
enum class EShaderStage : uint32_t
{
	Vertex	= 0,
	Pixel	= 1,
	Compute	= 2,
	All		= 3,
};

// 바인딩 리소스 유형
enum class EBindingType : uint32_t
{
	ConstantBuffer	= 0,	// CBV (b 레지스터)
	Texture			= 1,	// SRV (t 레지스터)
	Sampler			= 2,	// Sampler (s 레지스터)
	UAV				= 3,	// UAV (u 레지스터)
};

// 컬링 방향
enum class ECullMode : uint32_t
{
	None	= 0,
	Front	= 1,
	Back	= 2,
};

// 블렌드 모드
enum class EBlendMode : uint32_t
{
	Opaque		= 0,
	AlphaBlend	= 1,
	Additive	= 2,
};

// 바인딩 레이아웃 엔트리 (Root Signature / Pipeline Layout 한 슬롯)
struct BindingEntry
{
	EBindingType type;
	uint32_t     slot;   // b0, t0, u0, s0 등 레지스터 번호
	EShaderStage stage;
};

// 정점 입력 레이아웃 한 요소
struct VertexInputDesc
{
	const char*  semantic;
	uint32_t     semanticIndex;
	EPixelFormat format;
	uint32_t     byteOffset;
	uint32_t     inputSlot   = 0;
	bool         perInstance = false;
};

// PSO 생성 시 필요한 전체 서술자
struct PipelineStateDesc
{
	IShader*               shader         = nullptr;

	const VertexInputDesc* vertexInputs;
	uint32_t               vertexInputCount;

	EPixelFormat           rtvFormat;
	EPixelFormat           dsvFormat  = EPixelFormat::Unknown;

	ECullMode              cullMode   = ECullMode::Back;
	EBlendMode             blendMode  = EBlendMode::Opaque;
	bool                   depthEnable = false;

	IBindingLayout*        bindingLayout = nullptr;
};
