#ifndef IMPORTANCE_SAMPLING_HLSLI
#define IMPORTANCE_SAMPLING_HLSLI

#include "Lighting.hlsli"  // PI — include guard로 중복 정의 없이 안전

// GGX 중요도 샘플링 — Specular Prefiltered Map 컨볼루션(PrefilterSpecular.hlsl)과
// BRDF LUT 적분(IntegrateBRDF.hlsl)이 공유한다 (Karis 2013 split-sum 근사).

// Van der Corput 수열 (비트 반전 기반 저불일치 수열)
float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10f;  // / 0x100000000
}

// Hammersley 2D 저불일치 시퀀스 — i번째 샘플, 전체 N개
float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

// 접선 공간 GGX 분포를 따르는 half-vector를 world 공간으로 변환해 반환
// (up-vector 기반 TBN — IrradianceConvolve.hlsl과 동일한 관례)
float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
    float a = max(roughness * roughness, 0.0001f);

    float phi      = 2.0f * PI * Xi.x;
    float cosTheta = sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
    float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

    float3 H = float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

    float3 up    = (abs(N.y) > 0.99f) ? float3(0.0f, 0.0f, 1.0f) : float3(0.0f, 1.0f, 0.0f);
    float3 right = normalize(cross(up, N));
    up           = normalize(cross(N, right));

    return normalize(right * H.x + up * H.y + N * H.z);
}

#endif // IMPORTANCE_SAMPLING_HLSLI
