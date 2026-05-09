#pragma once

#include <cstdint>

constexpr uint32_t SKM_MAGIC   = 0x204D4B53; // 'SKM '
constexpr uint32_t SKM_VERSION = 1;

struct SkmFileHeader
{
    uint32_t magic;
    uint32_t version;
    uint32_t boneCount;
    uint32_t meshCount;
    uint32_t reserved;
};
