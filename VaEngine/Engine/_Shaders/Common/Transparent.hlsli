#ifndef TRANSPARENT_HLSLI
#define TRANSPARENT_HLSLI

// 반투명 오브젝트용 PBR 셰이더 (플랫폼 중립)
// 래퍼: DirectX/ForwardTransparent.hlsl (DXIL)
//       Vulkan/ForwardTransparent.hlsl  (SPIRV, 추후)

#include "Lighting.hlsli"
#include "GBufferMaterial.hlsli"

cbuffer CB_ViewProj : register(b0)
{
    float4x4 gViewProj;
};

// b1 — CB_GBufferMaterial (GBufferMaterial.hlsli)
// b2 — CB_Lights           (Lighting.hlsli)
// t0 — albedo texture
// t1 — Normal Map (없으면 기본 (128,128,255) tangent-up 텍스처)
// t2~t4 — IBL 산출물 (Diffuse Irradiance / Specular Prefiltered / BRDF LUT), append 신규
Texture2D gDiffuse : register(t0);
Texture2D gNormalMap : register(t1);
Texture2D<float4> gIrradiance  : register(t2);
Texture2D<float4> gPrefiltered : register(t3);
Texture2D<float2> gBRDFLUT     : register(t4);

struct VS_INPUT
{
    float3 pos     : POSITION;
    float3 normal  : NORMAL;
    float4 color   : COLOR;
    float2 uv      : TEXCOORD;
    float4 tangent : TANGENT;   // xyz + handedness(w)
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

// Weighted Blended OIT (McGuire & Bavoil 2013) 출력 — accum(가산)·revealage(곱셈)를 한 Pass에서
// 동시에 씀. 정렬 없이 여러 겹의 투명 프래그먼트를 합성 가능(현재 2-Pass CullMode의 비볼록·
// 교차 지오메트리·다중 오브젝트 정렬 오류를 구조적으로 해소 — 2026-07-12_Q&A.md Q1 참조).
struct PS_OUTPUT
{
    float4 accum     : SV_TARGET0;
    float  revealage : SV_TARGET1;
};

PS_OUTPUT PSMain(PS_INPUT input)
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

    float3 ambient = EvalAmbientIBL(N, V, albedo, gRoughness, gMetallic, gAO,
                                    gIrradiance, gPrefiltered, gBRDFLUT);
    float3 color = Lo + ambient + gEmissive;
    float  alpha = texColor.a * gAlbedo.a;

    // McGuire의 NDC-depth 기반 weight function(논문 제시 변형 중 하나) — input.pos.z는 DX
    // 컨벤션상 이미 [0,1] 범위의 NDC depth라 별도 선형화 없이 그대로 사용 가능.
    float depth  = input.pos.z;
    float weight = clamp(pow(min(1.0f, alpha * 10.0f) + 0.01f, 3.0f)
                        * 1e8f * pow(1.0f - depth * 0.9f, 3.0f), 1e-2f, 3e3f);

    PS_OUTPUT o;
    o.accum     = float4(color * alpha, alpha) * weight;
    o.revealage = 1.0f - alpha;  // (1,1,1,1)에서 시작해 곱셈 블렌드로 Π(1-alpha_i) 수렴
    return o;
}

#endif // TRANSPARENT_HLSLI
