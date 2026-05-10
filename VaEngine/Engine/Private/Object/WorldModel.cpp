#include "Object/WorldModel.h"

#include "Asset/MeshLoader.h"
#include "RHI/IRenderDevice.h"
#include "RHI/Texture/ITexture.h"
#include "Scene/RenderScene.h"

#include <filesystem>
#include <fstream>
#include <string>

static std::string ParseDiffuseTex(const std::string& matlPath)
{
    std::ifstream f(matlPath);
    std::string line;
    while (std::getline(f, line))
    {
        const std::string prefix = "diffuse_tex=";
        if (line.rfind(prefix, 0) == 0)
            return line.substr(prefix.size());
    }
    return {};
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

    if (matlPath.empty())
        return;

    const std::string texName = ParseDiffuseTex(matlPath);
    if (texName.empty())
        return;

    const std::string spath =
        (std::filesystem::path(matlPath).parent_path() / texName).string();

    texture = device->CreateTexture();
    texture->LoadFromFile(device, spath.c_str());
}

void WorldModel::Impl_AddToScene(RenderScene& scene) const
{
    for (const auto& mesh : meshes)
        scene.AddMesh(mesh.get(), transform.GetMatrix(), texture.get());
}
