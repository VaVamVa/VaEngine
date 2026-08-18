#pragma once

#include <cstdint>

constexpr uint32_t MESH_MAGIC   = 0x4853454D;  // 'MESH' little-endian
constexpr uint32_t MESH_VERSION = 2;  // v2: PrimitiveVertex에 tangent[4] 추가 (stride 48→64), 하위 호환 없음

struct MeshFileHeader
{
    uint32_t magic;
    uint32_t version;
    uint32_t meshCount;
    uint32_t reserved = 0;
};
