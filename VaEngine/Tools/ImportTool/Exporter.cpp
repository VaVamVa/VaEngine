#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "Exporter.h"
#include "Asset/MeshAsset.h"

#include <filesystem>
#include <fstream>
#include <iostream>

// ── 직렬화 헬퍼 ─────────────────────────────────────────────────────────────

static void WriteU32(std::ofstream& f, uint32_t v)
{
    f.write(reinterpret_cast<const char*>(&v), sizeof(v));
}

static void WriteU16(std::ofstream& f, uint16_t v)
{
    f.write(reinterpret_cast<const char*>(&v), sizeof(v));
}

static void WriteStr(std::ofstream& f, const std::string& s)
{
    WriteU32(f, static_cast<uint32_t>(s.size()));
    f.write(s.data(), static_cast<std::streamsize>(s.size()));
}

// ── 텍스처 저장 ─────────────────────────────────────────────────────────────

static void SaveTextureSlot(
    const TextureSlot& slot,
    const std::string& inputDir,
    const std::string& outputDir)
{
    if (slot.IsEmpty())
        return;

    const std::string dest = outputDir + "/" + slot.filename;

    if (slot.IsEmbedded())
    {
        if (slot.IsRGBA())
        {
            // 비압축 RGBA → stb_image_write로 PNG 저장
            stbi_write_png(
                dest.c_str(),
                static_cast<int>(slot.width),
                static_cast<int>(slot.height),
                4,
                slot.bytes.data(),
                static_cast<int>(slot.width) * 4);
            std::cout << "[ImportTool] Saved embedded (RGBA): " << slot.filename << "\n";
        }
        else
        {
            // 압축 embedded — 바이트 그대로 파일에 기록
            std::ofstream f(dest, std::ios::binary | std::ios::trunc);
            f.write(reinterpret_cast<const char*>(slot.bytes.data()),
                    static_cast<std::streamsize>(slot.bytes.size()));
            std::cout << "[ImportTool] Saved embedded: " << slot.filename << "\n";
        }
    }
    else
    {
        // 외부 파일 복사
        const auto src = std::filesystem::path(inputDir) / slot.filename;
        if (!std::filesystem::exists(src))
        {
            std::cerr << "[ImportTool] Texture not found: " << src.generic_string() << "\n";
            return;
        }
        std::filesystem::copy_file(src, dest,
            std::filesystem::copy_options::overwrite_existing);
        std::cout << "[ImportTool] Copied texture: " << slot.filename << "\n";
    }
}

// ── ExportMesh ───────────────────────────────────────────────────────────────

bool Exporter::ExportMesh(
    const std::vector<SubMeshIntermediate>& meshes,
    const std::string& outputDir,
    const std::string& stem)
{
    const std::string outPath = outputDir + "/" + stem + ".mesh";

    std::ofstream f(outPath, std::ios::binary | std::ios::trunc);
    if (!f.is_open())
    {
        std::cerr << "[ImportTool] Cannot open for write: " << outPath << "\n";
        return false;
    }

    MeshFileHeader header{};
    header.magic     = MESH_MAGIC;
    header.version   = MESH_VERSION;
    header.meshCount = static_cast<uint32_t>(meshes.size());
    f.write(reinterpret_cast<const char*>(&header), sizeof(header));

    constexpr uint32_t vertexStride = 48;

    for (const auto& sub : meshes)
    {
        WriteStr(f, sub.name);
        WriteStr(f, sub.materialName);

        const uint32_t vertexCount = static_cast<uint32_t>(
            sub.vertices.size() / vertexStride);
        WriteU32(f, vertexCount);
        WriteU32(f, vertexStride);
        f.write(reinterpret_cast<const char*>(sub.vertices.data()),
                static_cast<std::streamsize>(sub.vertices.size()));

        const uint32_t indexCount = static_cast<uint32_t>(sub.indices.size());
        WriteU32(f, indexCount);
        for (uint16_t idx : sub.indices)
            WriteU16(f, idx);

        if (indexCount % 2 != 0)
        {
            uint16_t pad = 0;
            WriteU16(f, pad);
        }
    }

    std::cout << "[ImportTool] Written: " << outPath
              << " (" << meshes.size() << " sub-mesh(es))\n";
    return true;
}

// ── ExportMaterial ───────────────────────────────────────────────────────────

void Exporter::ExportMaterial(
    const std::vector<MaterialIntermediate>& materials,
    const std::string& inputDir,
    const std::string& outputDir,
    const std::string& stem)
{
    // 텍스처 저장
    for (const auto& mat : materials)
    {
        SaveTextureSlot(mat.diffuseTex,  inputDir, outputDir);
        SaveTextureSlot(mat.normalTex,   inputDir, outputDir);
        SaveTextureSlot(mat.specularTex, inputDir, outputDir);
    }

    // .matl 텍스트 파일
    const std::string matlPath = outputDir + "/" + stem + ".matl";
    std::ofstream f(matlPath);
    if (!f.is_open())
    {
        std::cerr << "[ImportTool] Cannot write: " << matlPath << "\n";
        return;
    }

    for (const auto& mat : materials)
    {
        f << "[" << mat.name << "]\n";
        f << "ambient="   << mat.ambient[0]  << "," << mat.ambient[1]
                          << "," << mat.ambient[2]  << "," << mat.ambient[3]  << "\n";
        f << "diffuse="   << mat.diffuse[0]  << "," << mat.diffuse[1]
                          << "," << mat.diffuse[2]  << "," << mat.diffuse[3]  << "\n";
        f << "specular="  << mat.specular[0] << "," << mat.specular[1]
                          << "," << mat.specular[2]                           << "\n";
        f << "shininess=" << mat.specular[3] << "\n";
        f << "emissive="  << mat.emissive[0] << "," << mat.emissive[1]
                          << "," << mat.emissive[2] << "," << mat.emissive[3] << "\n";

        if (!mat.diffuseTex.IsEmpty())
            f << "diffuse_tex="  << mat.diffuseTex.filename  << "\n";
        if (!mat.normalTex.IsEmpty())
            f << "normal_tex="   << mat.normalTex.filename   << "\n";
        if (!mat.specularTex.IsEmpty())
            f << "specular_tex=" << mat.specularTex.filename << "\n";

        f << "\n";
    }

    std::cout << "[ImportTool] Written: " << matlPath << "\n";
}
