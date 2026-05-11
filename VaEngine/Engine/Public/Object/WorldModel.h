#pragma once

#include "Object/WorldObject.h"
#include "Mesh/MeshPrimitive.h"

#include <memory>
#include <string>
#include <vector>

class IRenderDevice;
class ITexture;
class RenderScene;

// 하나 이상의 서브메시 + diffuse 텍스처를 소유하는 WorldObject.
// meshPath  : .mesh 바이너리 파일 경로
// matlPath  : .matl 텍스트 파일 경로 (빈 문자열이면 텍스처 로드 생략)
class WorldModel : public WorldObject
{
public:
    void Initialize(IRenderDevice* device,
                    const std::string& meshPath,
                    const std::string& matlPath = {});

    const std::vector<std::unique_ptr<MeshPrimitive>>& GetMeshes()  const { return meshes;  }
    ITexture*                                          GetTexture() const { return texture.get(); }

protected:
    void Impl_AddToScene(RenderScene& scene) const override;

    std::vector<std::unique_ptr<MeshPrimitive>> meshes;
    std::unique_ptr<ITexture>                   texture;
};
