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
    // Layer 1로 설정하여 다른 오브젝트보다 나중에 그려지게 함 (자동화 시스템 활용)
    scene.AddMesh(mesh.get(), transform.GetMatrix(), 0, 0, 1);
}
