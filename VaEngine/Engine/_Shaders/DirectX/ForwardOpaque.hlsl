#include "../Common/Lighting.hlsli"
#include "../Common/GBufferMaterial.hlsli"

#pragma pack_matrix(row_major)

// b0 — per-frame: view-projection
cbuffer CB_ViewProj : register(b0)
{
    float4x4 gViewProj;
};

// b1 — CB_GBufferMaterial (GBufferMaterial.hlsli)
// b2 — CB_Lights           (Lighting.hlsli)
// t0 — albedo texture
// t1 — Normal Map (없으면 기본 (128,128,255) tangent-up 텍스처)
Texture2D gDiffuse : register(t0);
Texture2D gNormalMap : register(t1);

struct VS_INPUT
{
    // slot 0 — per vertex
    float3 pos     : POSITION;
    float3 normal  : NORMAL;
    float4 color   : COLOR;
    float2 uv      : TEXCOORD;
    float4 tangent : TANGENT;   // xyz + handedness(w)
    // slot 1 — per instance: world matrix rows
    float4 row0    : INSTANCETRANSFORM0;
    float4 row1    : INSTANCETRANSFORM1;
    float4 row2    : INSTANCETRANSFORM2;
    float4 row3    : INSTANCETRANSFORM3;
};

struct PS_INPUT
{
    float4 pos     : SV_POSITION;
    float3 wPos    : TEXCOORD0;
    float3 normal  : TEXCOORD1;
    float4 color   : TEXCOORD2;
    float2 uv      : TEXCOORD3;
    float4 tangent : TEXCOORD4;  // xyz + handedness(w)
};

PS_INPUT VSMain(VS_INPUT input)
{
    float4x4 world = float4x4(input.row0, input.row1, input.row2, input.row3);
    float4x4 mvp   = mul(world, gViewProj);

    PS_INPUT o;
    o.pos     = mul(float4(input.pos, 1.0f), mvp);
    o.wPos    = mul(float4(input.pos, 1.0f), world).xyz;
    o.normal  = normalize(mul(float4(input.normal, 0.0f), world).xyz);
    o.color   = input.color;
    o.uv      = input.uv;
    o.tangent = float4(normalize(mul(float4(input.tangent.xyz, 0.0f), world).xyz), input.tangent.w);
    return o;
}

float4 PSMain(PS_INPUT input) : SV_TARGET
{
    float3 geomN = normalize(input.normal);
    float3 V     = normalize(gEyePosW - input.wPos);

    float4 texColor = gDiffuse.Sample(LinearSampler, input.uv) * input.color;
    float3 albedo   = texColor.rgb * gAlbedo.rgb;

    float3 T = normalize(input.tangent.xyz - geomN * dot(geomN, input.tangent.xyz));
    float3 B = cross(geomN, T) * input.tangent.w;
    float3x3 TBN = float3x3(T, B, geomN);

    float3 normalSample = gNormalMap.Sample(LinearSampler, input.uv).rgb * 2.0f - 1.0f;
    float3 N = normalize(mul(normalSample, TBN));

    float3 Lo = float3(0.0f, 0.0f, 0.0f);

    // Directional Light
    {
        float3 L        = normalize(-gDirLight.direction);
        float3 radiance = gDirLight.color * gDirLight.intensity;
        Lo += EvalBRDF(N, V, L, albedo, gRoughness, gMetallic, radiance);
    }

    // Point Lights
    for (int i = 0; i < gNumPointLights; ++i)
    {
        float3 toLight = gPointLights[i].position - input.wPos;
        float  dist    = length(toLight);
        if (dist >= gPointLights[i].range) continue;
        float3 L        = toLight / dist;
        float  att      = CalcDistanceAttenuation(dist, gPointLights[i].range);
        float3 radiance = gPointLights[i].color * gPointLights[i].intensity * att;
        Lo += EvalBRDF(N, V, L, albedo, gRoughness, gMetallic, radiance);
    }

    // Spot Lights
    for (int j = 0; j < gNumSpotLights; ++j)
    {
        float3 toLight = gSpotLights[j].position - input.wPos;
        float  dist    = length(toLight);
        if (dist >= gSpotLights[j].range) continue;
        float3 L    = toLight / dist;
        float  spot = pow(max(dot(-L, gSpotLights[j].direction), 0.0f), gSpotLights[j].spot);
        float  att  = spot * CalcDistanceAttenuation(dist, gSpotLights[j].range);
        float3 radiance = gSpotLights[j].color * gSpotLights[j].intensity * att;
        Lo += EvalBRDF(N, V, L, albedo, gRoughness, gMetallic, radiance);
    }

    float3 ambient = float3(0.10f, 0.10f, 0.10f) * albedo * gAO;
    float3 color   = Lo + ambient + gEmissive;
    return float4(color, 1.0f);
}
