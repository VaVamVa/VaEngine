#pragma once

#include "Interfaces/IActivate.h"

#include <memory>

enum class ELightType { Directional, Point, Spot };

// ── GPU 레이아웃 구조체 ── Lighting.hlsli 와 1:1 대응 ──────────────────────

// 64 bytes — DirectionalLight (HLSL)
struct DirectionalLightData
{
    float ambient[4]   = { 0.1f,  0.1f,  0.1f, 1.0f };
    float diffuse[4]   = { 0.8f,  0.8f,  0.8f, 1.0f };
    float specular[4]  = { 0.5f,  0.5f,  0.5f, 1.0f };
    float direction[3] = { 0.577f,-0.577f, 0.577f    };
    float _pad         = 0.0f;
};

// 80 bytes — PointLight (HLSL)
struct PointLightData
{
    float ambient[4]     = {};
    float diffuse[4]     = {};
    float specular[4]    = {};
    float position[3]    = {};
    float range          = 0.0f;
    float attenuation[3] = { 1.0f, 0.0f, 0.0f };
    float _pad           = 0.0f;
};

// 96 bytes — SpotLight (HLSL)
struct SpotLightData
{
    float ambient[4]     = {};
    float diffuse[4]     = {};
    float specular[4]    = {};
    float position[3]    = {};
    float range          = 0.0f;
    float direction[3]   = {};
    float spot           = 0.0f;
    float attenuation[3] = { 1.0f, 0.0f, 0.0f };
    float _pad           = 0.0f;
};

// ── 인터페이스 ─────────────────────────────────────────────────────────────

class ILight : public IActivate
{
public:
    virtual ~ILight() = default;
    virtual ELightType GetType() const = 0;
};

class IDirectionalLight : public ILight
{
public:
    ELightType GetType() const override { return ELightType::Directional; }

    virtual const DirectionalLightData& GetData() const = 0;
    virtual void SetDirection(float x, float y, float z) = 0;
    virtual void SetAmbient  (float r, float g, float b, float a = 1.0f) = 0;
    virtual void SetDiffuse  (float r, float g, float b, float a = 1.0f) = 0;
    virtual void SetSpecular (float r, float g, float b, float a = 1.0f) = 0;
};

class IPointLight : public ILight
{
public:
    ELightType GetType() const override { return ELightType::Point; }

    virtual const PointLightData& GetData() const = 0;
    virtual void SetPosition   (float x, float y, float z) = 0;
    virtual void SetRange      (float range) = 0;
    virtual void SetAttenuation(float a0, float a1, float a2) = 0;
    virtual void SetAmbient    (float r, float g, float b, float a = 1.0f) = 0;
    virtual void SetDiffuse    (float r, float g, float b, float a = 1.0f) = 0;
    virtual void SetSpecular   (float r, float g, float b, float a = 1.0f) = 0;
};

class ISpotLight : public ILight
{
public:
    ELightType GetType() const override { return ELightType::Spot; }

    virtual const SpotLightData& GetData() const = 0;
    virtual void SetPosition   (float x, float y, float z) = 0;
    virtual void SetDirection  (float x, float y, float z) = 0;
    virtual void SetRange      (float range) = 0;
    virtual void SetSpot       (float spot) = 0;
    virtual void SetAttenuation(float a0, float a1, float a2) = 0;
    virtual void SetAmbient    (float r, float g, float b, float a = 1.0f) = 0;
    virtual void SetDiffuse    (float r, float g, float b, float a = 1.0f) = 0;
    virtual void SetSpecular   (float r, float g, float b, float a = 1.0f) = 0;
};

// Application이 구체 타입(Light.h) 없이 조명을 생성할 수 있도록 제공
namespace LightFactory {
    std::unique_ptr<IDirectionalLight> CreateDirectionalLight();
    std::unique_ptr<IPointLight>       CreatePointLight();
    std::unique_ptr<ISpotLight>        CreateSpotLight();
}
