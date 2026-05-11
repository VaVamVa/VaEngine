#include "../Common/Sampler.hlsli"

#pragma pack_matrix(row_major)

// b0 — InvProj + InvViewRot (행벡터 규칙: v_out = v_in * M)
cbuffer CB_SkyData : register(b0)
{
    float4x4 gInvProj;    // Projection 역행렬
    float4x4 gInvViewRot; // View 회전 역행렬 (translation 제거 후 전치)
};

Texture2D gHDRMap : register(t0);

// ─── Vertex Shader ──────────────────────────────────────────────────────────

struct VS_OUT
{
    float4 pos     : SV_Position;
    float4 clipPos : TEXCOORD0;  // PS에서 역변환에 사용
};

VS_OUT VSMain(uint vertexID : SV_VertexID)
{
    // VertexID 0,1,2 → NDC 전스크린 삼각형
    // (−1,−1), (3,−1), (−1, 3) — 화면 전체를 삼각형 하나로 커버
    float x = (float)((vertexID << 1u) & 2u) * 2.0f - 1.0f;
    float y = (float)(vertexID & 2u)          * 2.0f - 1.0f;

    VS_OUT o;
    o.pos     = float4(x, y, 1.0f, 1.0f);
    o.clipPos = float4(x, y, 1.0f, 1.0f);
    return o;
}

// ─── Pixel Shader ───────────────────────────────────────────────────────────

static const float PI = 3.14159265f;

float4 PSMain(VS_OUT input) : SV_Target
{
    // 1. Clip → View 공간 (행벡터 규칙: mul(v, M))
    float4 viewRay = mul(input.clipPos, gInvProj);
    viewRay /= viewRay.w;

    // 2. View → World 방향 벡터 (translation 성분 무시, w=0)
    float3 worldDir = normalize(mul(float4(viewRay.xyz, 0.0f), gInvViewRot).xyz);

    // 3. LatLong UV 매핑 (Equirectangular)
    float theta = atan2(worldDir.z, worldDir.x);                    // [−π, π]
    float phi   = asin(clamp(worldDir.y, -1.0f, 1.0f));            // [−π/2, π/2]

    float u = theta / (2.0f * PI) + 0.5f;
    float v = 0.5f  - phi / PI;

    return gHDRMap.Sample(LinearSampler, float2(u, v));
}
