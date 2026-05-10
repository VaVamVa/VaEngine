#include "Animation/AnimController.h"
#include "Animation/AnimClip.h"

#include <algorithm>

static AnimFrameDesc AdvanceClip(uint32_t clipIdx, float& runningTime, float speed,
                                  float deltaTime, const std::vector<AnimClipData>& clips)
{
    const AnimClipData& clip = clips[clipIdx];
    runningTime += deltaTime * speed;
    runningTime  = std::fmod(runningTime, clip.duration);

    const float    tpf  = clip.duration / static_cast<float>(clip.frameCount - 1);
    const uint32_t cur  = static_cast<uint32_t>(runningTime / tpf);
    const uint32_t next = std::min(cur + 1, clip.frameCount - 1);
    const float    t    = (runningTime - cur * tpf) / tpf;

    AnimFrameDesc desc;
    desc.clip      = static_cast<int32_t>(clipIdx);
    desc.curFrame  = cur;
    desc.nextFrame = next;
    desc.time      = t;
    return desc;
}

void AnimController::Resize(uint32_t instanceCount)
{
    states.assign(instanceCount, {});
    gpuData.assign(instanceCount, {});
}

void AnimController::Play(uint32_t instance, uint32_t clip, float speed)
{
    if (instance >= states.size()) return;
    auto& s       = states[instance];
    s.mode        = EAnimBlendMode::Tween;
    s.clip        = clip;
    s.speed       = speed;
    s.runningTime = 0.0f;
    s.blendTime   = 0.0f;
    s.tweenTime   = 0.0f;
}

void AnimController::PlayTween(uint32_t instance, uint32_t nextClip, float blendTime, float speed)
{
    if (instance >= states.size()) return;
    auto& s       = states[instance];
    s.mode        = EAnimBlendMode::Tween;
    s.nextClip    = nextClip;
    s.nextSpeed   = speed;
    s.nextRunTime = 0.0f;
    s.tweenTime   = 0.0f;
    s.blendTime   = blendTime;
}

void AnimController::PlayBlend(uint32_t instance,
                                uint32_t clip0, uint32_t clip1, uint32_t clip2,
                                float speed0, float speed1, float speed2)
{
    if (instance >= states.size()) return;
    auto& s       = states[instance];
    s.mode        = EAnimBlendMode::Blend;
    s.clip        = clip0;
    s.speed       = speed0;
    s.runningTime = 0.0f;
    s.blendClip1  = clip1;
    s.speed1      = speed1;
    s.runTime1    = 0.0f;
    s.blendClip2  = clip2;
    s.speed2      = speed2;
    s.runTime2    = 0.0f;
    s.blendAlpha  = 0.0f;
    s.blendTime   = 0.0f;
    s.tweenTime   = 0.0f;
}

void AnimController::SetBlendAlpha(uint32_t instance, float alpha)
{
    if (instance >= states.size()) return;
    states[instance].blendAlpha = std::clamp(alpha, 0.0f, 2.0f);
}

void AnimController::Update(float deltaTime, const std::vector<AnimClipData>& clips)
{
    const auto clipCount = static_cast<uint32_t>(clips.size());

    for (uint32_t i = 0; i < static_cast<uint32_t>(states.size()); ++i)
    {
        auto& s   = states[i];
        auto& gpu = gpuData[i];

        if (s.mode == EAnimBlendMode::Blend)
        {
            gpu.mode       = EAnimBlendMode::Blend;
            gpu.blendAlpha = s.blendAlpha;
            gpu.tweenTime  = 0.0f;

            if (s.clip < clipCount && clips[s.clip].frameCount >= 2)
                gpu.frame0 = AdvanceClip(s.clip, s.runningTime, s.speed, deltaTime, clips);
            if (s.blendClip1 < clipCount && clips[s.blendClip1].frameCount >= 2)
                gpu.frame1 = AdvanceClip(s.blendClip1, s.runTime1, s.speed1, deltaTime, clips);
            if (s.blendClip2 < clipCount && clips[s.blendClip2].frameCount >= 2)
                gpu.frame2 = AdvanceClip(s.blendClip2, s.runTime2, s.speed2, deltaTime, clips);
        }
        else
        {
            if (s.clip >= clipCount) continue;
            if (clips[s.clip].frameCount < 2) continue;

            gpu.mode       = EAnimBlendMode::Tween;
            gpu.blendAlpha = 0.0f;
            gpu.frame0     = AdvanceClip(s.clip, s.runningTime, s.speed, deltaTime, clips);

            if (s.blendTime > 0.0f)
            {
                s.tweenTime = std::min(s.tweenTime + deltaTime / s.blendTime, 1.0f);

                if (s.nextClip < clipCount && clips[s.nextClip].frameCount >= 2)
                    gpu.frame1 = AdvanceClip(s.nextClip, s.nextRunTime, s.nextSpeed, deltaTime, clips);

                if (s.tweenTime >= 1.0f)
                {
                    s.clip        = s.nextClip;
                    s.runningTime = s.nextRunTime;
                    s.speed       = s.nextSpeed;
                    s.blendTime   = 0.0f;
                    s.tweenTime   = 0.0f;
                    gpu.frame0    = gpu.frame1;
                    gpu.tweenTime = 0.0f;
                }
                else
                {
                    gpu.tweenTime = s.tweenTime;
                }
            }
            else
            {
                gpu.tweenTime = 0.0f;
                gpu.frame1    = gpu.frame0;
            }
        }
    }
}
