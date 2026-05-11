#pragma once

#include "PrimitiveShape.h"

class UVSphereShape : public PrimitiveShape
{
public:
    explicit UVSphereShape(float radius = 0.5f, int stacks = 18, int slices = 36)
        : radius(radius), stacks(stacks), slices(slices) {}

    MeshData Build() const override;

    float radius;
    int   stacks, slices;
};
