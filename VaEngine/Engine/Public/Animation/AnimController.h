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

struct TweenFrameDesc
{
    float         tweenTime = 0.0f;  // 전환 진행도 [0, 1] — 0=current만, 1=next만
    float         _pad[3]   = {};
    AnimFrameDesc current;
    AnimFrameDesc next;
};

class AnimController
{
public:
    void Resize(uint32_t instanceCount);

    // 클립 즉시 전환
    void Play(uint32_t instance, uint32_t clip, float speed = 1.0f);

    // blendTime 초 동안 nextClip으로 부드럽게 전환
    void PlayTween(uint32_t instance, uint32_t nextClip, float blendTime, float speed = 1.0f);

    // 매 프레임 호출 — 프레임 인덱스 및 보간값 갱신
    void Update(float deltaTime, const std::vector<AnimClipData>& clips);

    const std::vector<TweenFrameDesc>& GetTweenFrames() const { return gpuData; }
    uint32_t InstanceCount() const { return static_cast<uint32_t>(states.size()); }

private:
    struct InstanceState
    {
        uint32_t clip        = 0;
        float    speed       = 1.0f;
        float    runningTime = 0.0f;

        uint32_t nextClip    = 0;
        float    nextSpeed   = 1.0f;
        float    nextRunTime = 0.0f;
        float    tweenTime   = 0.0f;  // 전환 진행도 (0~1)
        float    blendTime   = 0.0f;  // 전환 총 시간 (0 = 비활성)
    };

    std::vector<InstanceState>  states;
    std::vector<TweenFrameDesc> gpuData;
};
