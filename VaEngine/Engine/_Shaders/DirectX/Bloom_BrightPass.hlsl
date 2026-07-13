#include "../Common/Sampler.hlsli"

#pragma pack_matrix(row_major)

// t0 — hdrOut (풀 해상도)
Texture2D<float4> gHDR : register(t0);

cbuffer CB_BloomThreshold : register(b0)
{
    float gThreshold;
    float3 _pad;
};

struct VS_OUT
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

VS_OUT VSMain(uint vid : SV_VertexID)
{
    VS_OUT o;
    o.uv  = float2((vid << 1) & 2, vid & 2);
    o.pos = float4(o.uv.x * 2.0f - 1.0f, 1.0f - o.uv.y * 2.0f, 0.0f, 1.0f);
    return o;
}

// Soft threshold — Karis 스타일 (threshold 이하는 0, 이상은 초과분만 통과)
float4 PSMain(VS_OUT input) : SV_TARGET
{
    float3 color = gHDR.Sample(LinearSampler, input.uv).rgb;
    float  luma  = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    float  contribution = max(luma - gThreshold, 0.0f) / max(luma, 1e-4f);
    return float4(color * contribution, 1.0f);
}
