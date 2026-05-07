#pragma once

#include "MeshPrimitive.h"

class Cube : public MeshPrimitive
{
protected:
	void BuildGeometry() override;
};
