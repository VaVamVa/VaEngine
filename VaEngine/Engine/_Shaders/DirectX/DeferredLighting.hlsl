#include "../Common/Lighting.hlsli"

#pragma pack_matrix(row_major)

// b0 — Deferred 전용 카메라 상수
cbuffer CB_DeferredCamera : register(b0)
{
    float4x4 InvViewProj;
    float3   EyePos;
    float    _pad;
    uint     ScreenW;
    uint     ScreenH;
    float2   _pad2;
};

// b2 — CB_Lights (Lighting.hlsli)

// G-Buffer SRV
Texture2D<float4> gAlbedoAO    : register(t0);  // RT0: Albedo(RGB) + AO(A)
Texture2D<float4> gNormalRough : register(t1);  // RT1: Normal(XYZ) + Roughness(W)
Texture2D<float4> gMaterialBuf : register(t2);  // RT2: Metallic(R)
Texture2D<float>  gDepth       : register(t3);  // Depth

RWTexture2D<float4> outHDR : register(u0);

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= ScreenW || id.y >= ScreenH)
        return;

    // --- 1. G-Buffer 읽기 ---
    float  depth       = gDepth[id.xy];
    float4 albedoAO    = gAlbedoAO[id.xy];
    float4 normalRough = gNormalRough[id.xy];
    float  metallic    = gMaterialBuf[id.xy].r;

    // depth == 1.0 → Sky 영역 (SkyPass가 이미 기록 완료)
    if (depth >= 1.0f)
        return;

    float3 albedo    = albedoAO.rgb;
    float  ao        = albedoAO.a;
    float3 N         = normalize(normalRough.xyz);
    float  roughness = normalRough.w;

    // --- 2. Depth → World Position 역투영 ---
    float2 uv       = (float2(id.xy) + 0.5f) / float2(ScreenW, ScreenH);
    float4 ndcPos   = float4(uv * 2.0f - 1.0f, depth, 1.0f);
    ndcPos.y        = -ndcPos.y;
    float4 wPos     = mul(ndcPos, InvViewProj);
    float3 worldPos = wPos.xyz / wPos.w;

    float3 V = normalize(EyePos - worldPos);

    // --- 3. Cook-Torrance GGX 조명 누적 ---
    float3 Lo = float3(0.0f, 0.0f, 0.0f);

    // Directional Light
    {
        float3 L        = normalize(-gDirLight.direction);
        float3 radiance = gDirLight.color * gDirLight.intensity;
        Lo += EvalBRDF(N, V, L, albedo, roughness, metallic, radiance);
    }

    // Point Lights
    for (int i = 0; i < gNumPointLights; ++i)
    {
        float3 toLight = gPointLights[i].position - worldPos;
        float  dist    = length(toLight);
        if (dist >= gPointLights[i].range) continue;

        float3 L        = toLight / dist;
        float  att      = 1.0f / dot(gPointLights[i].attenuation, float3(1.0f, dist, dist * dist));
        float3 radiance = gPointLights[i].color * gPointLights[i].intensity * att;
        Lo += EvalBRDF(N, V, L, albedo, roughness, metallic, radiance);
    }

    // Spot Lights
    for (int j = 0; j < gNumSpotLights; ++j)
    {
        float3 toLight = gSpotLights[j].position - worldPos;
        float  dist    = length(toLight);
        if (dist >= gSpotLights[j].range) continue;

        float3 L    = toLight / dist;
        float  spot = pow(max(dot(-L, gSpotLights[j].direction), 0.0f), gSpotLights[j].spot);
        float  att  = spot / dot(gSpotLights[j].attenuation, float3(1.0f, dist, dist * dist));
        float3 radiance = gSpotLights[j].color * gSpotLights[j].intensity * att;
        Lo += EvalBRDF(N, V, L, albedo, roughness, metallic, radiance);
    }

    // 간이 Ambient (IBL 미구현 — 상수 ambient 항)
    float3 ambient = float3(0.10f, 0.10f, 0.10f) * albedo * ao;

    outHDR[id.xy] = float4(Lo + ambient, 1.0f);
}
