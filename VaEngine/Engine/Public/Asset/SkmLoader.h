#pragma once

#include "Object/Skeleton.h"
#include "Mesh/SkinnedMesh.h"

#include <string>
#include <vector>

struct SkmLoadResult
{
    Skeleton                        skeleton;
    std::vector<SkinnedMeshData>    meshes;
    std::vector<std::string>        materialNames;
};

namespace SkmLoader
{
    // .smesh 파일 → SkmLoadResult (실패 시 빈 meshes)
    SkmLoadResult Load(const std::string& path);
}
