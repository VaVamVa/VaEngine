#include "Object/WorldModel.h"

#include "Asset/MeshLoader.h"
#include "Render/Material.h"
#include "RHI/IRenderDevice.h"
#include "RHI/Texture/ITexture.h"
#include "Scene/RenderScene.h"

#include <filesystem>
#include <fstream>
#include <string>

// 최소 구현 — 라인 스캔 방식. 텍스처 슬롯이 더 늘어나면 key-value 파서로 일반화
// (Agent/AgentLog/Plan/Refactoring_At260711.md 항목 3 참조). 텍스처 경로뿐 아니라
// scale= 같은 스칼라 값도 동일하게 raw string으로 반환 — 호출부가 알맞게 해석한다.
static std::string ParseMatlKey(const std::string& matlPath, const std::string& key)
{
    std::ifstream f(matlPath);
    std::string line;
    const std::string prefix = key + "=";
    while (std::getline(f, line))
    {
        if (line.rfind(prefix, 0) == 0)
            return line.substr(prefix.size());
    }
    return {};
}

void WorldModel::EnsureMaterial()
{
    if (!material)
        material = std::make_unique<Material>();
}

void WorldModel::Initialize(IRenderDevice* device,
                             const std::string& meshPath,
                             const std::string& matlPath)
{
    for (const auto& data : MeshLoader::Load(meshPath))
    {
        auto prim = std::make_unique<MeshPrimitive>();
        prim->Initialize(device, data);
        meshes.push_back(std::move(prim));
    }

    EnsureMaterial();
    material->Initialize(device);

    if (matlPath.empty())
        return;

    const std::filesystem::path matlDir = std::filesystem::path(matlPath).parent_path();

    const std::string diffuseTexName = ParseMatlKey(matlPath, "diffuse_tex");
    if (!diffuseTexName.empty())
    {
        const std::string spath = (matlDir / diffuseTexName).string();
        texture = device->CreateTexture();
        texture->LoadFromFile(device, spath.c_str());
        material->SetAlbedoTexture(texture.get());
    }

    const std::string normalTexName = ParseMatlKey(matlPath, "normal_tex");
    if (!normalTexName.empty())
    {
        const std::string npath = (matlDir / normalTexName).string();
        normalTexture = device->CreateTexture();
        normalTexture->LoadFromFile(device, npath.c_str());
        material->SetNormalTexture(normalTexture.get());
    }

    // scale= — 자동 단위 보정(Converter.cpp)이 닿지 않는 예외 케이스를 위한 안전망.
    // 게임플레이 효과는 transform.SetScaleMultiplier()를 쓴다 — 이 값과는 별개로 합성된다.
    const std::string scaleStr = ParseMatlKey(matlPath, "scale");
    if (!scaleStr.empty())
        transform.SetScale(std::stof(scaleStr));
}

void WorldModel::Impl_AddToScene(RenderScene& scene) const
{
    const Matrix4x4 world = GetWorldMatrix();
    for (const auto& mesh : meshes)
        scene.AddMesh(mesh.get(), world, material.get(), renderDesc);
}
