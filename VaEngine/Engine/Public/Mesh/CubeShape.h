#pragma once

#include "PrimitiveShape.h"

class CubeShape : public PrimitiveShape
{
public:
    explicit CubeShape(float halfX = 0.5f, float halfY = 0.5f, float halfZ = 0.5f)
        : halfX(halfX), halfY(halfY), halfZ(halfZ) {}

    MeshData Build() const override;

    float halfX, halfY, halfZ;
};
