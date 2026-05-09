#include "Mesh/UVSphereShape.h"

#include <cmath>
#include <cstring>
#include <numbers>

// DX12 CW 와인딩 (FrontCounterClockwise=FALSE)
// 쿼드 분할: (v00,v10,v01), (v10,v11,v01) — screen Y-down 기준 양의 부호면적 ✓
MeshData UVSphereShape::Build() const
{
    constexpr float PI = std::numbers::pi_v<float>;

    std::vector<PrimitiveVertex> verts;
    verts.reserve((stacks + 1) * (slices + 1));

    for (int i = 0; i <= stacks; ++i)
    {
        const float theta = PI * i / stacks;
        const float sinT  = std::sinf(theta);
        const float cosT  = std::cosf(theta);
        for (int j = 0; j <= slices; ++j)
        {
            const float phi = 2.0f * PI * j / slices;
            PrimitiveVertex v;
            v.pos[0]    = radius * sinT * std::cosf(phi);
            v.pos[1]    = radius * cosT;
            v.pos[2]    = radius * sinT * std::sinf(phi);
            v.normal[0] = sinT * std::cosf(phi);   // normalize(pos) = pos / radius
            v.normal[1] = cosT;
            v.normal[2] = sinT * std::sinf(phi);
            v.color[0]  = v.color[1] = v.color[2] = v.color[3] = 1.0f;
            v.uv[0]     = (float)j / slices;
            v.uv[1]     = (float)i / stacks;
            verts.push_back(v);
        }
    }

    std::vector<uint16_t> indices;
    indices.reserve(stacks * slices * 6);

    for (int i = 0; i < stacks; ++i)
    {
        for (int j = 0; j < slices; ++j)
        {
            const auto v00 = static_cast<uint16_t>(i       * (slices + 1) + j);
            const auto v10 = static_cast<uint16_t>((i + 1) * (slices + 1) + j);
            const auto v01 = static_cast<uint16_t>(i       * (slices + 1) + j + 1);
            const auto v11 = static_cast<uint16_t>((i + 1) * (slices + 1) + j + 1);
            indices.push_back(v00); indices.push_back(v10); indices.push_back(v01);
            indices.push_back(v10); indices.push_back(v11); indices.push_back(v01);
        }
    }

    MeshData data;
    data.vertexStride = sizeof(PrimitiveVertex);
    data.vertices.resize(verts.size() * sizeof(PrimitiveVertex));
    std::memcpy(data.vertices.data(), verts.data(), data.vertices.size());
    data.indices = std::move(indices);
    return data;
}
