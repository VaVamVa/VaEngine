#pragma once

#include "RHI/IRenderDevice.h"
#include "RHI/Texture/ITextureUAV.h"
#include "RHI/Texture/ITexture.h"
#include "RHI/Shader/IShader.h"
#include "RHI/Pipeline/IBindingLayout.h"
#include "RHI/Pipeline/IPipelineState.h"
#include "RHI/Buffer/IBuffer.h"

#include <memory>
#include <vector>
#include <cstdint>

// 파노라마(equirectangular) 환경 텍스처로부터 IBL 산출물을 계산·캐싱한다.
// - Diffuse Irradiance(Stage A) + Specular Prefiltered Map(Stage B): 스카이박스별 캐시 대상 —
//   targets 컬렉션에서 관리(GBufferRenderer 등과 동일 컨벤션: 대상마다 별도 인스턴스를 만들지 않고
//   하나의 Renderer가 컬렉션을 내부에서 관리).
// - BRDF LUT: roughness·NdotV에만 의존하고 환경 콘텐츠와 무관 — 스카이박스 캐시와 별개로
//   Initialize() 시점에 전역으로 1회만 계산한다(생명주기가 다름, EnsurePrecomputed 대상 아님).
//
// 계산은 RenderGraph를 거치지 않고 IRenderDevice::ImmediateSubmit으로 1회만 동기 실행한다.
class IBLRenderer
{
public:
    void Initialize(IRenderDevice* device,
                    const ShaderDesc& irradianceShaderDesc,
                    const ShaderDesc& prefilterShaderDesc,
                    const ShaderDesc& integrateBRDFShaderDesc);

    // sourcePanorama에 대한 IBL 산출물이 이미 있으면 그 핸들을 반환, 처음 보는 포인터면
    // ImmediateSubmit으로 즉시(1회) 계산 후 캐싱한다. 매 프레임 호출해도 안전(idempotent) —
    // SceneRenderer::AddPasses가 scene.GetSkybox()로 매 프레임 호출.
    // sourcePanorama == nullptr이면 무효 핸들을 반환한다.
    uint32_t EnsurePrecomputed(IRenderDevice* device, ITexture* sourcePanorama);

    // 무효 핸들이거나 아직 계산 전이면 폴백을 반환 — 호출부가 널 체크를 할 필요 없다.
    ITextureUAV* GetIrradianceMap(uint32_t handle) const;
    ITextureUAV* GetPrefilteredMap(uint32_t handle) const;
    // 스카이박스와 무관 — handle 불필요, 항상 유효(Initialize에서 1회 계산 완료).
    ITextureUAV* GetBRDFLUT() const { return brdfLUT.get(); }

    static constexpr uint32_t kInvalidHandle = 0xFFFFFFFFu;

private:
    struct IBLTarget
    {
        ITexture*                    sourcePanorama = nullptr;  // 캐싱 키(비교용) — 소유 아님
        std::unique_ptr<ITextureUAV> irradianceMap;
        std::unique_ptr<ITextureUAV> prefilteredMap;  // 밉 체인 (kPrefilterMipLevels)
    };

    void ComputeIrradiance(IRenderDevice* device, ITexture* source, ITextureUAV* output);
    void ComputePrefiltered(IRenderDevice* device, ITexture* source, ITextureUAV* output);
    void ComputeBRDFLUT(IRenderDevice* device);

    // Stage A — Diffuse Irradiance
    std::unique_ptr<IBindingLayout> irradianceBindingLayout;
    std::unique_ptr<IShader>        irradianceShader;
    std::unique_ptr<IPipelineState> irradiancePipelineState;

    // Stage B — Specular Prefiltered Map
    std::unique_ptr<IBindingLayout> prefilterBindingLayout;
    std::unique_ptr<IShader>        prefilterShader;
    std::unique_ptr<IPipelineState> prefilterPipelineState;
    std::unique_ptr<IBuffer>        prefilterParamsBuffer;  // b0 — CB_PrefilterParams(roughness), 밉마다 재업로드

    // Stage B — BRDF LUT (전역, 스카이박스 무관)
    std::unique_ptr<IBindingLayout> brdfBindingLayout;
    std::unique_ptr<IShader>        brdfShader;
    std::unique_ptr<IPipelineState> brdfPipelineState;
    std::unique_ptr<ITextureUAV>    brdfLUT;

    // 폴백 — 계산은 절대 하지 않는다(스카이박스 없을 때 컴퓨트 디스패치 자체를 막기 위함).
    // 리소스만 만들어 SRV 바인딩 대상으로 유효하게 두고, 내용은 셰이더가 gIBLEnabled 플래그로
    // 아예 읽지 않으므로(EvalAmbientIBL) 초기화되지 않은 값이어도 무해하다.
    std::unique_ptr<ITextureUAV> defaultIrradiance;
    std::unique_ptr<ITextureUAV> defaultPrefiltered;
    std::vector<IBLTarget>       targets;

    static constexpr uint32_t kIrradianceWidth  = 64;
    static constexpr uint32_t kIrradianceHeight = 32;

    // Lighting.hlsli의 IBL_SPEC_MIP_COUNT와 반드시 동기화.
    static constexpr uint32_t kPrefilterMipLevels = 5;
    static constexpr uint32_t kPrefilterWidth     = 128;
    static constexpr uint32_t kPrefilterHeight    = 64;

    static constexpr uint32_t kBRDFLUTSize = 128;
};
