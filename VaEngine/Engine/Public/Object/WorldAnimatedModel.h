#pragma once

#include "Object/WorldObject.h"
#include "Object/Skeleton.h"
#include "Animation/AnimController.h"
#include "Animation/AnimClip.h"
#include "Mesh/SkinnedMesh.h"
#include "RHI/IBuffer.h"
#include "RHI/Texture/ITexture.h"
#include "RHI/Texture/ITexture2DArray.h"

#include <memory>
#include <vector>
#include <string>

class IRenderDevice;
class RenderScene;

class WorldAnimatedModel : public WorldObject
{
public:
    void Initialize(IRenderDevice* device,
                    const std::string& skmPath,
                    const std::vector<std::string>& clipPaths,
                    const std::string& texturePath = {});

    void Update(float deltaTime);
    void AddToScene(RenderScene& scene) const;

    void Play     (uint32_t clipIndex, uint32_t instanceIndex = 0);
    void PlayTween(uint32_t nextClip, float blendTime, uint32_t instanceIndex = 0, float speed = 1.0f);
    void Resize   (uint32_t instanceCount);

    uint32_t GetClipCount() const { return clipCount; }


private:
    void BakeTransformsMap(IRenderDevice* device);

    std::unique_ptr<Skeleton>                   skeleton;
    std::vector<std::unique_ptr<SkinnedMesh>>   meshes;
    std::unique_ptr<ITexture>                   texture;
    std::unique_ptr<ITexture2DArray>            transformsMap;
    std::unique_ptr<IBuffer>                    tweenBuffer;
    std::vector<AnimClipData>                   clips;
    AnimController                              animController;

	uint32_t clipCount = 0;
    static constexpr uint32_t MAX_TWEEN_INSTANCES = 500;
};
