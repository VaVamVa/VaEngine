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
    float    tangent[4];     // offset 80 — xyz + handedness(w)
    // stride: 96 bytes
};
