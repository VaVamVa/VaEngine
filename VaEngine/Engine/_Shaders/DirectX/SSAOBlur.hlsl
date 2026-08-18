#include "../Common/Sampler.hlsli"

#pragma pack_matrix(row_major)

Texture2D<float> gSSAO : register(t0);

cbuffer CB_SSAOBlur : register(b0)
{
    float2 texelSize;
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

// 4x4 박스 블러 — SSAO.hlsl의 노이즈 타일 크기(4x4)와 정확히 일치시켜 주기 패턴을 상쇄한다.
float PSMain(VS_OUT input) : SV_TARGET
{
    float result = 0.0f;
    [unroll]
    for (int x = -2; x < 2; ++x)
    {
        [unroll]
        for (int y = -2; y < 2; ++y)
        {
            float2 offset = float2(x, y) * texelSize;
            result += gSSAO.Sample(PointSampler, input.uv + offset);
        }
    }
    return result / 16.0f;
}
