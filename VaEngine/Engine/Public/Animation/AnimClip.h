#pragma once

#include "Math/Container.h"

#include <string>
#include <vector>

struct KeyframeEntry
{
    Vector3    scale;
    Quaternion rotation;
    Vector3    translation;
};

struct AnimClipData
{
    std::string name;
    float       duration   = 0.0f;
    float       frameRate  = 30.0f;
    uint32_t    frameCount = 0;
    uint32_t    boneCount  = 0;

    std::vector<std::string>                boneNames;  // [boneIndex]
    std::vector<std::vector<KeyframeEntry>> keyframes;  // [boneIndex][frameIndex]
};
