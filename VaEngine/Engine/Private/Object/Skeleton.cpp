#include "Object/Skeleton.h"

int Skeleton::FindBone(const std::string& name) const
{
    for (int i = 0; i < static_cast<int>(bones.size()); ++i)
        if (bones[i].name == name)
            return i;
    return -1;
}
