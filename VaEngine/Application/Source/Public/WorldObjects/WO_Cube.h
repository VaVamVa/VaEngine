#pragma once

#include "Object/WorldObject.h"
#include "Mesh/MeshPrimitive.h"

#include <memory>

class IRenderDevice;

class WO_Cube : public WorldObject
{
public:
    void Initialize(IRenderDevice* device);

    MeshPrimitive* GetMesh() const { return mesh.get(); }

private:
    std::unique_ptr<MeshPrimitive> mesh;
};
