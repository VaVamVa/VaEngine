#include "Render/ILight.h"
#include "Render/Light.h"

namespace LightFactory {

std::unique_ptr<IDirectionalLight> CreateDirectionalLight()
{
    return std::make_unique<DirectionalLight>();
}

std::unique_ptr<IPointLight> CreatePointLight()
{
    return std::make_unique<PointLight>();
}

std::unique_ptr<ISpotLight> CreateSpotLight()
{
    return std::make_unique<SpotLight>();
}

} // namespace LightFactory
