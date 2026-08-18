#pragma once

#include "RHI/IRenderDevice.h"
#include "RHI/Buffer/IBuffer.h"
#include "RHI/Shader/IShader.h"
#include "RHI/Pipeline/IBindingLayout.h"
#include "RHI/Pipeline/IPipelineState.h"
#include "Math/Container.h"

#include <memory>
#include <cstdint>

class RenderGraph;
class RenderScene;
class GBufferRenderer;
class ICommandList;
class IDepthBuffer;
class ITextureUAV;
class IColorBuffer;

class DeferredLightingRenderer
{
public:
    void Initialize(IRenderDevice* device, const ShaderDesc& csDesc,
                    uint32_t width, uint32_t height);

    // shadowDepthHandle: ShadowMapRenderer가 그래프에 등록한 트랜지언트 depth 핸들(배열, kMaxCascadeCount 슬라이스 고정)
    // cascadeViewProj/splitDistances: ShadowMapRenderer::GetLightViewProj()/GetSplitDistances() — 각각
    //   kMaxCascadeCount개/(kMaxCascadeCount-1)개 원소 배열(활성치 이후는 미사용).
    // activeCascadeCount: ShadowMapRenderer::GetActiveCascadeCount() — 셰이더가 실제로 사용할 캐스케이드 개수.
    // blendWidth: ShadowMapRenderer::GetBlendWidth().
    // irradianceMap/prefilteredMap/brdfLUT: IBLRenderer가 반환하는 IBL 산출물(Stage A/B)
    // iblEnabled: 스카이박스가 없어 irradianceMap/prefilteredMap이 계산되지 않은 폴백이면 false —
    //             셰이더가 이 텍스처들을 아예 샘플링하지 않고 기존 flat ambient로 대체한다.
    // ssaoMap: SSAORenderer::GetBlurredSSAO() — 간접광(ambient/IBL)에만 곱해진다.
    // ssaoEnabled: 런타임 디버그 토글(비교분석용) — false면 셰이더가 ssaoMap을 아예 샘플링하지 않는다.
    // showCascades: RenderScene::GetShowCascades() — true면 픽셀의 캐스케이드 인덱스를 색으로 덧칠.
    void AddPasses(RenderGraph& graph, GBufferRenderer& gbuffer,
                   uint32_t shadowDepthHandle, const Matrix4x4* cascadeViewProj, const float* splitDistances,
                   uint32_t activeCascadeCount, float blendWidth, uint32_t shadowResolution,
                   ITextureUAV* irradianceMap, ITextureUAV* prefilteredMap, ITextureUAV* brdfLUT,
                   bool iblEnabled, IColorBuffer* ssaoMap, bool ssaoEnabled, bool showCascades);

    // DeferredLightingPass::Execute 에서 호출
    void Dispatch(ICommandList* cmdList, GBufferRenderer& gbuffer, const RenderScene& scene,
                  IDepthBuffer* shadowDepth, const Matrix4x4* cascadeViewProj, const float* splitDistances,
                  uint32_t activeCascadeCount, float blendWidth, uint32_t shadowResolution,
                  ITextureUAV* irradianceMap, ITextureUAV* prefilteredMap, ITextureUAV* brdfLUT,
                  bool iblEnabled, IColorBuffer* ssaoMap, bool ssaoEnabled, bool showCascades);

private:
    std::unique_ptr<IBindingLayout> computeBindingLayout;
    std::unique_ptr<IShader>        computeShader;
    std::unique_ptr<IPipelineState> computePipelineState;

    std::unique_ptr<IBuffer> cameraBuffer;      // CB_DeferredCamera (b0)
    std::unique_ptr<IBuffer> lightsBuffer;      // CB_Lights         (b2)
    std::unique_ptr<IBuffer> shadowLightVPBuffer; // CB_ShadowLightVP (b1, append)

    uint32_t width  = 0;
    uint32_t height = 0;
};
