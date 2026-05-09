#pragma once

#include <cstdint>

struct SkinnedVertex
{
    float    pos[3];         // offset  0
    float    normal[3];      // offset 12
    float    color[4];       // offset 24
    float    uv[2];          // offset 40
    uint32_t boneIndex[4];   // offset 48
    float    boneWeight[4];  // offset 64
    // stride: 80 bytes
};
