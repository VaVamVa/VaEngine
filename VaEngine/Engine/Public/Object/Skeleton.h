#pragma once

#include "Math/Container.h"

#include <string>
#include <vector>

struct BoneData
{
    std::string name;
    int         parentIndex;  // -1 = 루트
    Matrix4x4   bindPose;     // 본 공간 → 모델 공간
    Matrix4x4   offsetMatrix; // 모델 공간 → 본 공간 (Inverse Bind Pose)
};

class Skeleton
{
public:
    void AddBone(BoneData bone) { bones.push_back(std::move(bone)); }

    const std::vector<BoneData>& GetBones() const { return bones; }
    uint32_t                     BoneCount() const { return static_cast<uint32_t>(bones.size()); }

    // 이름으로 본 인덱스 검색 — 없으면 -1
    int FindBone(const std::string& name) const;

private:
    std::vector<BoneData> bones;
};
