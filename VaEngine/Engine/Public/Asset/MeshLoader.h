#pragma once

#include "Mesh/MeshData.h"

#include <string>
#include <vector>

namespace MeshLoader
{
    // .mesh 파일 → MeshData 목록 (서브메시 개수만큼)
    // 실패 시 빈 vector 반환
    std::vector<MeshData> Load(const std::string& path);
}
