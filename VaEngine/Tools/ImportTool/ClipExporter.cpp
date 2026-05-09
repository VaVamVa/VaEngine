#include "ClipExporter.h"
#include "Asset/ClipAsset.h"

#include <fstream>
#include <iostream>

static void WriteU32C(std::ofstream& f, uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); }

static void WriteStrC(std::ofstream& f, const std::string& s)
{
    WriteU32C(f, static_cast<uint32_t>(s.size()));
    f.write(s.data(), static_cast<std::streamsize>(s.size()));
}

void ClipExporter::ExportClips(const std::vector<AnimClipIntermediate>& clips,
                                const std::string& outputDir,
                                const std::string& stem)
{
    for (size_t i = 0; i < clips.size(); ++i)
    {
        const AnimClipIntermediate& clip = clips[i];

        // 클립 1개: stem.clip / 복수: stem_ClipName.clip / 이름 없으면 stem_N.clip
        std::string filename;
        if (clips.size() == 1)
        {
            filename = stem + ".clip";
        }
        else
        {
            std::string suffix = clip.name.empty()
                ? ("_" + std::to_string(i))
                : ("_" + clip.name);
            for (char& c : suffix)
                if (c == '|' || c == '/' || c == '\\' || c == ' ')
                    c = '_';
            filename = stem + suffix + ".clip";
        }

        const std::string path = outputDir + "/" + filename;
        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        if (!f.is_open())
        {
            std::cerr << "[ImportTool] Cannot open for write: " << path << "\n";
            continue;
        }

        ClipFileHeader header{};
        header.magic      = CLIP_MAGIC;
        header.version    = CLIP_VERSION;
        header.duration   = clip.duration;
        header.frameRate  = clip.frameRate;
        header.frameCount = clip.frameCount;
        header.boneCount  = clip.boneCount;
        f.write(reinterpret_cast<const char*>(&header), sizeof(header));

        WriteStrC(f, clip.name);

        for (uint32_t b = 0; b < clip.boneCount; ++b)
            WriteStrC(f, clip.boneNames[b]);

        // [boneIndex][frameIndex] SRT — 40 bytes/frame (ClipLoader와 동일 순서)
        for (uint32_t b = 0; b < clip.boneCount; ++b)
        {
            for (uint32_t fr = 0; fr < clip.frameCount; ++fr)
            {
                const KeyframeSRT& kf = clip.keyframes[b][fr];
                f.write(reinterpret_cast<const char*>(kf.scale),       12);
                f.write(reinterpret_cast<const char*>(kf.rotation),    16);
                f.write(reinterpret_cast<const char*>(kf.translation), 12);
            }
        }

        std::cout << "[ImportTool] Written: " << path
                  << " (frames=" << clip.frameCount
                  << ", bones="  << clip.boneCount << ")\n";
    }
}
