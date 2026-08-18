#include "WorldObjects/WO_Cube.h"

#include "RHI/IRenderDevice.h"
#include "Mesh/CubeShape.h"

void WO_Cube::Initialize(IRenderDevice* device)
{
    auto m = std::make_unique<MeshPrimitive>();
    m->Initialize(device, CubeShape{}.Build());
    meshes.push_back(std::move(m));
    EnsureMaterial();
    material->Initialize(device);
    material->SetBlendMode(EBlendMode::AlphaBlend);
    material->SetAlbedo(1.0f, 1.0f, 1.0f, 0.5f);
}
