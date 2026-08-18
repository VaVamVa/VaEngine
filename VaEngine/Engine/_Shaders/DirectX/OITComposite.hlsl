#include "../Common/Sampler.hlsli"

#pragma pack_matrix(row_major)

// Weighted Blended OIT 합성 — accum/revealage를 풀어 최종 색·알파를 계산하고,
// 기존 EBlendMode::AlphaBlend PSO로 hdrOut에 블렌드한다(별도 블렌드 모드 불필요).
Texture2D<float4> gAccum     : register(t0);
Texture2D<float>  gRevealage : register(t1);

struct VS_OUT
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

// SV_VertexID 기반 전체화면 삼각형 (Blit.hlsl과 동일 컨벤션)
VS_OUT VSMain(uint vid : SV_VertexID)
{
    VS_OUT o;
    o.uv  = float2((vid << 1) & 2, vid & 2);
    o.pos = float4(o.uv.x * 2.0f - 1.0f, 1.0f - o.uv.y * 2.0f, 0.0f, 1.0f);
    return o;
}

float4 PSMain(VS_OUT input) : SV_TARGET
{
    float4 accum     = gAccum.Sample(PointSampler, input.uv);
    float  revealage = gRevealage.Sample(PointSampler, input.uv);

    float3 finalColor = accum.rgb / max(accum.a, 1e-5f);
    float  finalAlpha = 1.0f - revealage;

    return float4(finalColor, finalAlpha);
}
