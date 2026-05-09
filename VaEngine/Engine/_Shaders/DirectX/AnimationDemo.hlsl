#include "../Common/Lighting.hlsli"

#pragma pack_matrix(row_major)

#define MAX_MODEL_TRANSFORMS 250
#define MAX_MODEL_INSTANCE   500

// b0 — per-frame ViewProj
cbuffer CB_ViewProj : register(b0)
{
    float4x4 gViewProj;
};

// b2 — CB_Lights (Lighting.hlsli 정의)

// b3 — 인스턴스별 애니메이션 상태
struct AnimFrameData
{
    int   Clip;
    uint  CurFrame;
    uint  NextFrame;
    float Time;         // 프레임 간 보간값 [0, 1]
};

struct TweenFrameData
{
    float         TweenTime;  // 전환 진행도 [0, 1] — 0=Current만, 1=Next만
    float3        _Pad;
    AnimFrameData Current;
    AnimFrameData Next;
};

cbuffer CB_TweenFrame : register(b3)
{
    TweenFrameData TweenFrames[MAX_MODEL_INSTANCE];
};

// t0 — Diffuse 텍스처
Texture2D gDiffuse : register(t0);

// t1 — 본 변환 행렬 배열 (R32G32B32A32_FLOAT)
// Width  = boneCount * 4  (행렬 1개 = float4 행 × 4)
// Height = frameCount
// Array  = clipIndex
Texture2DArray TransformsMap : register(t1);

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

// TransformsMap에서 4x4 본 행렬 로드
float4x4 LoadBoneMatrix(int clip, uint frame, uint boneIdx)
{
    int y = (int)frame;
    int x = (int)(boneIdx * 4);
    int z = clip;
    float4 r0 = TransformsMap.Load(int4(x + 0, y, z, 0));
    float4 r1 = TransformsMap.Load(int4(x + 1, y, z, 0));
    float4 r2 = TransformsMap.Load(int4(x + 2, y, z, 0));
    float4 r3 = TransformsMap.Load(int4(x + 3, y, z, 0));
    return float4x4(r0, r1, r2, r3);
}

// 하나의 AnimFrameData에서 스키닝 행렬 계산 (4본 가중합)
float4x4 ComputeSkinMatrix(AnimFrameData anim, uint4 boneIdx, float4 boneWeight)
{
    float4x4 result = (float4x4)0;
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        float4x4 curMat  = LoadBoneMatrix(anim.Clip, anim.CurFrame,  boneIdx[i]);
        float4x4 nextMat = LoadBoneMatrix(anim.Clip, anim.NextFrame, boneIdx[i]);
        float4x4 boneMat = lerp(curMat, nextMat, anim.Time);
        result += boneMat * boneWeight[i];
    }
    return result;
}

// ── Vertex Shader ─────────────────────────────────────────────────────────────

PS_INPUT VSMain(VS_INPUT input, uint instanceID : SV_InstanceID)
{
    TweenFrameData tween = TweenFrames[instanceID];

    float4x4 skinMat = ComputeSkinMatrix(tween.Current, input.boneIndex, input.boneWeight);
    if (tween.TweenTime > 0.0f)
    {
        float4x4 nextSkinMat = ComputeSkinMatrix(tween.Next, input.boneIndex, input.boneWeight);
        skinMat = lerp(skinMat, nextSkinMat, tween.TweenTime);
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
