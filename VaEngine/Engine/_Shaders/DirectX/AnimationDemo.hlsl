#include "../Common/Lighting.hlsli"

#pragma pack_matrix(row_major)

#define MAX_MODEL_TRANSFORMS 250

// b0 — per-frame ViewProj
cbuffer CB_ViewProj : register(b0)
{
    float4x4 gViewProj;
};

// b2 — CB_Lights (Lighting.hlsli 정의)

// t0 — Diffuse 텍스처
Texture2D gDiffuse : register(t0);

// t1 — Bone Palette: BonePaletteCompute가 매 프레임 굽는 인스턴스별 본 행렬
//   Layout: BonePalette[(instanceID * MAX_MODEL_TRANSFORMS + boneIdx) * 4 + row]   (row 0~3)
StructuredBuffer<float4> BonePalette : register(t1);

// ── 입력 구조체 ──────────────────────────────────────────────────────────────

struct VS_INPUT
{
    // slot 0 — per vertex (SkinnedVertex)
    float3   pos         : POSITION;
    float3   normal      : NORMAL;
    float4   color       : COLOR;
    float2   uv          : TEXCOORD;
    uint4    boneIndex   : BONEINDEX;
    float4   boneWeight  : BONEWEIGHT;
    // slot 1 — per instance (world matrix rows)
    float4   row0        : INSTANCETRANSFORM0;
    float4   row1        : INSTANCETRANSFORM1;
    float4   row2        : INSTANCETRANSFORM2;
    float4   row3        : INSTANCETRANSFORM3;
};

struct PS_INPUT
{
    float4 pos    : SV_POSITION;
    float3 wPos   : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float4 color  : TEXCOORD2;
    float2 uv     : TEXCOORD3;
};

// ── 헬퍼 ─────────────────────────────────────────────────────────────────────

// BonePalette에서 4x4 본 행렬 로드 (compute가 이미 lerp 처리한 결과)
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
    // 4본 가중합 — lerp/tween은 compute에서 이미 처리됨
    float4x4 skinMat = (float4x4)0;
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        float4x4 boneMat = LoadSkinnedBone(instanceID, input.boneIndex[i]);
        skinMat += boneMat * input.boneWeight[i];
    }

    float4x4 world = float4x4(input.row0, input.row1, input.row2, input.row3);
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

float4 PSMain(PS_INPUT input) : SV_TARGET
{
    float3 normal = normalize(input.normal);
    float3 toEye  = normalize(gEyePosW - input.wPos);

    float4 ambient = float4(0, 0, 0, 0);
    float4 diffuse = float4(0, 0, 0, 0);
    float4 spec    = float4(0, 0, 0, 0);
    float4 A, D, S;

    ComputeDirectionalLight(gMaterial, gDirLight, normal, toEye, A, D, S);
    ambient += A; diffuse += D; spec += S;

    for (int i = 0; i < gNumPointLights; ++i)
    {
        ComputePointLight(gMaterial, gPointLights[i], input.wPos, normal, toEye, A, D, S);
        ambient += A; diffuse += D; spec += S;
    }

    for (int j = 0; j < gNumSpotLights; ++j)
    {
        ComputeSpotLight(gMaterial, gSpotLights[j], input.wPos, normal, toEye, A, D, S);
        ambient += A; diffuse += D; spec += S;
    }

    float4 texColor = gDiffuse.Sample(LinearSampler, input.uv) * input.color;
    float4 litColor = texColor * (ambient + diffuse) + spec;
    litColor.a      = texColor.a * gMaterial.diffuse.a;
    return litColor;
}
