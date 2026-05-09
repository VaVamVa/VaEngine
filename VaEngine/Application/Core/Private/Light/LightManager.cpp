#include "Light/LightManager.h"
#include "Scene/RenderScene.h"

#include <cstring>

LightManager::LightManager()
    : dirLight(LightFactory::CreateDirectionalLight())
{
}

LightManager::~LightManager() = default;

IPointLight* LightManager::AddPointLight()
{
    pointLights.push_back(LightFactory::CreatePointLight());
    return pointLights.back().get();
}

IPointLight* LightManager::GetPointLight(uint32_t index)
{
    return index < static_cast<uint32_t>(pointLights.size()) ? pointLights[index].get() : nullptr;
}

void LightManager::RemovePointLight(uint32_t index)
{
    if (index < static_cast<uint32_t>(pointLights.size()))
        pointLights.erase(pointLights.begin() + index);
}

uint32_t LightManager::GetPointLightCount() const
{
    return static_cast<uint32_t>(pointLights.size());
}

ISpotLight* LightManager::AddSpotLight()
{
    spotLights.push_back(LightFactory::CreateSpotLight());
    return spotLights.back().get();
}

ISpotLight* LightManager::GetSpotLight(uint32_t index)
{
    return index < static_cast<uint32_t>(spotLights.size()) ? spotLights[index].get() : nullptr;
}

void LightManager::RemoveSpotLight(uint32_t index)
{
    if (index < static_cast<uint32_t>(spotLights.size()))
        spotLights.erase(spotLights.begin() + index);
}

uint32_t LightManager::GetSpotLightCount() const
{
    return static_cast<uint32_t>(spotLights.size());
}

void LightManager::Submit(RenderScene* scene) const
{
    LightingState state;
    std::memset(&state.dirLight, 0, sizeof(state.dirLight));  // default member 초기값 덮어쓰기

    if (dirLight->IsEnabled())
        state.dirLight = dirLight->GetData();

    for (const auto& pl : pointLights)
        if (pl->IsEnabled())
            state.pointLights.push_back(pl->GetData());

    for (const auto& sl : spotLights)
        if (sl->IsEnabled())
            state.spotLights.push_back(sl->GetData());

    scene->SetLighting(state);
}
