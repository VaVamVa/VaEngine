#include "Asset/MeshLoader.h"
#include "Asset/MeshAsset.h"

#include <fstream>
#include <iostream>

static uint32_t ReadU32(std::ifstream& f)
{
    uint32_t v = 0;
    f.read(reinterpret_cast<char*>(&v), sizeof(v));
    return v;
}

static uint16_t ReadU16(std::ifstream& f)
{
    uint16_t v = 0;
    f.read(reinterpret_cast<char*>(&v), sizeof(v));
    return v;
}

static std::string ReadStr(std::ifstream& f)
{
    const uint32_t len = ReadU32(f);
    std::string s(len, '\0');
    f.read(s.data(), static_cast<std::streamsize>(len));
    return s;
}

std::vector<MeshData> MeshLoader::Load(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open())
    {
        std::cerr << "[MeshLoader] Cannot open: " << path << "\n";
        return {};
    }

    MeshFileHeader header{};
    f.read(reinterpret_cast<char*>(&header), sizeof(header));

    if (header.magic != MESH_MAGIC)
    {
        std::cerr << "[MeshLoader] Invalid magic in: " << path << "\n";
        return {};
    }
    if (header.version != MESH_VERSION)
    {
        std::cerr << "[MeshLoader] Version mismatch in: " << path
                  << " (file=" << header.version
                  << " expected=" << MESH_VERSION << ")\n";
    }

    std::vector<MeshData> result;
    result.reserve(header.meshCount);

    for (uint32_t m = 0; m < header.meshCount; ++m)
    {
        ReadStr(f);  // name (디버그용, 현재 사용 안 함)
        ReadStr(f);  // materialName (Phase 2용)

        const uint32_t vertexCount  = ReadU32(f);
        const uint32_t vertexStride = ReadU32(f);

        MeshData data;
        data.vertexStride = vertexStride;
        data.vertices.resize(static_cast<size_t>(vertexCount) * vertexStride);
        f.read(reinterpret_cast<char*>(data.vertices.data()),
               static_cast<std::streamsize>(data.vertices.size()));

        const uint32_t indexCount = ReadU32(f);
        data.indices.resize(indexCount);
        for (uint32_t i = 0; i < indexCount; ++i)
            data.indices[i] = ReadU16(f);

        // 4-byte 정렬 패딩 skip
        if (indexCount % 2 != 0)
            ReadU16(f);

        result.push_back(std::move(data));
    }

    return result;
}
