#ifndef LIGHTING_HLSLI
#define LIGHTING_HLSLI

#include "Sampler.hlsli"

#define MAX_POINT_LIGHTS 8
#define MAX_SPOT_LIGHTS  4
#define PI 3.14159265f

// ── PBR 조명 구조체 (ILight.h 와 1:1) ────────────────────────────────────────

struct DirectionalLight   // 32 bytes
{
    float3 color;
    float  intensity;
    float3 direction;
    float  _pad;
};

struct PointLight          // 32 bytes
{
    float3 color;
    float  range;
    float3 position;
    float  intensity;
};

struct SpotLight           // 48 bytes
{
    float3 color;
    float  range;
    float3 position;
    float  intensity;
    float3 direction;
    float  spot;
};

// Per-frame light constants (b2) — material 없음
cbuffer CB_Lights : register(b2)
{
    DirectionalLight gDirLight;
    PointLight       gPointLights[MAX_POINT_LIGHTS];  // 32 * 8 = 256 bytes
    SpotLight        gSpotLights[MAX_SPOT_LIGHTS];    // 48 * 4 = 192 bytes
    float3           gEyePosW;
    int              gNumPointLights;
    int              gNumSpotLights;
    uint             gIBLEnabled;   // 0/1 — false면 스카이박스 없음(IBL 폴백), EvalAmbientIBL이 flat ambient로 대체
    float2           _lightPad;
    // total: 32 + 256 + 192 + 32 = 512 bytes (CBV 256-aligned)
};

// ── Cook-Torrance GGX ────────────────────────────────────────────────────────

// Trowbridge-Reitz GGX 법선 분포 함수
float D_GGX(float NdotH, float roughness)
{
    float a  = max(roughness * roughness, 0.0001f);
    float a2 = a * a;
    float d  = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
    return a2 / (PI * d * d);
}

// Fresnel-Schlick 근사
float3 F_Schlick(float VdotH, float3 F0)
{
    return F0 + (1.0f - F0) * pow(1.0f - VdotH, 5.0f);
}

// Smith Schlick-GGX 기하 감쇠 — 핵심 항은 k를 직접 받는다. k의 유도 공식이 direct 조명과
// IBL(환경광)에서 서로 다르므로(Karis/UE4 remapping), k 계산은 호출부 전용 래퍼로 분리한다.
float G_Smith(float NdotV, float NdotL, float k)
{
    float gv = NdotV / (NdotV * (1.0f - k) + k);
    float gl = NdotL / (NdotL * (1.0f - k) + k);
    return gv * gl;
}

// Direct 조명(Directional/Point/Spot)용 k — k_direct = (roughness+1)^2 / 8
float G_Smith_Direct(float NdotV, float NdotL, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return G_Smith(NdotV, NdotL, k);
}

// IBL(환경광)용 k = roughness^2 / 2 (Karis/UE4 remapping) — BRDF LUT 적분(IntegrateBRDF.hlsl)에서 사용
float G_Smith_IBL(float NdotV, float NdotL, float roughness)
{
    float k = (roughness * roughness) / 2.0f;
    return G_Smith(NdotV, NdotL, k);
}

// Fresnel-Schlick, roughness 보정판 (Sébastien Lagarde) — grazing angle에서 거친 표면이
// 매끈한 표면만큼 밝게 반사되는 것을 방지. IBL Specular/Diffuse kS·kD 분리 전용.
float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    float3 maxF = max(float3(1.0f - roughness, 1.0f - roughness, 1.0f - roughness), F0);
    return F0 + (maxF - F0) * pow(1.0f - cosTheta, 5.0f);
}

// 물리 기반 거리 감쇠 (역제곱 법칙 + range 기반 windowing)
// windowing: range 밖에서 부드럽게 0으로 수렴 (UE4 스타일)
float CalcDistanceAttenuation(float dist, float range)
{
    float distSq    = max(dist * dist, 0.0001f);
    float invDistSq  = 1.0f / distSq;
    float windowing  = saturate(1.0f - pow(dist / range, 4.0f));
    return invDistSq * windowing * windowing;
}

// 단일 광원의 BRDF 기여
// radiance: light.color * light.intensity (* attenuation)
float3 EvalBRDF(
    float3 N, float3 V, float3 L,
    float3 albedo, float roughness, float metallic,
    float3 radiance)
{
    float3 H    = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0f);
    float NdotV = max(dot(N, V), 0.001f);
    float NdotH = max(dot(N, H), 0.0f);
    float VdotH = max(dot(V, H), 0.0f);

    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);

    float  D = D_GGX(NdotH, roughness);
    float3 F = F_Schlick(VdotH, F0);
    float  G = G_Smith_Direct(NdotV, NdotL, roughness);

    float3 spec = (D * F * G) / max(4.0f * NdotV * NdotL, 0.001f);
    float3 kD   = (1.0f - F) * (1.0f - metallic);

    return (kD * albedo / PI + spec) * radiance * NdotL;
}

// ── IBL (Image-Based Lighting) ───────────────────────────────────────────────

// Specular Prefiltered Map의 밉 레벨 수 — IBLRenderer::kPrefilterMipLevels(C++)와 반드시 동기화.
#define IBL_SPEC_MIP_COUNT 5

// Sky.hlsl과 동일한 LatLong 정방향 매핑 — 방향 벡터를 equirect UV로 변환
float2 DirToEquirectUV(float3 dir)
{
    float theta = atan2(dir.z, dir.x);
    float phi   = asin(clamp(dir.y, -1.0f, 1.0f));
    return float2(theta / (2.0f * PI) + 0.5f, 0.5f - phi / PI);
}

// DirToEquirectUV의 역변환 — equirect UV를 방향 벡터로 복원 (PrefilterSpecular.hlsl에서 사용)
float3 EquirectUVToDir(float2 uv)
{
    float theta  = (uv.x - 0.5f) * 2.0f * PI;
    float phi    = (0.5f - uv.y) * PI;
    float cosPhi = cos(phi);
    return float3(cosPhi * cos(theta), sin(phi), cosPhi * sin(theta));
}

// Diffuse(Stage A) + Specular(Stage B) IBL 합산 — Deferred/Transparent가 공유.
// gIBLEnabled==0(스카이박스 없음)이면 기존 flat ambient로 대체.
float3 EvalAmbientIBL(
    float3 N, float3 V, float3 albedo, float roughness, float metallic, float ao,
    Texture2D<float4> irradianceMap, Texture2D<float4> prefilteredMap, Texture2D<float2> brdfLUT)
{
    if (gIBLEnabled == 0)
        return float3(0.10f, 0.10f, 0.10f) * albedo * ao;

    float  NdotV = max(dot(N, V), 0.001f);
    float3 F0    = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
    float3 kS    = FresnelSchlickRoughness(NdotV, F0, roughness);
    float3 kD    = (1.0f - kS) * (1.0f - metallic);

    float3 irradiance = irradianceMap.SampleLevel(LinearSampler, DirToEquirectUV(N), 0).rgb;
    float3 diffuse     = kD * albedo * irradiance;

    float3 R = reflect(-V, N);
    float  mipLevel        = roughness * float(IBL_SPEC_MIP_COUNT - 1);
    float3 prefilteredColor = prefilteredMap.SampleLevel(LinearSampler, DirToEquirectUV(R), mipLevel).rgb;
    float2 envBRDF          = brdfLUT.SampleLevel(LinearSampler, float2(NdotV, roughness), 0).rg;
    float3 specular         = prefilteredColor * (kS * envBRDF.x + envBRDF.y);

    return (diffuse + specular) * ao;
}

#endif // LIGHTING_HLSLI
