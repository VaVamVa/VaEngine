#ifndef SAMPLER_HLSLI
#define SAMPLER_HLSLI

SamplerState LinearSampler : register(s0);
SamplerState PointSampler  : register(s1);

// Shadow Map PCF 비교 샘플러 — BindingLayout_DirectX.cpp가 hasTexture일 때 s0과 함께 항상 추가
SamplerComparisonState ShadowSampler : register(s2);

#endif // SAMPLER_HLSLI
