#include "WorldObjects/WO_Katana.h"

void WO_Katana::Initialize(IRenderDevice* device)
{
	const std::string dir = std::string(ASSETS_DIR) + "Katana";
	WorldModel::Initialize(device, dir + "/Katana.mesh", dir + "/Katana	.matl");
}
