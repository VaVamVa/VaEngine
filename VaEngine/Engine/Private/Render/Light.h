#pragma once

#include "Render/ILight.h"

class DirectionalLight : public IDirectionalLight
{
public:
    const DirectionalLightData& GetData() const override { return data; }

    void SetEnabled(bool e) override { enabled = e; }
    bool IsEnabled()  const override { return enabled; }

    void SetDirection(float x, float y, float z) override
        { data.direction[0]=x; data.direction[1]=y; data.direction[2]=z; }
    void SetColor(float r, float g, float b) override
        { data.color[0]=r; data.color[1]=g; data.color[2]=b; }
    void SetIntensity(float i) override
        { data.intensity = i; }

private:
    DirectionalLightData data;
    bool                 enabled = true;
};

class PointLight : public IPointLight
{
public:
    const PointLightData& GetData() const override { return data; }

    void SetEnabled(bool e) override { enabled = e; }
    bool IsEnabled()  const override { return enabled; }

    void SetPosition(float x, float y, float z) override
        { data.position[0]=x; data.position[1]=y; data.position[2]=z; }
    void SetColor(float r, float g, float b) override
        { data.color[0]=r; data.color[1]=g; data.color[2]=b; }
    void SetIntensity(float i) override
        { data.intensity = i; }
    void SetRange(float range) override
        { data.range = range; }
    void SetAttenuation(float a0, float a1, float a2) override
        { data.attenuation[0]=a0; data.attenuation[1]=a1; data.attenuation[2]=a2; }

private:
    PointLightData data;
    bool           enabled = true;
};

class SpotLight : public ISpotLight
{
public:
    const SpotLightData& GetData() const override { return data; }

    void SetEnabled(bool e) override { enabled = e; }
    bool IsEnabled()  const override { return enabled; }

    void SetPosition(float x, float y, float z) override
        { data.position[0]=x; data.position[1]=y; data.position[2]=z; }
    void SetDirection(float x, float y, float z) override
        { data.direction[0]=x; data.direction[1]=y; data.direction[2]=z; }
    void SetColor(float r, float g, float b) override
        { data.color[0]=r; data.color[1]=g; data.color[2]=b; }
    void SetIntensity(float i) override
        { data.intensity = i; }
    void SetRange(float range) override
        { data.range = range; }
    void SetSpot(float spot) override
        { data.spot = spot; }
    void SetAttenuation(float a0, float a1, float a2) override
        { data.attenuation[0]=a0; data.attenuation[1]=a1; data.attenuation[2]=a2; }

private:
    SpotLightData data;
    bool          enabled = true;
};
