#pragma once

#include "Render/IMaterial.h"

class Material : public IMaterial
{
public:
    const MaterialData& GetData() const override { return data; }

    void SetAmbient(float r, float g, float b, float a) override
        { data.ambient[0]=r; data.ambient[1]=g; data.ambient[2]=b; data.ambient[3]=a; }

    void SetDiffuse(float r, float g, float b, float a) override
        { data.diffuse[0]=r; data.diffuse[1]=g; data.diffuse[2]=b; data.diffuse[3]=a; }

    void SetSpecular(float r, float g, float b, float shininess) override
        { data.specular[0]=r; data.specular[1]=g; data.specular[2]=b; data.specular[3]=shininess; }

    void SetReflect(float r, float g, float b, float a) override
        { data.reflect[0]=r; data.reflect[1]=g; data.reflect[2]=b; data.reflect[3]=a; }

private:
    MaterialData data;
};
