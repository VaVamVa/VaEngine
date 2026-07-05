#ifndef GBUFFER_MATERIAL_HLSLI
#define GBUFFER_MATERIAL_HLSLI

// CB_GBufferMaterial (b1) — IMaterial.h :: MaterialData (48 bytes) 와 레이아웃 1:1
// 필드 순서·타입 변경 시 C++ MaterialData 와 반드시 동기화.
cbuffer CB_GBufferMaterial : register(b1)
{
    float4 gAlbedo;         // albedo[4]:      base color tint (.a = alpha, Cutout 기준)
    float3 gEmissive;       // emissive[3]:    자체 발광
    float  gRoughness;      // roughness:      PBR 거칠기 [0,1]
    float  gMetallic;       // metallic:       PBR 금속도 [0,1]
    float  gAO;             // ao:             앰비언트 오클루전 [0,1]
    float  gAlphaThreshold; // alphaThreshold: Cutout clip() 기준값. 0 = 비활성
    float  _matPad;
};

#endif // GBUFFER_MATERIAL_HLSLI
