#include "Mesh/CubeShape.h"

#include <cstring>

// 각 면을 독립적인 4개 정점으로 구성 (총 24 vertices, 36 indices)
// 면 별 UV 독립, 텍스처 매핑 정확도 보장
// DX12 CW 와인딩 (FrontCounterClockwise=FALSE, CullMode=Back)
MeshData CubeShape::Build() const
{
    const float x = halfX, y = halfY, z = halfZ;

    // 각 면: v0(0,1) v1(0,0) v2(1,0) v3(1,1) — screen 기준 RB→RT→LT→LB
    // { pos[3], normal[3], color[4], uv[2] }
    PrimitiveVertex verts[24] = {
        // -Z face (front),  normal = (0, 0, -1)
        { {-x,-y,-z}, {0,0,-1}, {1,1,1,1}, {0,1} },
        { {-x,+y,-z}, {0,0,-1}, {1,1,1,1}, {0,0} },
        { {+x,+y,-z}, {0,0,-1}, {1,1,1,1}, {1,0} },
        { {+x,-y,-z}, {0,0,-1}, {1,1,1,1}, {1,1} },
        // +Z face (back),   normal = (0, 0, +1)
        { {+x,-y,+z}, {0,0,+1}, {1,1,1,1}, {0,1} },
        { {+x,+y,+z}, {0,0,+1}, {1,1,1,1}, {0,0} },
        { {-x,+y,+z}, {0,0,+1}, {1,1,1,1}, {1,0} },
        { {-x,-y,+z}, {0,0,+1}, {1,1,1,1}, {1,1} },
        // +X face (right),  normal = (+1, 0, 0)
        { {+x,-y,-z}, {+1,0,0}, {1,1,1,1}, {0,1} },
        { {+x,+y,-z}, {+1,0,0}, {1,1,1,1}, {0,0} },
        { {+x,+y,+z}, {+1,0,0}, {1,1,1,1}, {1,0} },
        { {+x,-y,+z}, {+1,0,0}, {1,1,1,1}, {1,1} },
        // -X face (left),   normal = (-1, 0, 0)
        { {-x,-y,+z}, {-1,0,0}, {1,1,1,1}, {0,1} },
        { {-x,+y,+z}, {-1,0,0}, {1,1,1,1}, {0,0} },
        { {-x,+y,-z}, {-1,0,0}, {1,1,1,1}, {1,0} },
        { {-x,-y,-z}, {-1,0,0}, {1,1,1,1}, {1,1} },
        // +Y face (top),    normal = (0, +1, 0)
        { {+x,+y,+z}, {0,+1,0}, {1,1,1,1}, {0,1} },
        { {+x,+y,-z}, {0,+1,0}, {1,1,1,1}, {0,0} },
        { {-x,+y,-z}, {0,+1,0}, {1,1,1,1}, {1,0} },
        { {-x,+y,+z}, {0,+1,0}, {1,1,1,1}, {1,1} },
        // -Y face (bottom), normal = (0, -1, 0)
        { {+x,-y,-z}, {0,-1,0}, {1,1,1,1}, {0,1} },
        { {+x,-y,+z}, {0,-1,0}, {1,1,1,1}, {0,0} },
        { {-x,-y,+z}, {0,-1,0}, {1,1,1,1}, {1,0} },
        { {-x,-y,-z}, {0,-1,0}, {1,1,1,1}, {1,1} },
    };

    uint16_t indices[36];
    for (uint16_t f = 0; f < 6; ++f)
    {
        const uint16_t b = f * 4;
        indices[f * 6 + 0] = b + 0;
        indices[f * 6 + 1] = b + 1;
        indices[f * 6 + 2] = b + 2;
        indices[f * 6 + 3] = b + 0;
        indices[f * 6 + 4] = b + 2;
        indices[f * 6 + 5] = b + 3;
    }

    MeshData data;
    data.vertexStride = sizeof(PrimitiveVertex);
    data.vertices.resize(sizeof(verts));
    std::memcpy(data.vertices.data(), verts, sizeof(verts));
    data.indices.assign(std::begin(indices), std::end(indices));
    return data;
}
