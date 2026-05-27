#include "../Common/Sampler.hlsli"

#pragma pack_matrix(row_major)

// t0 — HDR 입력 (DeferredLighting + Sky + Transparent 합성 결과)
Texture2D<float4> gHDR : register(t0);

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

float4 PSMain(VS_OUT input) : SV_TARGET
{
    return gHDR.Sample(LinearSampler, input.uv);
}
