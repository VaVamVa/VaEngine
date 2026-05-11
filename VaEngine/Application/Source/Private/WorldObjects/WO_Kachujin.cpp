#include "WorldObjects/WO_Kachujin.h"

void WO_Kachujin::Initialize(IRenderDevice* device)
{
    const std::string dir = std::string(ASSETS_DIR) + "Kachujin/";

    // 클립 파일 목록 — ImportTool이 생성하는 stem_ClipName.clip 형식에 맞게 추가
    const std::vector<std::string> clipPaths = {
        dir + "Salsa_Dancing.clip",
        dir + "Sword_And_Shield_Idle.clip",
		dir + "Sword_And_Shield_Slash.clip",
    };

    WorldAnimatedModel::Initialize(device,
        dir + "Kachujin.smesh",
        clipPaths,
        dir + "Kachujin_Diffuse.png");
}
