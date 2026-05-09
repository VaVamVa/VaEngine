#pragma once

#include "Scene/Transform.h"

class WorldObject
{
public:
    virtual ~WorldObject() = default;

    Transform transform;
};
