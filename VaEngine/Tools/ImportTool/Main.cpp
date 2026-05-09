#include "Converter.h"
#include "Exporter.h"
#include "SkmExporter.h"
#include "ClipExporter.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// 디렉토리 안의 .fbx 파일 목록 수집
static std::vector<fs::path> CollectFbx(const fs::path& dir)
{
    std::vector<fs::path> files;
    if (!fs::is_directory(dir))
        return files;
    for (const auto& entry : fs::directory_iterator(dir))
    {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        // 대소문자 무관 비교
        for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (ext == ".fbx")
            files.push_back(entry.path());
    }
    return files;
}

static void RunSession(std::istream& in, std::ostream& out)
{
    const std::string baseInputDir  = ASSET_INPUT_DIR;
    const std::string baseOutputDir = ASSET_OUTPUT_DIR;

    out << "=== VaImportTool ===\n"
        << "Input  : " << baseInputDir  << "\n"
        << "Output : " << baseOutputDir << "\n\n";

    Converter    converter;
    Exporter     exporter;
    SkmExporter  skmExporter;
    ClipExporter clipExporter;

    while (true)
    {
        out << "Asset name (q to quit): ";
        std::string name;
        if (!std::getline(in, name) || name == "q")
            break;

        name = fs::path(name).stem().string();
        if (name.empty())
            continue;

        const fs::path inputDir  = fs::path(baseInputDir)  / name;
        const fs::path outputDir = fs::path(baseOutputDir) / name;

        if (!fs::is_directory(inputDir))
        {
            out << "[ImportTool] Directory not found: " << inputDir << "\n\n";
            continue;
        }

        const auto fbxFiles = CollectFbx(inputDir);
        if (fbxFiles.empty())
        {
            out << "[ImportTool] No .fbx files in: " << inputDir << "\n\n";
            continue;
        }

        fs::create_directories(outputDir);

        bool skmExported = false;

        for (const auto& fbxPath : fbxFiles)
        {
            const std::string fbxStr = fbxPath.string();
            out << "[ImportTool] Inspecting: " << fbxPath.filename() << "\n";

            FbxTraits traits = converter.Inspect(fbxStr);

            if (traits.hasAnimations)
            {
                // 애니메이션 있음 → 클립 (.clip), 메시 있으면 .smesh도 생성
                auto result = converter.ConvertSkinned(fbxStr);
                if (!result.clips.empty())
                {
                    // 클립 FBX에서 .smesh이 아직 없으면 함께 생성
                    if (!skmExported && !result.meshes.empty())
                    {
                        skmExporter.ExportSkm(result, outputDir.string(), name);
                        exporter.ExportMaterial(result.materials,
                                                inputDir.string(),
                                                outputDir.string(), name);
                        skmExported = true;
                    }

                    // FBX 파일명을 클립 stem으로 사용 (공백 → _)
                    std::string clipStem = fbxPath.stem().string();
                    for (char& c : clipStem)
                        if (c == ' ') c = '_';
                    clipExporter.ExportClips(result.clips, outputDir.string(), clipStem);
                }
            }
            else if (traits.hasBones)
            {
                // 본 있음, 애니메이션 없음 → 스키닝 메시 (.smesh) — 폴더당 1회
                if (!skmExported)
                {
                    auto result = converter.ConvertSkinned(fbxStr);
                    if (!result.meshes.empty())
                    {
                        skmExporter.ExportSkm(result, outputDir.string(), name);
                        exporter.ExportMaterial(result.materials,
                                                inputDir.string(),
                                                outputDir.string(), name);
                        skmExported = true;
                    }
                }
            }
            else
            {
                // 본 없음, 애니메이션 없음 → 정적 메시 (.mesh)
                auto result = converter.Convert(fbxStr);
                if (!result.meshes.empty())
                {
                    std::string stem = fbxPath.stem().string();
                    for (char& c : stem) if (c == ' ') c = '_';
                    exporter.ExportMesh(result.meshes, outputDir.string(), stem);
                    exporter.ExportMaterial(result.materials,
                                            inputDir.string(),
                                            outputDir.string(), stem);
                }
            }
        }

        out << "\n";
    }

    out << "Done.\n";
}

int main()
{
    RunSession(std::cin, std::cout);
    return 0;
}
