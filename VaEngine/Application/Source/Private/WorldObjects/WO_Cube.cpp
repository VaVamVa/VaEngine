#include "WorldObjects/WO_Cube.h"

#include "RHI/IRenderDevice.h"
#include "Mesh/CubeShape.h"
#include "Scene/RenderScene.h"

void WO_Cube::Initialize(IRenderDevice* device)
{
    mesh = std::make_unique<MeshPrimitive>();
    mesh->Initialize(device, CubeShape{}.Build());
}

void WO_Cube::Impl_AddToScene(RenderScene& scene) const
{
    scene.AddMesh(mesh.get(), transform.GetMatrix());
}
