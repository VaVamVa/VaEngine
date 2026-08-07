#pragma once

#include "RHI/Pipeline/PipelineDesc.h"

#include <cstdint>

class IBuffer;
class ITexture;
class IRenderDevice;

// GPU 레이아웃 — GBuffer b1 (HLSL GBufferMaterial 구조체와 1:1, 48 bytes)
struct MaterialData
{
    float albedo[4]      = { 1.f, 1.f, 1.f, 1.f };  // base color tint (RT0.RGB 곱연산)
    float emissive[3]    = { 0.f, 0.f, 0.f };        // 자체 발광
    float roughness      = 0.5f;                      // → RT1.W
    float metallic       = 0.0f;                      // → RT2.R
    float ao             = 1.0f;                      // → RT0.A
    float alphaThreshold = 0.0f;                      // Cutout용. GBuffer PS에서 clip()
    float _pad           = 0.0f;
};

class IMaterial
{
public:
    virtual ~IMaterial() = default;

    // 렌더링 모드 — Renderer가 PSO 선택에 사용
    virtual EBlendMode  GetBlendMode()      const = 0;
    virtual EBackfaceCullMode GetCullMode() const = 0;
    virtual bool        IsDepthWrite()      const = 0;
    virtual float       GetAlphaThreshold() const = 0;
    virtual uint16_t    GetID()             const = 0;

    // 텍스처 슬롯 — non-owning 포인터. 소유권은 WorldObject 서브클래스
    virtual ITexture*  GetAlbedoTexture()         const = 0;
    virtual ITexture*  GetNormalTexture()          const = 0;
    virtual void       SetAlbedoTexture(ITexture*)      = 0;
    virtual void       SetNormalTexture(ITexture*)      = 0;

    // GPU 파라미터 버퍼 — Material 자신이 소유·관리
    virtual IBuffer*            GetBuffer() const = 0;
    virtual const MaterialData& GetData()   const = 0;

    // PBR 파라미터 세터
    virtual void SetAlbedo   (float r, float g, float b, float a = 1.f) = 0;
    virtual void SetEmissive (float r, float g, float b)                = 0;
    virtual void SetRoughness(float v)                                  = 0;
    virtual void SetMetallic (float v)                                  = 0;
    virtual void SetAO       (float v)                                  = 0;

    // 렌더링 모드 세터
    virtual void SetBlendMode     (EBlendMode) = 0;
    virtual void SetCullMode      (EBackfaceCullMode) = 0;
    virtual void SetDepthWrite    (bool)       = 0;
    virtual void SetAlphaThreshold(float)      = 0;

    // Dirty flag 업로드 — GBufferRenderer·ForwardRenderer 그룹 변경 시 호출
    virtual void UpdateBufferIfDirty() = 0;

    // GPU 버퍼 초기화 — WorldObject::Initialize(IRenderDevice*) 에서 호출
    virtual void Initialize(IRenderDevice* device) = 0;
};
