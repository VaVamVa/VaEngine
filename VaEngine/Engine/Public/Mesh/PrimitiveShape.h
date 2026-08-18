#pragma once

#include "MeshData.h"

#include <cstddef>
#include <cstdint>

class PrimitiveShape
{
public:
    virtual ~PrimitiveShape() = default;
    virtual MeshData Build() const = 0;

protected:
    // 삼각형의 pos/uv 델타로부터 per-vertex tangent(xyz)+handedness(w)를 계산해 verts에 기록.
    // normal은 이미 verts에 채워져 있어야 한다 (Gram-Schmidt 직교화에 사용).
    static void ComputeTangents(PrimitiveVertex* verts, size_t vertexCount,
                                 const uint16_t* indices, size_t indexCount);
};
