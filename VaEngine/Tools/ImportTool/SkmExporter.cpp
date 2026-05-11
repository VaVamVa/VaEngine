#include "SkmExporter.h"
#include "Asset/SkmAsset.h"

#include <fstream>
#include <iostream>

static void WriteU32S(std::ofstream& f, uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); }
static void WriteI32S(std::ofstream& f, int32_t  v) { f.write(reinterpret_cast<const char*>(&v), 4); }
static void WriteU16S(std::ofstream& f, uint16_t v) { f.write(reinterpret_cast<const char*>(&v), 2); }

static void WriteStrS(std::ofstream& f, const std::string& s)
{
    WriteU32S(f, static_cast<uint32_t>(s.size()));
    f.write(s.data(), static_cast<std::streamsize>(s.size()));
}

bool SkmExporter::ExportSkm(const SkinnedConvertResult& result,
                             const std::string& outputDir,
                             const std::string& stem)
{
    const std::string path = outputDir + "/" + stem + ".smesh";
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f.is_open())
    {
        std::cerr << "[ImportTool] Cannot open for write: " << path << "\n";
        return false;
    }

    SkmFileHeader header{};
    header.magic     = SKM_MAGIC;
    header.version   = SKM_VERSION;
    header.boneCount = static_cast<uint32_t>(result.bones.size());
    header.meshCount = static_cast<uint32_t>(result.meshes.size());
    f.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // Bones: name, parentIndex, bindPose(64), offsetMatrix(64)
    for (const auto& bone : result.bones)
    {
        WriteStrS(f, bone.name);
        WriteI32S(f, bone.parentIndex);
        f.write(reinterpret_cast<const char*>(bone.bindPose),     64);
        f.write(reinterpret_cast<const char*>(bone.offsetMatrix), 64);
    }

    // Skinned meshes: name, materialName, vertexCount, stride, vertices, indexCount, indices [, pad]
    for (const auto& mesh : result.meshes)
    {
        WriteStrS(f, mesh.name);
        WriteStrS(f, mesh.materialName);

        constexpr uint32_t kStride = 80;
        const uint32_t vertexCount = static_cast<uint32_t>(mesh.vertices.size()) / kStride;
        WriteU32S(f, vertexCount);
        WriteU32S(f, kStride);
        f.write(reinterpret_cast<const char*>(mesh.vertices.data()),
                static_cast<std::streamsize>(mesh.vertices.size()));

        const uint32_t indexCount = static_cast<uint32_t>(mesh.indices.size());
        WriteU32S(f, indexCount);
        for (uint16_t idx : mesh.indices)
            WriteU16S(f, idx);
        if (indexCount % 2 != 0)
            WriteU16S(f, 0);
    }

    std::cout << "[ImportTool] Written: " << path
              << " (" << result.bones.size() << " bone(s), "
              << result.meshes.size() << " mesh(es))\n";
    return true;
}
