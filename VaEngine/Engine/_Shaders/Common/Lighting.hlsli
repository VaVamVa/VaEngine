#ifndef LIGHTING_HLSLI
#define LIGHTING_HLSLI

#include "Sampler.hlsli"

#define MAX_POINT_LIGHTS 8
#define MAX_SPOT_LIGHTS  4

// --- Light / Material structs ---

struct DirectionalLight
{
    float4 ambient;
    float4 diffuse;
    float4 specular;
    float3 direction;
    float  _pad;
};

struct PointLight
{
    float4 ambient;
    float4 diffuse;
    float4 specular;
    float3 position;
    float  range;
    float3 attenuation;
    float  _pad;
};

struct SpotLight
{
    float4 ambient;
    float4 diffuse;
    float4 specular;
    float3 position;
    float  range;
    float3 direction;
    float  spot;
    float3 attenuation;
    float  _pad;
};

struct Material
{
    float4 ambient;
    float4 diffuse;
    float4 specular; // w = specular power
    float4 reflect;
};

// Per-frame light constants (b2)
cbuffer CB_Lights : register(b2)
{
    DirectionalLight gDirLight;
    PointLight       gPointLights[MAX_POINT_LIGHTS];   // 80 * 8 = 640 bytes
    SpotLight        gSpotLights[MAX_SPOT_LIGHTS];     // 96 * 4 = 384 bytes
    Material         gMaterial;
    float3           gEyePosW;
    int              gNumPointLights;
    int              gNumSpotLights;
    float3           _lightPad;
};

// --- Light contribution functions ---

void ComputeDirectionalLight(
    Material mat, DirectionalLight light,
    float3 normal, float3 toEye,
    out float4 ambient, out float4 diffuse, out float4 spec)
{
    ambient = float4(0, 0, 0, 0);
    diffuse = float4(0, 0, 0, 0);
    spec    = float4(0, 0, 0, 0);

    float3 lightVec = -light.direction;
    ambient = mat.ambient * light.ambient;

    float diffFactor = dot(lightVec, normal);
    if (diffFactor > 0.0f)
    {
        diffuse = diffFactor * mat.diffuse * light.diffuse;
        float3 r = reflect(-lightVec, normal);
        float specFactor = pow(max(dot(r, toEye), 0.0f), mat.specular.w);
        spec = specFactor * mat.specular * light.specular;
    }
}

void ComputePointLight(
    Material mat, PointLight light,
    float3 pos, float3 normal, float3 toEye,
    out float4 ambient, out float4 diffuse, out float4 spec)
{
    ambient = float4(0, 0, 0, 0);
    diffuse = float4(0, 0, 0, 0);
    spec    = float4(0, 0, 0, 0);

    float3 lightVec = light.position - pos;
    float dist = length(lightVec);
    if (dist > light.range) return;

    lightVec /= dist;
    ambient = mat.ambient * light.ambient;

    float diffFactor = dot(lightVec, normal);
    if (diffFactor > 0.0f)
    {
        diffuse = diffFactor * mat.diffuse * light.diffuse;
        float3 r = reflect(-lightVec, normal);
        float specFactor = pow(max(dot(r, toEye), 0.0f), mat.specular.w);
        spec = specFactor * mat.specular * light.specular;
    }

    float att = 1.0f / dot(light.attenuation, float3(1.0f, dist, dist * dist));
    diffuse *= att;
    spec    *= att;
}

void ComputeSpotLight(
    Material mat, SpotLight light,
    float3 pos, float3 normal, float3 toEye,
    out float4 ambient, out float4 diffuse, out float4 spec)
{
    ambient = float4(0, 0, 0, 0);
    diffuse = float4(0, 0, 0, 0);
    spec    = float4(0, 0, 0, 0);

    float3 lightVec = light.position - pos;
    float dist = length(lightVec);
    if (dist > light.range) return;

    lightVec /= dist;
    ambient = mat.ambient * light.ambient;

    float diffFactor = dot(lightVec, normal);
    if (diffFactor > 0.0f)
    {
        diffuse = diffFactor * mat.diffuse * light.diffuse;
        float3 r = reflect(-lightVec, normal);
        float specFactor = pow(max(dot(r, toEye), 0.0f), mat.specular.w);
        spec = specFactor * mat.specular * light.specular;
    }

    float spot = pow(max(dot(-lightVec, light.direction), 0.0f), light.spot);
    float att  = spot / dot(light.attenuation, float3(1.0f, dist, dist * dist));
    ambient *= spot;
    diffuse *= att;
    spec    *= att;
}

// TODO: Shadow mapping — Depth Buffer 구현 후 활성화
// Texture2D    gShadowMap        : register(t8);
// float4x4     gShadowTransform;
// float        gShadowMapSize;
// float        gShadowMapInvSize;

#endif // LIGHTING_HLSLI
