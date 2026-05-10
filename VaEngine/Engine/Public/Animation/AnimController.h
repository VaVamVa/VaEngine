#pragma once

#include <cstdint>
#include <vector>

struct AnimClipData;

// GPU cbuffer와 1:1 매핑 (AnimationDemo.hlsl CB_TweenFrame)
struct AnimFrameDesc
{
    int32_t  clip      = 0;
    uint32_t curFrame  = 0;
    uint32_t nextFrame = 0;
    float    time      = 0.0f;  // 프레임 간 보간값 [0, 1]
};

enum class EAnimBlendMode : int32_t
{
    Tween = 0,  // 2-clip cross-fade
    Blend = 1,  // 3-clip weighted blend (alpha [0,2])
};

struct TweenFrameDesc
{
    EAnimBlendMode mode       = EAnimBlendMode::Tween;
    float          tweenTime  = 0.0f;  // Tween: [0,1]
    float          blendAlpha = 0.0f;  // Blend: [0,2]
    float          _pad       = 0.0f;
    AnimFrameDesc  frame0;             // Tween: current / Blend: clip0
    AnimFrameDesc  frame1;             // Tween: next    / Blend: clip1
    AnimFrameDesc  frame2;             //                  Blend: clip2
};

class AnimController
{
public:
    void Resize(uint32_t instanceCount);

    // 클립 즉시 전환
    void Play(uint32_t instance, uint32_t clip, float speed = 1.0f);

    // blendTime 초 동안 nextClip으로 부드럽게 전환
    void PlayTween(uint32_t instance, uint32_t nextClip, float blendTime, float speed = 1.0f);

    // 3-clip 가중치 합성 시작 — alpha [0,2]는 SetBlendAlpha로 외부 제어
    void PlayBlend(uint32_t instance,
                   uint32_t clip0, uint32_t clip1, uint32_t clip2,
                   float speed0 = 1.0f, float speed1 = 1.0f, float speed2 = 1.0f);

    // Blend 모드의 alpha 값 설정 ([0,2]: 0↔1 = clip0↔1, 1↔2 = clip1↔2)
    void SetBlendAlpha(uint32_t instance, float alpha);

    // 매 프레임 호출 — 프레임 인덱스 및 보간값 갱신
    void Update(float deltaTime, const std::vector<AnimClipData>& clips);

    const std::vector<TweenFrameDesc>& GetTweenFrames() const { return gpuData; }
    uint32_t InstanceCount() const { return static_cast<uint32_t>(states.size()); }

private:
    struct InstanceState
    {
        EAnimBlendMode mode        = EAnimBlendMode::Tween;

        // Tween 모드
        uint32_t clip        = 0;
        float    speed       = 1.0f;
        float    runningTime = 0.0f;
        uint32_t nextClip    = 0;
        float    nextSpeed   = 1.0f;
        float    nextRunTime = 0.0f;
        float    tweenTime   = 0.0f;
        float    blendTime   = 0.0f;

        // Blend 모드
        uint32_t blendClip1  = 0;
        uint32_t blendClip2  = 0;
        float    runTime1    = 0.0f;
        float    runTime2    = 0.0f;
        float    speed1      = 1.0f;
        float    speed2      = 1.0f;
        float    blendAlpha  = 0.0f;  // [0, 2]
    };

    std::vector<InstanceState>  states;
    std::vector<TweenFrameDesc> gpuData;
};
