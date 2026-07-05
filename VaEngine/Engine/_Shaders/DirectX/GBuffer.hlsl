#include "../Common/Sampler.hlsli"
#include "../Common/GBufferMaterial.hlsli"

#pragma pack_matrix(row_major)

// b0 — per-frame: view-projection
cbuffer CB_ViewProj : register(b0)
{
    float4x4 gViewProj;
};

// b1 — CB_GBufferMaterial (GBufferMaterial.hlsli)
// t0 — Albedo 텍스처
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
    float3 N        = normalize(input.normal);
    float4 texColor = gDiffuse.Sample(LinearSampler, input.uv) * input.color;

    // Cutout: alphaThreshold > 0 인 경우 반투명 픽셀 폐기
    clip(texColor.a - gAlphaThreshold);

    float3 albedo = texColor.rgb * gAlbedo.rgb;

    GBuffer_OUT o;
    o.rt0 = float4(albedo, gAO);                          // RT0: Albedo(RGB) + AO(A)
    o.rt1 = float4(N, gRoughness);                        // RT1: Normal + Roughness
    o.rt2 = float4(gMetallic, 0.0f, 0.0f, 0.0f);         // RT2: Metallic
    return o;
}
