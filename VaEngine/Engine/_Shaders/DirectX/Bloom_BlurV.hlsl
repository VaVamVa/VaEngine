#include "../Common/Sampler.hlsli"

#pragma pack_matrix(row_major)

// t0 — 이전 단계 출력 (half-res)
Texture2D<float4> gSrc : register(t0);

cbuffer CB_BlurTexel : register(b0)
{
    float2 gTexelSize;
    float2 _pad;
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

// 9-tap 가우시안 (수직)
float4 PSMain(VS_OUT input) : SV_TARGET
{
    static const float weights[5] = { 0.2270270270f, 0.1945945946f, 0.1216216216f, 0.0540540541f, 0.0162162162f };

    float3 result = gSrc.Sample(LinearSampler, input.uv).rgb * weights[0];
    [unroll]
    for (int i = 1; i < 5; ++i)
    {
        float2 offset = float2(0.0f, gTexelSize.y * i);
        result += gSrc.Sample(LinearSampler, input.uv + offset).rgb * weights[i];
        result += gSrc.Sample(LinearSampler, input.uv - offset).rgb * weights[i];
    }
    return float4(result, 1.0f);
}
