#include "../Common/Sampler.hlsli"

#pragma pack_matrix(row_major)

// t0 — HDR 입력 (DeferredLighting + Sky + Transparent 합성 결과)
Texture2D<float4> gHDR : register(t0);

// b0 — Tonemap 파라미터. 백버퍼가 R8G8B8A8_UNORM(non-sRGB)이므로 감마 보정도 여기서 수행
cbuffer CB_Tonemap : register(b0)
{
    float gExposure;
    float3 _tonemapPad;
};

struct VS_OUT
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

// SV_VertexID 기반 전체화면 삼각형 (정점 버퍼 불필요)
VS_OUT VSMain(uint vid : SV_VertexID)
{
    VS_OUT o;
    o.uv  = float2((vid << 1) & 2, vid & 2);
    // DX NDC: Y-up, UV: Y-down → UV.y 반전
    o.pos = float4(o.uv.x * 2.0f - 1.0f, 1.0f - o.uv.y * 2.0f, 0.0f, 1.0f);
    return o;
}

// ACES Filmic (Narkowicz 2015 근사)
float3 ACESFilmic(float3 x)
{
    const float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float4 PSMain(VS_OUT input) : SV_TARGET
{
    float3 hdrColor = gHDR.Sample(LinearSampler, input.uv).rgb * gExposure;
    float3 tonemapped = ACESFilmic(hdrColor);
    float3 gammaCorrected = pow(tonemapped, 1.0f / 2.2f);  // 백버퍼 non-sRGB → 수동 감마
    return float4(gammaCorrected, 1.0f);
}
