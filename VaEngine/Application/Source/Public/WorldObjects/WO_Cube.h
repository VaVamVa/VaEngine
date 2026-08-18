#pragma once

#include "Object/WorldModel.h"

class IRenderDevice;

class WO_Cube : public WorldModel
{
public:
    void Initialize(IRenderDevice* device);
};
