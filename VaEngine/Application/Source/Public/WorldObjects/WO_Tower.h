#pragma once

#include "Object/WorldModel.h"

class IRenderDevice;

class WO_Tower : public WorldModel
{
public:
    void Initialize(IRenderDevice* device);
};
