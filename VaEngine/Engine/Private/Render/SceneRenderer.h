#pragma once

#include "GBufferRenderer.h"
#include "DeferredLightingRenderer.h"
#include "ForwardRenderer.h"
#include "AnimationRenderer.h"
#include "ShadowMapRenderer.h"
#include "BloomRenderer.h"
#include "IBLRenderer.h"
#include "SSAORenderer.h"

#include "RHI/IRenderDevice.h"
#include "RHI/Buffer/IDepthBuffer.h"
#include "RHI/Buffer/IBuffer.h"
#include "RHI/Shader/IShader.h"
#include "RHI/Pipeline/IBindingLayout.h"
#include "RHI/Pipeline/IPipelineState.h"
#include "RHI/Common_RHI.h"
#include "Render/RenderGraph.h"

#include <memory>
#include <cstdint>

class RenderScene;
struct FrameOutput;

class SceneRenderer
{
public:
    // 1. 공통 초기화 (depth 버퍼 생성 — 반드시 먼저 호출)
    void Initialize(IRenderDevice* device, uint32_t width, uint32_t height);

    // 2. 서브 렌더러 초기화 (Initialize 이후 순서 무관)
    void InitializeGBuffer(IRenderDevice* device, const ShaderDesc& shaderDesc);
    void InitializeLighting(IRenderDevice* device, const ShaderDesc& shaderDesc);
    void InitializeBlit(IRenderDevice* device, const ShaderDesc& shaderDesc);
    void InitializeSky(IRenderDevice* device, const ShaderDesc& shaderDesc);
    void InitializeForward(IRenderDevice* device, const ShaderDesc& shaderDesc);
    void InitializeTransparentForward(IRenderDevice* device, const ShaderDesc& shaderDesc,
                                      const ShaderDesc& compositeShaderDesc);
    void InitializeAnimation(IRenderDevice* device, const ShaderDesc& shaderDesc);
    void InitializeGBufferSkinned(IRenderDevice* device, const ShaderDesc& shaderDesc);
    void InitializeShadowMap(IRenderDevice* device, const ShaderDesc& shaderDesc, uint32_t resolution = 2048);
    void InitializeShadowMapSkinned(IRenderDevice* device, const ShaderDesc& shaderDesc);
    void InitializeBloom(IRenderDevice* device,
                         const ShaderDesc& brightDesc, const ShaderDesc& blurHDesc,
                         const ShaderDesc& blurVDesc,  const ShaderDesc& compositeDesc);
    void InitializeDebugLines(IRenderDevice* device, const ShaderDesc& shaderDesc);
    void InitializeDebugText(IRenderDevice* device, const ShaderDesc& glyphDesc, const char* fontPath);
    void InitializeIBL(IRenderDevice* device,
                       const ShaderDesc& irradianceShaderDesc,
                       const ShaderDesc& prefilterShaderDesc,
                       const ShaderDesc& integrateBRDFShaderDesc);
    void InitializeSSAO(IRenderDevice* device, const ShaderDesc& ssaoDesc, const ShaderDesc& blurDesc);

    // Execute.cpp 에서 한 번만 호출. device는 IBLRenderer의 1회성 ImmediateSubmit 프리컴퓨트에 필요.
    void AddPasses(RenderGraph& graph, const FrameOutput& output, const RenderScene& scene, IRenderDevice* device);

private:
    GBufferRenderer          gbufferRenderer;
    DeferredLightingRenderer lightingRenderer;
    ForwardRenderer          forwardRenderer;
    AnimationRenderer        animationRenderer;
    ShadowMapRenderer        shadowMapRenderer;
    BloomRenderer            bloomRenderer;
    IBLRenderer              iblRenderer;
    SSAORenderer             ssaoRenderer;

    // hdrOut → backbuffer Blit PSO (Tonemap + Gamma, Phase 1-1)
    std::unique_ptr<IBindingLayout> blitLayout;
    std::unique_ptr<IShader>        blitShader;
    std::unique_ptr<IPipelineState> blitPSO;
    std::unique_ptr<IBuffer>        tonemapBuffer;  // b0 — CB_Tonemap (Exposure)

    // 영속 Depth 버퍼 (SceneRenderer 소유 → GBufferRenderer에 포인터로 전달)
    std::unique_ptr<IDepthBuffer> depth;

    uint32_t width  = 0;
    uint32_t height = 0;
};
