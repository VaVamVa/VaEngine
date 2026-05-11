#include "Asset/ClipLoader.h"
#include "Asset/ClipAsset.h"

#include <fstream>
#include <iostream>

static uint32_t ReadU32(std::ifstream& f)
{
    uint32_t v = 0; f.read(reinterpret_cast<char*>(&v), 4); return v;
}
static float ReadF32(std::ifstream& f)
{
    float v = 0; f.read(reinterpret_cast<char*>(&v), 4); return v;
}
static std::string ReadStr(std::ifstream& f)
{
    const uint32_t len = ReadU32(f);
    std::string s(len, '\0');
    f.read(s.data(), static_cast<std::streamsize>(len));
    return s;
}

AnimClipData ClipLoader::Load(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open())
    {
        std::cerr << "[ClipLoader] Cannot open: " << path << "\n";
        return {};
    }

    ClipFileHeader header{};
    f.read(reinterpret_cast<char*>(&header), sizeof(header));

    if (header.magic != CLIP_MAGIC)
    {
        std::cerr << "[ClipLoader] Invalid magic: " << path << "\n";
        return {};
    }

    AnimClipData clip;
    clip.name       = ReadStr(f);
    clip.duration   = header.duration;
    clip.frameRate  = header.frameRate;
    clip.frameCount = header.frameCount;
    clip.boneCount  = header.boneCount;

    clip.boneNames.resize(header.boneCount);
    for (uint32_t b = 0; b < header.boneCount; ++b)
        clip.boneNames[b] = ReadStr(f);

    // [boneIndex][frameIndex] 순서로 저장됨
    clip.keyframes.resize(header.boneCount,
                          std::vector<KeyframeEntry>(header.frameCount));

    for (uint32_t b = 0; b < header.boneCount; ++b)
    {
        for (uint32_t fr = 0; fr < header.frameCount; ++fr)
        {
            KeyframeEntry& e = clip.keyframes[b][fr];
            f.read(reinterpret_cast<char*>(&e.scale),       12);
            f.read(reinterpret_cast<char*>(&e.rotation),    16);
            f.read(reinterpret_cast<char*>(&e.translation), 12);
        }
    }

    return clip;
}
