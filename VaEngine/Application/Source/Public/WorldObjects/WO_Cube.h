#pragma once

#include "Object/WorldObject.h"
#include "Mesh/MeshPrimitive.h"

#include <memory>

class IRenderDevice;
class RenderScene;

class WO_Cube : public WorldObject
{
public:
    void Initialize(IRenderDevice* device);

protected:
    void Impl_AddToScene(RenderScene& scene) const override;

private:
    std::unique_ptr<MeshPrimitive> mesh;
};
