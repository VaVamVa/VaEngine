#pragma once

#include "MeshData.h"

class PrimitiveShape
{
public:
    virtual ~PrimitiveShape() = default;
    virtual MeshData Build() const = 0;
};
