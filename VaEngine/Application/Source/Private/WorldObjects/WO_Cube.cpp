#include "WorldObjects/WO_Cube.h"

#include "RHI/IRenderDevice.h"
#include "Mesh/CubeShape.h"

void WO_Cube::Initialize(IRenderDevice* device)
{
    mesh = std::make_unique<MeshPrimitive>();
    mesh->Initialize(device, CubeShape{}.Build());
}
