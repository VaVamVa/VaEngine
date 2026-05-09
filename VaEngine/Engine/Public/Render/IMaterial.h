#pragma once

// GPU 레이아웃 — Lighting.hlsli Material 구조체와 일치 (64 bytes)
struct MaterialData
{
    float ambient[4]  = { 0.2f, 0.2f, 0.2f, 1.0f };
    float diffuse[4]  = { 0.8f, 0.8f, 0.8f, 1.0f };
    float specular[4] = { 0.5f, 0.5f, 0.5f, 32.0f }; // w = shininess
    float reflect[4]  = { 0.0f, 0.0f, 0.0f, 1.0f  };
};

class IMaterial
{
public:
    virtual ~IMaterial() = default;

    virtual const MaterialData& GetData() const = 0;

    virtual void SetAmbient (float r, float g, float b, float a = 1.0f) = 0;
    virtual void SetDiffuse (float r, float g, float b, float a = 1.0f) = 0;
    virtual void SetSpecular(float r, float g, float b, float shininess) = 0;
    virtual void SetReflect (float r, float g, float b, float a = 1.0f) = 0;
};
