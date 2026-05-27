#include "../Common/Lighting.hlsli"

#pragma pack_matrix(row_major)

// b0 — Deferred 전용 카메라 상수 (§12.6)
cbuffer CB_DeferredCamera : register(b0)
{
    float4x4 InvViewProj;
    float3   EyePos;
    float    _pad;
    uint     ScreenW;
    uint     ScreenH;
    float2   _pad2;
};

// b2 — CB_Lights (Lighting.hlsli — gDirLight / gPointLights / gSpotLights / gNumPointLights 등)

// G-Buffer SRV (Compute SRV는 descriptor table)
Texture2D<float4> gAlbedoAO    : register(t0);  // RT0: Albedo(RGB) + AO(A)
Texture2D<float4> gNormalRough : register(t1);  // RT1: Normal(XYZ, [-1,1]) + Roughness(W)
Texture2D<float4> gMaterialBuf : register(t2);  // RT2: Metallic(R) + 예약(GBA)
Texture2D<float>  gDepth       : register(t3);  // Depth SRV (R24_UNORM_X8_TYPELESS)

// HDR 출력 UAV
RWTexture2D<float4> outHDR : register(u0);

// roughness를 Phong specular power로 변환 (PBR 전환 전 임시)
// roughness 0 → 고광택(high), 1 → 무광(low)
float RoughnessToSpecPow(float roughness)
{
    return max(1.0f - roughness, 0.01f) * 128.0f;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    // Dispatch 범위 초과 스레드 조기 종료
    if (id.x >= ScreenW || id.y >= ScreenH)
        return;

    // --- 1. G-Buffer 읽기 ---
    float  depth       = gDepth[id.xy];
    float4 albedoAO    = gAlbedoAO[id.xy];
    float4 normalRough = gNormalRough[id.xy];
    float  metallic    = gMaterialBuf[id.xy].r;

    // depth == 1.0 → 스카이 영역. SkyPass가 이미 hdrOut에 기록했으므로 그대로 유지
    if (depth >= 1.0f)
        return;

    float3 albedo    = albedoAO.rgb;
    float  ao        = albedoAO.a;
    float3 N         = normalize(normalRough.xyz);
    float  roughness = normalRough.w;
    float  specPow   = RoughnessToSpecPow(roughness);

    // --- 2. Depth → WorldPos 역투영 ---
    float2 uv     = (float2(id.xy) + 0.5f) / float2(ScreenW, ScreenH);
    float4 ndcPos = float4(uv * 2.0f - 1.0f, depth, 1.0f);
    ndcPos.y      = -ndcPos.y;          // DX NDC: Y 축 상단이 +1
    float4 wPos   = mul(ndcPos, InvViewProj);
    float3 worldPos = wPos.xyz / wPos.w;

    float3 V = normalize(EyePos - worldPos);

    // --- 3. 조명 누적 (Phong 기반 — 추후 PBR로 교체) ---
    float3 result = float3(0.0f, 0.0f, 0.0f);

    // Directional Light
    {
        float3 L     = normalize(-gDirLight.direction);
        float3 H     = normalize(L + V);
        float  NdotL = max(dot(N, L), 0.0f);
        float  NdotH = max(dot(N, H), 0.0f);

        result += gDirLight.ambient.rgb  * albedo * ao;
        result += gDirLight.diffuse.rgb  * albedo * NdotL;
        result += gDirLight.specular.rgb * pow(NdotH, specPow) * (1.0f - roughness);
    }

    // Point Lights
    for (int i = 0; i < gNumPointLights; ++i)
    {
        float3 toLight = gPointLights[i].position - worldPos;
        float  dist    = length(toLight);
        if (dist >= gPointLights[i].range) continue;

        float3 L   = toLight / dist;
        float3 H   = normalize(L + V);
        float  att = 1.0f / dot(gPointLights[i].attenuation, float3(1.0f, dist, dist * dist));
        float  NdotL = max(dot(N, L), 0.0f);
        float  NdotH = max(dot(N, H), 0.0f);

        result += gPointLights[i].ambient.rgb  * albedo * ao;
        result += gPointLights[i].diffuse.rgb  * albedo * NdotL  * att;
        result += gPointLights[i].specular.rgb * pow(NdotH, specPow) * (1.0f - roughness) * att;
    }

    // Spot Lights
    for (int j = 0; j < gNumSpotLights; ++j)
    {
        float3 toLight = gSpotLights[j].position - worldPos;
        float  dist    = length(toLight);
        if (dist >= gSpotLights[j].range) continue;

        float3 L    = toLight / dist;
        float3 H    = normalize(L + V);
        float  spot = pow(max(dot(-L, gSpotLights[j].direction), 0.0f), gSpotLights[j].spot);
        float  att  = spot / dot(gSpotLights[j].attenuation, float3(1.0f, dist, dist * dist));
        float  NdotL = max(dot(N, L), 0.0f);
        float  NdotH = max(dot(N, H), 0.0f);

        result += gSpotLights[j].ambient.rgb  * albedo * ao   * spot;
        result += gSpotLights[j].diffuse.rgb  * albedo * NdotL * att;
        result += gSpotLights[j].specular.rgb * pow(NdotH, specPow) * (1.0f - roughness) * att;
    }

    outHDR[id.xy] = float4(result, 1.0f);
}
