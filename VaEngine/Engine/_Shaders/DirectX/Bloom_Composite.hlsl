#include "../Common/Sampler.hlsli"

#pragma pack_matrix(row_major)

// t0 — 블러된 Bloom (half-res) — LinearSampler로 업샘플링하며 hdrOut(RTV)에 가산 블렌딩
Texture2D<float4> gBloom : register(t0);

cbuffer CB_BloomIntensity : register(b0)
{
    float gIntensity;
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

// PSO가 additive blend(One, One)로 구성되어 hdrOut 기존 값 위에 가산됨
float4 PSMain(VS_OUT input) : SV_TARGET
{
    float3 bloom = gBloom.Sample(LinearSampler, input.uv).rgb * gIntensity;
    return float4(bloom, 1.0f);
}
