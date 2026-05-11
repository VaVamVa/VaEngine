#include "Asset/SkmLoader.h"
#include "Asset/SkmAsset.h"

#include <fstream>
#include <iostream>
#include <cstring>

static uint32_t ReadU32(std::ifstream& f)
{
    uint32_t v = 0; f.read(reinterpret_cast<char*>(&v), 4); return v;
}
static int32_t ReadI32(std::ifstream& f)
{
    int32_t v = 0; f.read(reinterpret_cast<char*>(&v), 4); return v;
}
static uint16_t ReadU16(std::ifstream& f)
{
    uint16_t v = 0; f.read(reinterpret_cast<char*>(&v), 2); return v;
}
static std::string ReadStr(std::ifstream& f)
{
    const uint32_t len = ReadU32(f);
    std::string s(len, '\0');
    f.read(s.data(), static_cast<std::streamsize>(len));
    return s;
}
static void ReadMatrix(std::ifstream& f, Matrix4x4& m)
{
    f.read(reinterpret_cast<char*>(m.m), 64);
}

SkmLoadResult SkmLoader::Load(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open())
    {
        std::cerr << "[SkmLoader] Cannot open: " << path << "\n";
        return {};
    }

    SkmFileHeader header{};
    f.read(reinterpret_cast<char*>(&header), sizeof(header));

    if (header.magic != SKM_MAGIC)
    {
        std::cerr << "[SkmLoader] Invalid magic: " << path << "\n";
        return {};
    }

    SkmLoadResult result;

    // ── 본 읽기 ──────────────────────────────────────────────────────────────
    for (uint32_t b = 0; b < header.boneCount; ++b)
    {
        BoneData bone;
        bone.name        = ReadStr(f);
        bone.parentIndex = ReadI32(f);
        ReadMatrix(f, bone.bindPose);
        ReadMatrix(f, bone.offsetMatrix);
        result.skeleton.AddBone(std::move(bone));
    }

    // ── 서브메시 읽기 ─────────────────────────────────────────────────────────
    for (uint32_t m = 0; m < header.meshCount; ++m)
    {
        ReadStr(f);  // name (디버그용)
        result.materialNames.push_back(ReadStr(f));

        const uint32_t vertexCount  = ReadU32(f);
        const uint32_t vertexStride = ReadU32(f);

        SkinnedMeshData data;
        data.vertexStride = vertexStride;
        data.vertices.resize(static_cast<size_t>(vertexCount) * vertexStride);
        f.read(reinterpret_cast<char*>(data.vertices.data()),
               static_cast<std::streamsize>(data.vertices.size()));

        const uint32_t indexCount = ReadU32(f);
        data.indices.resize(indexCount);
        for (uint32_t i = 0; i < indexCount; ++i)
            data.indices[i] = ReadU16(f);
        if (indexCount % 2 != 0) ReadU16(f);  // 패딩

        result.meshes.push_back(std::move(data));
    }

    return result;
}
