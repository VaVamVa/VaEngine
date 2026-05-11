#pragma once

#include "PrimitiveShape.h"

// subdivisions=6 이하 권장 (uint16_t 한계: ~40k 정점)
class IcoSphereShape : public PrimitiveShape
{
public:
    explicit IcoSphereShape(float radius = 0.5f, int subdivisions = 3)
        : radius(radius), subdivisions(subdivisions) {}

    MeshData Build() const override;

    float radius;
    int   subdivisions;
};
