#pragma once

#include "Object/WorldAnimatedModel.h"

class IRenderDevice;

class WO_Kachujin : public WorldAnimatedModel
{
public:
    void Initialize(IRenderDevice* device);
};
