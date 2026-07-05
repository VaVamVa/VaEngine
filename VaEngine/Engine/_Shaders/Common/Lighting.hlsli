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

struct PointLight          // 48 bytes
{
    float3 color;
    float  range;
    float3 position;
    float  intensity;
    float3 attenuation;
    float  _pad;
};

struct SpotLight           // 64 bytes
{
    float3 color;
    float  range;
    float3 position;
    float  intensity;
    float3 direction;
    float  spot;
    float3 attenuation;
    float  _pad;
};

// Per-frame light constants (b2) — material 없음
cbuffer CB_Lights : register(b2)
{
    DirectionalLight gDirLight;
    PointLight       gPointLights[MAX_POINT_LIGHTS];  // 48 * 8 = 384 bytes
    SpotLight        gSpotLights[MAX_SPOT_LIGHTS];    // 64 * 4 = 256 bytes
    float3           gEyePosW;
    int              gNumPointLights;
    int              gNumSpotLights;
    float3           _lightPad;
    // total: 704 bytes → 768 (CBV 256-aligned)
};

// ── Cook-Torrance GGX ────────────────────────────────────────────────────────

// Trowbridge-Reitz GGX 법선 분포 함수
float D_GGX(float NdotH, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
    return a2 / (PI * d * d);
}

// Fresnel-Schlick 근사
float3 F_Schlick(float VdotH, float3 F0)
{
    return F0 + (1.0f - F0) * pow(1.0f - VdotH, 5.0f);
}

// Smith Schlick-GGX 기하 감쇠
float G_Smith(float NdotV, float NdotL, float roughness)
{
    float r  = roughness + 1.0f;
    float k  = (r * r) / 8.0f;
    float gv = NdotV / (NdotV * (1.0f - k) + k);
    float gl = NdotL / (NdotL * (1.0f - k) + k);
    return gv * gl;
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
    float  G = G_Smith(NdotV, NdotL, roughness);

    float3 spec = (D * F * G) / max(4.0f * NdotV * NdotL, 0.001f);
    float3 kD   = (1.0f - F) * (1.0f - metallic);

    return (kD * albedo / PI + spec) * radiance * NdotL;
}

#endif // LIGHTING_HLSLI
