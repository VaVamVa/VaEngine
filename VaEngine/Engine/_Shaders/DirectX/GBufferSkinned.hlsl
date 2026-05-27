#include "../Common/Sampler.hlsli"

#pragma pack_matrix(row_major)

#define MAX_MODEL_TRANSFORMS 250

// b0 — per-frame ViewProj
cbuffer CB_ViewProj : register(b0)
{
    float4x4 gViewProj;
};

// b1 — G-Buffer material constants
cbuffer CB_GBufferMaterial : register(b1)
{
    float  gRoughness;
    float  gMetallic;
    float2 _matPad;
};

// t0 — Diffuse texture
Texture2D gDiffuse : register(t0);

// t1 — Bone Palette (BonePaletteCompute 출력, 인스턴스별 본 행렬)
//   Layout: BonePalette[(instanceID * MAX_MODEL_TRANSFORMS + boneIdx) * 4 + row]
StructuredBuffer<float4> BonePalette : register(t1);

// ── 입력 구조체 ──────────────────────────────────────────────────────────────

struct VS_INPUT
{
    // slot 0 — per vertex (SkinnedVertex)
    float3 pos        : POSITION;
    float3 normal     : NORMAL;
    float4 color      : COLOR;
    float2 uv         : TEXCOORD;
    uint4  boneIndex  : BONEINDEX;
    float4 boneWeight : BONEWEIGHT;
    // slot 1 — per instance (world matrix rows)
    float4 row0       : INSTANCETRANSFORM0;
    float4 row1       : INSTANCETRANSFORM1;
    float4 row2       : INSTANCETRANSFORM2;
    float4 row3       : INSTANCETRANSFORM3;
};

struct PS_INPUT
{
    float4 pos    : SV_POSITION;
    float3 wPos   : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float4 color  : TEXCOORD2;
    float2 uv     : TEXCOORD3;
};

// ── MRT 출력 ─────────────────────────────────────────────────────────────────
// RT0: R8G8B8A8_UNORM    — Albedo(RGB) + AO(A)
// RT1: R16G16B16A16_FLOAT — WorldNormal(XYZ) + Roughness(W)
// RT2: R8G8B8A8_UNORM    — Metallic(R) + 예약(GBA)

struct GBuffer_OUT
{
    float4 rt0 : SV_TARGET0;
    float4 rt1 : SV_TARGET1;
    float4 rt2 : SV_TARGET2;
};

// ── 헬퍼 ─────────────────────────────────────────────────────────────────────

float4x4 LoadSkinnedBone(uint instanceID, uint boneIdx)
{
    uint base = (instanceID * MAX_MODEL_TRANSFORMS + boneIdx) * 4;
    return float4x4(
        BonePalette[base + 0],
        BonePalette[base + 1],
        BonePalette[base + 2],
        BonePalette[base + 3]
    );
}

// ── Vertex Shader ─────────────────────────────────────────────────────────────

PS_INPUT VSMain(VS_INPUT input, uint instanceID : SV_InstanceID)
{
    float4x4 skinMat = (float4x4)0;
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        float4x4 boneMat = LoadSkinnedBone(instanceID, input.boneIndex[i]);
        skinMat += boneMat * input.boneWeight[i];
    }

    float4x4 world        = float4x4(input.row0, input.row1, input.row2, input.row3);
    float4   skinnedPos    = mul(float4(input.pos,    1.0f), skinMat);
    float4   skinnedNormal = mul(float4(input.normal, 0.0f), skinMat);

    PS_INPUT o;
    o.pos    = mul(mul(skinnedPos,    world), gViewProj);
    o.wPos   = mul(skinnedPos,    world).xyz;
    o.normal = normalize(mul(skinnedNormal, world).xyz);
    o.color  = input.color;
    o.uv     = input.uv;
    return o;
}

// ── Pixel Shader ──────────────────────────────────────────────────────────────

GBuffer_OUT PSMain(PS_INPUT input)
{
    float3 N      = normalize(input.normal);
    float4 albedo = gDiffuse.Sample(LinearSampler, input.uv) * input.color;

    GBuffer_OUT o;
    o.rt0 = float4(albedo.rgb, 1.0f);
    o.rt1 = float4(N, gRoughness);
    o.rt2 = float4(gMetallic, 0.0f, 0.0f, 0.0f);
    return o;
}
