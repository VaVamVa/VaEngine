#pragma once

#include <cstdint>
#include <vector>

struct PrimitiveVertex
{
    float pos[3];     // offset  0
    float normal[3];  // offset 12
    float color[4];   // offset 24
    float uv[2];      // offset 40
    // stride: 48 bytes
};

struct MeshData
{
    std::vector<uint8_t>  vertices;
    std::vector<uint16_t> indices;
    uint32_t              vertexStride = 0;
};
