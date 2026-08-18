#pragma pack_matrix(row_major)

#define MAX_MODEL_TRANSFORMS 250

// b0 — Directional Light 시점 view*proj (Orthographic), 정적 ShadowMap.hlsl과 공용 CB
cbuffer CB_ShadowViewProj : register(b0)
{
    float4x4 gLightViewProj;
};

// t1 — Bone Palette (BonePaletteCompute 출력, 인스턴스별 본 행렬) — GBufferSkinned.hlsl과 동일 레이아웃
//   Layout: BonePalette[(instanceID * MAX_MODEL_TRANSFORMS + boneIdx) * 4 + row]
StructuredBuffer<float4> BonePalette : register(t1);

struct VS_INPUT
{
    // slot 0 — per vertex (POSITION/BONEINDEX/BONEWEIGHT만 사용, 나머지 속성은 depth-only라 불필요)
    float3 pos        : POSITION;
    uint4  boneIndex  : BONEINDEX;
    float4 boneWeight : BONEWEIGHT;
    // slot 1 — per instance: world matrix rows
    float4 row0 : INSTANCETRANSFORM0;
    float4 row1 : INSTANCETRANSFORM1;
    float4 row2 : INSTANCETRANSFORM2;
    float4 row3 : INSTANCETRANSFORM3;
};

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

float4 VSMain(VS_INPUT input, uint instanceID : SV_InstanceID) : SV_POSITION
{
    float4x4 skinMat = (float4x4)0;
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        float4x4 boneMat = LoadSkinnedBone(instanceID, input.boneIndex[i]);
        skinMat += boneMat * input.boneWeight[i];
    }

    float4x4 world      = float4x4(input.row0, input.row1, input.row2, input.row3);
    float4   skinnedPos = mul(float4(input.pos, 1.0f), skinMat);
    float4   wPos       = mul(skinnedPos, world);
    return mul(wPos, gLightViewProj);
}

// Depth Only — Color 출력 없음. RHI가 그래픽스 PSO에 VS+PS 쌍을 요구하므로 빈 PS로 충족.
void PSMain()
{
}
