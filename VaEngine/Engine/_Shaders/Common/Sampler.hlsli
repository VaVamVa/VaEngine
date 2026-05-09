#ifndef SAMPLER_HLSLI
#define SAMPLER_HLSLI

SamplerState LinearSampler : register(s0);
SamplerState PointSampler  : register(s1);

// TODO: Depth Buffer 구현 후 활성화
// SamplerComparisonState ShadowSampler : register(s2);

#endif // SAMPLER_HLSLI
