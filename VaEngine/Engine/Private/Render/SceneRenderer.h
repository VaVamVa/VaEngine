#pragma once

#include "GBufferRenderer.h"
#include "DeferredLightingRenderer.h"
#include "ForwardRenderer.h"
#include "AnimationRenderer.h"

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
    void InitializeAnimation(IRenderDevice* device, const ShaderDesc& shaderDesc);
    void InitializeGBufferSkinned(IRenderDevice* device, const ShaderDesc& shaderDesc);
    void InitializeDebugLines(IRenderDevice* device, const ShaderDesc& shaderDesc);
    void InitializeDebugText(IRenderDevice* device, const ShaderDesc& glyphDesc, const char* fontPath);

    // Execute.cpp 에서 한 번만 호출
    void AddPasses(RenderGraph& graph, const FrameOutput& output, const RenderScene& scene);

    // Execute.cpp 에서 renderGraph.Execute 직후 호출 — 영속 리소스 상태를 다음 프레임용으로 갱신
    void UpdateResourceStates(const RenderGraph& graph);

private:
    GBufferRenderer          gbufferRenderer;
    DeferredLightingRenderer lightingRenderer;
    ForwardRenderer          forwardRenderer;
    AnimationRenderer        animationRenderer;

    // hdrOut → backbuffer Blit PSO
    std::unique_ptr<IBindingLayout> blitLayout;
    std::unique_ptr<IShader>        blitShader;
    std::unique_ptr<IPipelineState> blitPSO;

    // 영속 Depth 버퍼 (SceneRenderer 소유 → GBufferRenderer에 포인터로 전달)
    std::unique_ptr<IDepthBuffer> depth;

    // 영속 리소스의 프레임 간 상태 추적 (RenderGraph::Reset 후 재 Import 시 사용)
    EResourceState rt0State   = EResourceState::RenderTarget;
    EResourceState rt1State   = EResourceState::RenderTarget;
    EResourceState rt2State   = EResourceState::RenderTarget;
    EResourceState hdrState   = EResourceState::UnorderedAccess;
    EResourceState depthState = EResourceState::DepthWrite;

    uint32_t width  = 0;
    uint32_t height = 0;
};
