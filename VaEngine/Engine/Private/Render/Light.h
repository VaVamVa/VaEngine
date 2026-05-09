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
    void SetAmbient(float r, float g, float b, float a) override
        { data.ambient[0]=r; data.ambient[1]=g; data.ambient[2]=b; data.ambient[3]=a; }
    void SetDiffuse(float r, float g, float b, float a) override
        { data.diffuse[0]=r; data.diffuse[1]=g; data.diffuse[2]=b; data.diffuse[3]=a; }
    void SetSpecular(float r, float g, float b, float a) override
        { data.specular[0]=r; data.specular[1]=g; data.specular[2]=b; data.specular[3]=a; }

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
    void SetRange(float range) override
        { data.range = range; }
    void SetAttenuation(float a0, float a1, float a2) override
        { data.attenuation[0]=a0; data.attenuation[1]=a1; data.attenuation[2]=a2; }
    void SetAmbient(float r, float g, float b, float a) override
        { data.ambient[0]=r; data.ambient[1]=g; data.ambient[2]=b; data.ambient[3]=a; }
    void SetDiffuse(float r, float g, float b, float a) override
        { data.diffuse[0]=r; data.diffuse[1]=g; data.diffuse[2]=b; data.diffuse[3]=a; }
    void SetSpecular(float r, float g, float b, float a) override
        { data.specular[0]=r; data.specular[1]=g; data.specular[2]=b; data.specular[3]=a; }

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
    void SetRange(float range) override
        { data.range = range; }
    void SetSpot(float spot) override
        { data.spot = spot; }
    void SetAttenuation(float a0, float a1, float a2) override
        { data.attenuation[0]=a0; data.attenuation[1]=a1; data.attenuation[2]=a2; }
    void SetAmbient(float r, float g, float b, float a) override
        { data.ambient[0]=r; data.ambient[1]=g; data.ambient[2]=b; data.ambient[3]=a; }
    void SetDiffuse(float r, float g, float b, float a) override
        { data.diffuse[0]=r; data.diffuse[1]=g; data.diffuse[2]=b; data.diffuse[3]=a; }
    void SetSpecular(float r, float g, float b, float a) override
        { data.specular[0]=r; data.specular[1]=g; data.specular[2]=b; data.specular[3]=a; }

private:
    SpotLightData data;
    bool          enabled = true;
};
