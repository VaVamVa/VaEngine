#include "Mesh/PrimitiveShape.h"

#include <cmath>
#include <vector>

// Lengyel의 tangent 계산법: 삼각형의 pos/uv 델타로 tangent(T)/bitangent(B) 방향을 구하고,
// 정점별로 누적 후 normal에 대해 Gram-Schmidt 직교화. handedness(w)는 cross(N,T)와 B의 내적 부호.
void PrimitiveShape::ComputeTangents(PrimitiveVertex* verts, size_t vertexCount,
                                      const uint16_t* indices, size_t indexCount)
{
    std::vector<float> tan1x(vertexCount, 0.0f), tan1y(vertexCount, 0.0f), tan1z(vertexCount, 0.0f);
    std::vector<float> tan2x(vertexCount, 0.0f), tan2y(vertexCount, 0.0f), tan2z(vertexCount, 0.0f);

    for (size_t i = 0; i + 2 < indexCount; i += 3)
    {
        const uint16_t i0 = indices[i];
        const uint16_t i1 = indices[i + 1];
        const uint16_t i2 = indices[i + 2];

        const PrimitiveVertex& v0 = verts[i0];
        const PrimitiveVertex& v1 = verts[i1];
        const PrimitiveVertex& v2 = verts[i2];

        const float x1 = v1.pos[0] - v0.pos[0], y1 = v1.pos[1] - v0.pos[1], z1 = v1.pos[2] - v0.pos[2];
        const float x2 = v2.pos[0] - v0.pos[0], y2 = v2.pos[1] - v0.pos[1], z2 = v2.pos[2] - v0.pos[2];

        const float s1 = v1.uv[0] - v0.uv[0], t1 = v1.uv[1] - v0.uv[1];
        const float s2 = v2.uv[0] - v0.uv[0], t2 = v2.uv[1] - v0.uv[1];

        const float denom = s1 * t2 - s2 * t1;
        const float r     = (std::fabs(denom) < 1e-8f) ? 0.0f : 1.0f / denom;

        const float sx = (t2 * x1 - t1 * x2) * r;
        const float sy = (t2 * y1 - t1 * y2) * r;
        const float sz = (t2 * z1 - t1 * z2) * r;
        const float tx = (s1 * x2 - s2 * x1) * r;
        const float ty = (s1 * y2 - s2 * y1) * r;
        const float tz = (s1 * z2 - s2 * z1) * r;

        const uint16_t tri[3] = { i0, i1, i2 };
        for (uint16_t idx : tri)
        {
            tan1x[idx] += sx; tan1y[idx] += sy; tan1z[idx] += sz;
            tan2x[idx] += tx; tan2y[idx] += ty; tan2z[idx] += tz;
        }
    }

    for (size_t i = 0; i < vertexCount; ++i)
    {
        PrimitiveVertex& v = verts[i];
        const float nx = v.normal[0], ny = v.normal[1], nz = v.normal[2];
        float       tx = tan1x[i],    ty = tan1y[i],    tz = tan1z[i];

        // Gram-Schmidt: tangent를 normal에 수직으로 투영
        const float dotNT = nx * tx + ny * ty + nz * tz;
        float ox = tx - nx * dotNT;
        float oy = ty - ny * dotNT;
        float oz = tz - nz * dotNT;
        const float len = std::sqrtf(ox * ox + oy * oy + oz * oz);
        if (len > 1e-8f)
        {
            ox /= len; oy /= len; oz /= len;
        }
        else
        {
            // UV 델타가 축퇴된 경우(면적 0 등) — normal에 수직인 임의 축으로 대체
            ox = 1.0f; oy = 0.0f; oz = 0.0f;
        }

        // handedness: cross(N, T) 와 누적 bitangent(tan2)의 내적 부호
        const float cx = ny * tz - nz * ty;
        const float cy = nz * tx - nx * tz;
        const float cz = nx * ty - ny * tx;
        const float w  = (cx * tan2x[i] + cy * tan2y[i] + cz * tan2z[i]) < 0.0f ? -1.0f : 1.0f;

        v.tangent[0] = ox; v.tangent[1] = oy; v.tangent[2] = oz; v.tangent[3] = w;
    }
}
