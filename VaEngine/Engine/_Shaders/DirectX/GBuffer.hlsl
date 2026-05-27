#include "../Common/Sampler.hlsli"

#pragma pack_matrix(row_major)

// b0 — per-frame: view-projection (ForwardOpaque와 동일 슬롯)
cbuffer CB_ViewProj : register(b0)
{
    float4x4 gViewProj;
};

// b1 — G-Buffer 기록용 material 상수
//       Normal Mapping / PBR 텍스처 도입 시 텍스처로 대체 예정
cbuffer CB_GBufferMaterial : register(b1)
{
    float  gRoughness;
    float  gMetallic;
    float2 _matPad;
};

// t0 — Albedo 텍스처 (ForwardOpaque 동일 슬롯)
Texture2D gDiffuse : register(t0);

// --- 정점 입력 (ForwardOpaque와 동일 레이아웃 — PSO 공유 가능) ---

struct VS_INPUT
{
    // slot 0 — per vertex
    float3 pos    : POSITION;
    float3 normal : NORMAL;
    float4 color  : COLOR;
    float2 uv     : TEXCOORD;
    // slot 1 — per instance: world matrix rows
    float4 row0   : INSTANCETRANSFORM0;
    float4 row1   : INSTANCETRANSFORM1;
    float4 row2   : INSTANCETRANSFORM2;
    float4 row3   : INSTANCETRANSFORM3;
};

struct PS_INPUT
{
    float4 pos    : SV_POSITION;
    float3 wPos   : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float4 color  : TEXCOORD2;
    float2 uv     : TEXCOORD3;
};

// --- MRT 출력 ---
// RT0: R8G8B8A8_UNORM   — Albedo(RGB) + AO(A)
// RT1: R16G16B16A16_FLOAT — WorldNormal(XYZ, [-1,1]) + Roughness(W)
// RT2: R8G8B8A8_UNORM   — Metallic(R) + 예약(GBA)

struct GBuffer_OUT
{
    float4 rt0 : SV_TARGET0;
    float4 rt1 : SV_TARGET1;
    float4 rt2 : SV_TARGET2;
};

PS_INPUT VSMain(VS_INPUT input)
{
    float4x4 world = float4x4(input.row0, input.row1, input.row2, input.row3);
    float4x4 mvp   = mul(world, gViewProj);

    PS_INPUT o;
    o.pos    = mul(float4(input.pos, 1.0f), mvp);
    o.wPos   = mul(float4(input.pos, 1.0f), world).xyz;
    o.normal = normalize(mul(float4(input.normal, 0.0f), world).xyz);
    o.color  = input.color;
    o.uv     = input.uv;
    return o;
}

GBuffer_OUT PSMain(PS_INPUT input)
{
    float3 N      = normalize(input.normal);
    float4 albedo = gDiffuse.Sample(LinearSampler, input.uv) * input.color;

    GBuffer_OUT o;
    // RT0: Albedo. AO는 placeholder 1.0 — AO 텍스처 도입 시 샘플링으로 대체
    o.rt0 = float4(albedo.rgb, 1.0f);
    // RT1: Normal을 [-1,1] FLOAT 그대로 저장 (FLOAT16 포맷이므로 인코딩 불필요)
    o.rt1 = float4(N, gRoughness);
    // RT2: Metallic R 채널. GBA 예약
    o.rt2 = float4(gMetallic, 0.0f, 0.0f, 0.0f);
    return o;
}
