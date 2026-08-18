#pragma once

#include "Object/WorldModel.h"

class IRenderDevice;

class WO_Katana : public WorldModel
{
public:
	void Initialize(IRenderDevice* device);
};