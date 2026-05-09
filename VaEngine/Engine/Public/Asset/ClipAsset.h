#pragma once

#include <cstdint>

constexpr uint32_t CLIP_MAGIC   = 0x50494C43; // 'CLIP'
constexpr uint32_t CLIP_VERSION = 2;

// 파일 레이아웃: [ClipFileHeader][str name][boneCount × str boneName][keyframes: boneCount × frameCount × 40bytes]
struct ClipFileHeader
{
    uint32_t magic;
    uint32_t version;
    float    duration;
    float    frameRate;
    uint32_t frameCount;
    uint32_t boneCount;
};
