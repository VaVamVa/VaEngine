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
	ConstantBuffer	= 0,	// CBV (b 레지스터) — root CBV
	Texture			= 1,	// SRV via descriptor table (t 레지스터) — Texture2D / Texture2DArray 등
	Sampler			= 2,	// Sampler (s 레지스터)
	UAV				= 3,	// UAV via root descriptor (u 레지스터) — Buffer/RWByteAddressBuffer/RWStructuredBuffer
	TextureUAV		= 4,	// UAV via descriptor table (u 레지스터) — RWTexture / RWTexture2DArray
	BufferSRV		= 5,	// SRV via root descriptor (t 레지스터) — StructuredBuffer / ByteAddressBuffer (compute 입력)
};

// 컬링 방향 (래스터라이저 백페이스 컬링 — 프러스텀 컬링과는 무관)
enum class EBackfaceCullMode : uint32_t
{
	None	= 0,
	Front	= 1,
	Back	= 2,
};

// 블렌드 모드
enum class EBlendMode : uint32_t
{
	Opaque		  = 0,
	AlphaBlend	  = 1,
	Additive	  = 2,
	// Weighted Blended OIT 전용 — RT0(accum) 가산 + RT1(revealage) 곱셈을 한 PSO에서 동시에 사용.
	// 범용 "렌더타겟별 독립 블렌드" 시스템이 아니라 이 조합 하나만 실용적으로 배선(Refactoring_At260711.md
	// 항목 2와 동일한 패턴 — 지금 필요한 조합만 하드코딩, 세 번째 조합이 필요해지면 그때 일반화).
	OITAccumulate = 3,
};

// 프리미티브 토폴로지 타입 (PSO 생성 시 지정)
enum class EPrimitiveTopologyType : uint32_t
{
	Triangle = 0,
	Line     = 1,
	Point    = 2,
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

	EPixelFormat           rtvFormats[8] = {};
	uint32_t               rtvCount      = 1;
	EPixelFormat           dsvFormat     = EPixelFormat::Unknown;

	EBackfaceCullMode      cullMode      = EBackfaceCullMode::Back;
	EBlendMode             blendMode     = EBlendMode::Opaque;
	bool                   depthEnable   = false;
	bool                   depthWrite    = true;
	EPrimitiveTopologyType topologyType  = EPrimitiveTopologyType::Triangle;

	IBindingLayout*        bindingLayout = nullptr;
};
