#pragma once

#include "Converter.h"
#include <string>
#include <vector>

class Exporter
{
public:
    // 서브메시 목록 → <outputDir>/<stem>.mesh
    bool ExportMesh(
        const std::vector<SubMeshIntermediate>& meshes,
        const std::string& outputDir,
        const std::string& stem);

    // 재질 목록 → <outputDir>/<stem>.matl  +  텍스처 파일 복사
    void ExportMaterial(
        const std::vector<MaterialIntermediate>& materials,
        const std::string& inputDir,   // 텍스처 원본 위치
        const std::string& outputDir,
        const std::string& stem);
};
