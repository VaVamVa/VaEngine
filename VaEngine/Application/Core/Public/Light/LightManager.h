#pragma once

#include "Render/ILight.h"

#include <memory>
#include <vector>

class RenderScene;

class LightManager
{
public:
    LightManager();
    ~LightManager();

    // Directional light (1개)
    IDirectionalLight* GetDirectionalLight() { return dirLight.get(); }

    // Point lights
    IPointLight* AddPointLight();
    IPointLight* GetPointLight(uint32_t index);
    void         RemovePointLight(uint32_t index);
    uint32_t     GetPointLightCount() const;

    // Spot lights
    ISpotLight*  AddSpotLight();
    ISpotLight*  GetSpotLight(uint32_t index);
    void         RemoveSpotLight(uint32_t index);
    uint32_t     GetSpotLightCount() const;

    // 매 프레임 RenderScene에 LightingState 제출
    void Submit(RenderScene* scene) const;

private:
    std::unique_ptr<IDirectionalLight>        dirLight;
    std::vector<std::unique_ptr<IPointLight>> pointLights;
    std::vector<std::unique_ptr<ISpotLight>>  spotLights;
};
