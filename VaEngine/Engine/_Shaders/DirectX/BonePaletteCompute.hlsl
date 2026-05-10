// Step 3 — 본 팔레트 GPU 오프로드.
//
// 현재 AnimationDemo.hlsl VS의 ComputeSkinMatrix + 2-clip lerp 로직을 그대로 compute로 이동.
// 인스턴스별 본별 4x4 행렬을 매 프레임 굽어 BonePalette buffer에 저장.
//
// Dispatch  : (1, instanceCount, 1)
// numthreads: (MAX_MODEL_TRANSFORMS, 1, 1) = (250, 1, 1)
// thread.x = boneIdx, thread.y(=group.y) = instanceID
//
// 출력 레이아웃 (RWStructuredBuffer<float4>):
//   BonePalette[(instanceID * MAX_MODEL_TRANSFORMS + boneIdx) * 4 + row]  (row 0~3)

#pragma pack_matrix(row_major)

#define MAX_MODEL_TRANSFORMS 250
#define MAX_MODEL_INSTANCE   500

struct AnimFrameData
{
    int   Clip;
    uint  CurFrame;
    uint  NextFrame;
    float Time;         // 프레임 간 보간값 [0, 1]
};

// EAnimBlendMode 미러 (C++ AnimController.h와 동일)
#define ANIM_MODE_TWEEN 0
#define ANIM_MODE_BLEND 1

struct TweenFrameData
{
    int           Mode;        // ANIM_MODE_TWEEN or ANIM_MODE_BLEND
    float         TweenTime;   // Tween: [0,1]
    float         BlendAlpha;  // Blend: [0,2]
    float         _Pad;
    AnimFrameData Frame0;      // Tween: Current / Blend: Clip0
    AnimFrameData Frame1;      // Tween: Next    / Blend: Clip1
    AnimFrameData Frame2;      //                  Blend: Clip2
};

// b0 — 인스턴스별 애니메이션 상태 (graphics에서 b3로 쓰던 데이터를 compute에선 b0)
cbuffer CB_TweenFrame : register(b0)
{
    TweenFrameData TweenFrames[MAX_MODEL_INSTANCE];
};

// t0 — 본 변환 행렬 배열 (정적 애니 데이터)
// Width  = boneCount * 4
// Height = frameCount
// Array  = clipIndex
Texture2DArray TransformsMap : register(t0);

// u0 — 본 팔레트 출력 (RWStructuredBuffer<float4>)
RWStructuredBuffer<float4> BonePalette : register(u0);

// 단일 클립/프레임에서 본 행렬 1개 로드
float4x4 LoadFrameMatrix(int clip, uint frame, uint boneIdx)
{
    int x = (int)(boneIdx * 4);
    int y = (int)frame;
    int z = clip;
    float4 r0 = TransformsMap.Load(int4(x + 0, y, z, 0));
    float4 r1 = TransformsMap.Load(int4(x + 1, y, z, 0));
    float4 r2 = TransformsMap.Load(int4(x + 2, y, z, 0));
    float4 r3 = TransformsMap.Load(int4(x + 3, y, z, 0));
    return float4x4(r0, r1, r2, r3);
}

// 단일 AnimFrameData에서 (Cur ↔ Next frame) lerp된 본 행렬 계산
float4x4 ComputeSkinMatrixForBone(AnimFrameData anim, uint boneIdx)
{
    float4x4 cur  = LoadFrameMatrix(anim.Clip, anim.CurFrame,  boneIdx);
    float4x4 next = LoadFrameMatrix(anim.Clip, anim.NextFrame, boneIdx);
    return lerp(cur, next, anim.Time);
}

[numthreads(MAX_MODEL_TRANSFORMS, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    const uint boneIdx    = dtid.x;
    const uint instanceID = dtid.y;

    TweenFrameData tween = TweenFrames[instanceID];

    float4x4 skinMat = (float4x4)0;
    if (tween.Mode == ANIM_MODE_BLEND)
    {
        float a  = clamp(tween.BlendAlpha, 0.0f, 2.0f);
        float4x4 m0 = ComputeSkinMatrixForBone(tween.Frame0, boneIdx);
        float4x4 m1 = ComputeSkinMatrixForBone(tween.Frame1, boneIdx);
        float4x4 m2 = ComputeSkinMatrixForBone(tween.Frame2, boneIdx);
        skinMat = (a <= 1.0f) ? lerp(m0, m1, a) : lerp(m1, m2, a - 1.0f);
    }
    else
    {
        skinMat = ComputeSkinMatrixForBone(tween.Frame0, boneIdx);
        if (tween.TweenTime > 0.0f)
            skinMat = lerp(skinMat, ComputeSkinMatrixForBone(tween.Frame1, boneIdx), tween.TweenTime);
    }

    // 출력: 4 row × float4
    const uint baseIdx = (instanceID * MAX_MODEL_TRANSFORMS + boneIdx) * 4;
    BonePalette[baseIdx + 0] = skinMat[0];
    BonePalette[baseIdx + 1] = skinMat[1];
    BonePalette[baseIdx + 2] = skinMat[2];
    BonePalette[baseIdx + 3] = skinMat[3];
}
