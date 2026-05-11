#include "Object/WorldAnimatedModel.h"

#include "Object/Skeleton.h"
#include "Animation/AnimClip.h"
#include "Asset/SkmLoader.h"
#include "Asset/ClipLoader.h"
#include "Scene/RenderScene.h"

#include "RHI/IRenderDevice.h"
#include "RHI/IBuffer.h"
#include "RHI/Common_RHI.h"
#include "RHI/Texture/ITexture.h"
#include "RHI/Texture/ITexture2DArray.h"
#include "Math/Container.h"

#include <algorithm>
#include <filesystem>
#include <unordered_map>
#include <vector>
#include <string>

void WorldAnimatedModel::Initialize(IRenderDevice* device,
                                     const std::string& skmPath,
                                     const std::vector<std::string>& clipPaths,
                                     const std::string& texturePath)
{
    // 스켈레톤 + 스키닝 메시 로드
    SkmLoadResult result = SkmLoader::Load(skmPath);
    skeleton = std::make_unique<Skeleton>(std::move(result.skeleton));

    constexpr uint32_t kDefaultMaxInstances = 64;
    for (auto& meshData : result.meshes)
    {
        auto mesh = std::make_unique<SkinnedMesh>();
        mesh->Initialize(device, meshData);
        // 본 팔레트 — 같은 mesh를 공유하는 모든 인스턴스가 이 buffer에 영역 분할 사용
        mesh->CreateBonePalette(device, kDefaultMaxInstances);
        meshes.push_back(std::move(mesh));
    }

    // 애니메이션 클립 로드
    for (const auto& path : clipPaths)
    {
        AnimClipData clip = ClipLoader::Load(path);
        clip.name = std::filesystem::path(path).stem().string();
        if (clip.frameCount > 0)
            clips.push_back(std::move(clip));
        ++clipCount;
    }

    // Diffuse 텍스처 로드
    if (!texturePath.empty())
    {
        texture = device->CreateTexture();
        texture->LoadFromFile(device, texturePath.c_str());
    }

    // 본 변환 행렬을 Texture2DArray에 굽기
    if (!clips.empty() && skeleton->BoneCount() > 0)
        BakeTransformsMap(device);

    // TweenFrame 상수 버퍼 (매 프레임 Upload)
    tweenBuffer = device->CreateBuffer({
        .size   = MAX_TWEEN_INSTANCES * sizeof(TweenFrameDesc),
        .usage  = EBufferUsage::ConstantBuffer,
        .access = EMemoryAccess::Upload,
        .stride = 0
    });

    animController.Resize(1);
    if (!clips.empty())
        animController.Play(0, 0);
}

void WorldAnimatedModel::BakeTransformsMap(IRenderDevice* device)
{
    const auto&    bones     = skeleton->GetBones();
    const uint32_t boneCount = skeleton->BoneCount();
    const uint32_t clipCount = static_cast<uint32_t>(clips.size());

    uint32_t maxFrames = 0;
    for (const auto& clip : clips)
        maxFrames = std::max(maxFrames, clip.frameCount);
    if (maxFrames == 0)
        return;

    const uint32_t texWidth  = boneCount * 4;  // 행렬 1개 = float4 행 × 4
    const uint32_t texHeight = maxFrames;

    // 메모리 레이아웃: [clip][frame][boneIdx*4 + row] × float4
    std::vector<float> texData(
        static_cast<size_t>(texWidth) * texHeight * clipCount * 4, 0.0f);

    for (uint32_t c = 0; c < clipCount; ++c)
    {
        const AnimClipData& clip = clips[c];

        std::unordered_map<std::string, uint32_t> clipBoneIdx;
        clipBoneIdx.reserve(clip.boneCount);
        for (uint32_t i = 0; i < clip.boneCount; ++i)
            clipBoneIdx[clip.boneNames[i]] = i;

        for (uint32_t f = 0; f < maxFrames; ++f)
        {
            // 프레임 수가 짧은 클립은 마지막 프레임으로 패딩
            const uint32_t frameIdx = (f < clip.frameCount) ? f : clip.frameCount - 1;

            // 글로벌 변환 계산 (부모 인덱스 < 자식 인덱스 가정)
            std::vector<Matrix4x4> globals(boneCount);

            for (uint32_t b = 0; b < boneCount; ++b)
            {
                Matrix4x4 local;
                auto it = clipBoneIdx.find(bones[b].name);
                if (it != clipBoneIdx.end())
                {
                    const KeyframeEntry& key = clip.keyframes[it->second][frameIdx];
                    local = Matrix4x4::Scale(key.scale)
                          * Matrix4x4::RotationQuat(key.rotation)
                          * Matrix4x4::Translation(key.translation);
                }
                else
                {
                    local = Matrix4x4::Identity();
                }

                const int parent = bones[b].parentIndex;
                globals[b] = (parent < 0)
                    ? local
                    : (local * globals[static_cast<uint32_t>(parent)]);
            }

            // finalSkinningMatrix = offsetMatrix * globalTransform → 텍스처 패킹
            for (uint32_t b = 0; b < boneCount; ++b)
            {
                Matrix4x4 finalMat = bones[b].offsetMatrix * globals[b];

                const size_t baseTexel = (static_cast<size_t>(c) * texHeight + f)
                                        * texWidth + b * 4;
                for (uint32_t row = 0; row < 4; ++row)
                {
                    const size_t idx = (baseTexel + row) * 4;
                    texData[idx + 0] = finalMat.m[row][0];
                    texData[idx + 1] = finalMat.m[row][1];
                    texData[idx + 2] = finalMat.m[row][2];
                    texData[idx + 3] = finalMat.m[row][3];
                }
            }
        }
    }

    transformsMap = device->CreateTexture2DArray();
    transformsMap->Upload(device, texData.data(), texWidth, texHeight, clipCount);
}

void WorldAnimatedModel::Update(float deltaTime)
{
    animController.Update(deltaTime, clips);

    const auto& frames = animController.GetTweenFrames();
    if (!frames.empty())
        tweenBuffer->Upload(frames.data(), frames.size() * sizeof(TweenFrameDesc));
}

void WorldAnimatedModel::Impl_AddToScene(RenderScene& scene) const
{
    const Matrix4x4 world = transform.GetMatrix();
    const uint32_t  count = animController.InstanceCount();

    for (const auto& mesh : meshes)
    {
        scene.AddSkinnedMesh(
            mesh.get(),
            world,
            texture.get(),
            transformsMap.get(),
            tweenBuffer.get(),
            count
        );
    }
}

void WorldAnimatedModel::Play(uint32_t clipIndex, uint32_t instanceIndex)
{
    animController.Play(instanceIndex, clipIndex);
}

void WorldAnimatedModel::PlayTween(uint32_t nextClip, float blendTime, uint32_t instanceIndex, float speed)
{
    animController.PlayTween(instanceIndex, nextClip, blendTime, speed);
}

void WorldAnimatedModel::PlayBlend(uint32_t clip0, uint32_t clip1, uint32_t clip2,
                                    float speed0, float speed1, float speed2,
                                    uint32_t instanceIndex)
{
    animController.PlayBlend(instanceIndex, clip0, clip1, clip2, speed0, speed1, speed2);
}

void WorldAnimatedModel::SetBlendAlpha(float alpha, uint32_t instanceIndex)
{
    animController.SetBlendAlpha(instanceIndex, alpha);
}

void WorldAnimatedModel::Resize(uint32_t instanceCount)
{
    animController.Resize(instanceCount);
}
