#include "WorldObjects/WO_Tower.h"

void WO_Tower::Initialize(IRenderDevice* device)
{
    const std::string dir = std::string(ASSETS_DIR) + "Tower";
    WorldModel::Initialize(device, dir + "/Tower.mesh", dir + "/Tower.matl");
}
