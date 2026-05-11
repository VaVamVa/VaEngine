#include "Mesh/IcoSphereShape.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <map>
#include <numbers>

// DX12 CW 와인딩 (FrontCounterClockwise=FALSE)
// 정20면체 → N회 분할 후 구면 투영. 분할은 CW 와인딩 유지.
// UV: 구면 좌표 (phi=atan2, theta=acos) — phi=±π 경계에 심(seam) 발생
MeshData IcoSphereShape::Build() const
{
    constexpr float PI = std::numbers::pi_v<float>;
    const float     t  = (1.0f + std::sqrtf(5.0f)) * 0.5f; // 황금비

    using Pos3  = std::array<float, 3>;
    using Face  = std::array<int,   3>;

    auto norm = [this](float x, float y, float z) -> Pos3
    {
        float len = std::sqrtf(x * x + y * y + z * z);
        return { x / len * radius, y / len * radius, z / len * radius };
    };

    std::vector<Pos3> positions = {
        norm(-1,+t, 0), norm(+1,+t, 0), norm(-1,-t, 0), norm(+1,-t, 0),
        norm( 0,-1,+t), norm( 0,+1,+t), norm( 0,-1,-t), norm( 0,+1,-t),
        norm(+t, 0,-1), norm(+t, 0,+1), norm(-t, 0,-1), norm(-t, 0,+1)
    };

    // DX12 CW: 표준 CCW 정20면체 면 목록을 좌우 반전
    std::vector<Face> faces = {
        {0,5,11}, {0,1,5},  {0,7,1},  {0,10,7},  {0,11,10},
        {1,9,5},  {5,4,11}, {11,2,10},{10,6,7},   {7,8,1},
        {3,4,9},  {3,2,4},  {3,6,2},  {3,8,6},    {3,9,8},
        {4,5,9},  {2,11,4}, {6,10,2}, {8,7,6},    {9,1,8}
    };

    for (int lv = 0; lv < subdivisions; ++lv)
    {
        std::map<std::pair<int,int>, int> cache;

        auto mid = [&](int a, int b) -> int
        {
            auto key = (a < b) ? std::make_pair(a, b) : std::make_pair(b, a);
            if (auto it = cache.find(key); it != cache.end()) return it->second;
            const Pos3& pa = positions[a];
            const Pos3& pb = positions[b];
            float mx = (pa[0] + pb[0]) * 0.5f;
            float my = (pa[1] + pb[1]) * 0.5f;
            float mz = (pa[2] + pb[2]) * 0.5f;
            float len = std::sqrtf(mx * mx + my * my + mz * mz);
            positions.push_back({ mx / len * radius, my / len * radius, mz / len * radius });
            return cache[key] = static_cast<int>(positions.size()) - 1;
        };

        std::vector<Face> next;
        next.reserve(faces.size() * 4);
        for (const Face& f : faces)
        {
            int ab = mid(f[0], f[1]);
            int bc = mid(f[1], f[2]);
            int ca = mid(f[2], f[0]);
            next.push_back({ f[0], ab, ca });
            next.push_back({ ab,  f[1], bc });
            next.push_back({ ca,  bc,  f[2] });
            next.push_back({ ab,  bc,  ca });
        }
        faces = std::move(next);
    }

    std::vector<PrimitiveVertex> verts;
    verts.reserve(positions.size());
    for (const Pos3& p : positions)
    {
        PrimitiveVertex v;
        v.pos[0]    = p[0]; v.pos[1] = p[1]; v.pos[2] = p[2];
        v.normal[0] = p[0] / radius;           // 구면 위의 점: normalize(pos)
        v.normal[1] = p[1] / radius;
        v.normal[2] = p[2] / radius;
        v.color[0]  = v.color[1] = v.color[2] = v.color[3] = 1.0f;
        v.uv[0]    = 0.5f + std::atan2f(p[2], p[0]) / (2.0f * PI);
        v.uv[1]    = std::acosf(std::clamp(p[1] / radius, -1.0f, 1.0f)) / PI;
        verts.push_back(v);
    }

    std::vector<uint16_t> indices;
    indices.reserve(faces.size() * 3);
    for (const Face& f : faces)
    {
        indices.push_back(static_cast<uint16_t>(f[0]));
        indices.push_back(static_cast<uint16_t>(f[1]));
        indices.push_back(static_cast<uint16_t>(f[2]));
    }

    MeshData data;
    data.vertexStride = sizeof(PrimitiveVertex);
    data.vertices.resize(verts.size() * sizeof(PrimitiveVertex));
    std::memcpy(data.vertices.data(), verts.data(), data.vertices.size());
    data.indices = std::move(indices);
    return data;
}
