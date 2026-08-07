#pragma once

#include "Render/IMaterial.h"
#include "RHI/Buffer/IBuffer.h"

#include <memory>

class Material : public IMaterial
{
public:
    Material();
    ~Material() override = default;

    void Initialize(IRenderDevice* device) override;
    void UpdateBufferIfDirty()             override;

    EBlendMode  GetBlendMode()      const override { return blendMode; }
    EBackfaceCullMode GetCullMode() const override { return cullMode; }
    bool        IsDepthWrite()      const override { return depthWrite; }
    float       GetAlphaThreshold() const override { return data.alphaThreshold; }
    uint16_t    GetID()             const override { return id; }

    ITexture*  GetAlbedoTexture() const override { return albedoTexture; }
    ITexture*  GetNormalTexture() const override { return normalTexture; }
    void SetAlbedoTexture(ITexture* tex) override { albedoTexture = tex; }
    void SetNormalTexture(ITexture* tex) override { normalTexture = tex; }

    IBuffer*            GetBuffer() const override { return buffer.get(); }
    const MaterialData& GetData()   const override { return data; }

    void SetAlbedo   (float r, float g, float b, float a = 1.f) override;
    void SetEmissive (float r, float g, float b)                override;
    void SetRoughness(float v)                                  override;
    void SetMetallic (float v)                                  override;
    void SetAO       (float v)                                  override;

    void SetBlendMode     (EBlendMode m) override { blendMode  = m; }
    void SetCullMode      (EBackfaceCullMode m) override { cullMode = m; }
    void SetDepthWrite    (bool v)       override { depthWrite = v; }
    void SetAlphaThreshold(float v)      override { data.alphaThreshold = v; dirty = true; }

private:
    MaterialData data;
    bool         dirty      = true;
    EBlendMode   blendMode  = EBlendMode::Opaque;
    EBackfaceCullMode cullMode = EBackfaceCullMode::Back;
    bool         depthWrite = true;
    uint16_t     id         = 0;

    ITexture*                albedoTexture = nullptr;
    ITexture*                normalTexture = nullptr;
    std::unique_ptr<IBuffer> buffer;
};
