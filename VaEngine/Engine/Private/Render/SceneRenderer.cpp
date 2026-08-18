#include "SceneRenderer.h"

#include "Render/RenderGraph.h"
#include "Render/IRenderPass.h"
#include "Scene/RenderScene.h"

#include "RHI/IRenderDevice.h"
#include "RHI/ICommandList.h"
#include "RHI/Texture/ITextureUAV.h"
#include "RHI/Pipeline/PipelineDesc.h"
#include "RHI/Common_RHI.h"

#include <cstring>
#include <vector>
#include <cstdint>

// ── RenderPass 정의 ────────────────────────────────────────────────────────────

namespace {

// Sky를 hdrOut에 렌더링 (Deferred 경로: backBuffer 대신 hdrOut이 최초 타겟)
// 주의: ForwardRenderer::InitializeSky의 skyPsoDesc.rtvFormats 를
//       R16G16B16A16_FLOAT 로 맞춰야 format mismatch가 발생하지 않음
struct DeferredSkyPass : IRenderPass
{
    DeferredSkyPass(ForwardRenderer* renderer, ITextureUAV* hdrOut, const FrameOutput& output)
        : renderer(renderer), hdrOut(hdrOut), output(output) {}

    void OnCompile(RenderGraph& /*graph*/) override {}

    void DeclareResources(std::vector<PassResourceDecl>& /*reads*/,
                          std::vector<PassResourceDecl>& writes) const override
    {
        writes.push_back({ hdrOut, EResourceState::RenderTarget });
    }

    void Execute(ICommandList* cmdList, const RenderScene& scene) override
    {
        RenderPassDesc passDesc;
        passDesc.renderTargetCount            = 1;
        passDesc.renderTargets[0].view        = hdrOut->GetRTV();
        passDesc.renderTargets[0].loadAction  = ELoadAction::Clear;
        passDesc.renderTargets[0].storeAction = EStoreAction::Store;
        passDesc.renderTargets[0].clearColor[0] = 0.0f;
        passDesc.renderTargets[0].clearColor[1] = 0.0f;
        passDesc.renderTargets[0].clearColor[2] = 0.0f;
        passDesc.renderTargets[0].clearColor[3] = 1.0f;

        cmdList->BeginRenderPass(passDesc);
        cmdList->SetViewport(0.0f, 0.0f,
                             static_cast<float>(output.width),
                             static_cast<float>(output.height),
                             0.0f, 1.0f);
        cmdList->SetScissorRect(0, 0,
                                static_cast<int32_t>(output.width),
                                static_cast<int32_t>(output.height));
        renderer->RenderSky(cmdList, scene);
        cmdList->EndRenderPass();
    }

    ForwardRenderer* renderer;
    ITextureUAV*     hdrOut;
    FrameOutput      output;
};

// hdrOut SRV → backBuffer (SV_VertexID 기반 전체화면 삼각형, vertex buffer 불필요)
// Tonemap(ACES Filmic) + Gamma 보정을 포함 (Phase 1-1)
struct DeferredBlitPass : IRenderPass
{
    DeferredBlitPass(IPipelineState* blitPSO, ITextureUAV* hdrOut, const FrameOutput& output, IBuffer* tonemapBuffer)
        : blitPSO(blitPSO), hdrOut(hdrOut), output(output), tonemapBuffer(tonemapBuffer) {}

    void OnCompile(RenderGraph& /*graph*/) override {}

    void DeclareResources(std::vector<PassResourceDecl>& reads,
                          std::vector<PassResourceDecl>& writes) const override
    {
        reads.push_back({ hdrOut,             EResourceState::PixelShaderResource });
        writes.push_back({ output.backBuffer, EResourceState::RenderTarget         });
    }

    void Execute(ICommandList* cmdList, const RenderScene& /*scene*/) override
    {
        RenderPassDesc passDesc;
        passDesc.renderTargetCount            = 1;
        passDesc.renderTargets[0].view        = output.backBufferView;
        passDesc.renderTargets[0].loadAction  = ELoadAction::DontCare;
        passDesc.renderTargets[0].storeAction = EStoreAction::Store;
        std::memcpy(passDesc.renderTargets[0].clearColor, output.clearColor, sizeof(float) * 4);

        cmdList->BeginRenderPass(passDesc);
        cmdList->SetViewport(0.0f, 0.0f,
                             static_cast<float>(output.width),
                             static_cast<float>(output.height),
                             0.0f, 1.0f);
        cmdList->SetScissorRect(0, 0,
                                static_cast<int32_t>(output.width),
                                static_cast<int32_t>(output.height));

        blitPSO->Bind(cmdList);
        hdrOut->BindSRV(cmdList, 0, /*isCompute*/ false);   // root 0 → t0
        cmdList->SetConstantBuffer(tonemapBuffer, 1);       // root 1 → b0
        cmdList->DrawInstanced(3, 1);

        cmdList->EndRenderPass();
    }

    IPipelineState* blitPSO;
    ITextureUAV*    hdrOut;
    FrameOutput     output;
    IBuffer*        tonemapBuffer;
};

} // namespace

// ── Initialize ─────────────────────────────────────────────────────────────────

void SceneRenderer::Initialize(IRenderDevice* device, uint32_t w, uint32_t h)
{
    width  = w;
    height = h;
    depth  = device->CreateDepthBuffer(w, h, EPixelFormat::D24_UNORM_S8_UINT);
}

void SceneRenderer::InitializeGBuffer(IRenderDevice* device, const ShaderDesc& shaderDesc)
{
    gbufferRenderer.Initialize(device, depth.get(), shaderDesc, width, height);
}

void SceneRenderer::InitializeLighting(IRenderDevice* device, const ShaderDesc& csDesc)
{
    lightingRenderer.Initialize(device, csDesc, width, height);
}

void SceneRenderer::InitializeBlit(IRenderDevice* device, const ShaderDesc& shaderDesc)
{
    // 새 슬롯은 항상 끝에 append — root 0(t0) 불변, root 1 — b0 Tonemap CB 신규
    BindingEntry blitBindings[] = {
        { EBindingType::Texture,        0, EShaderStage::Pixel },  // root 0 → t0 (hdrOut SRV)
        { EBindingType::ConstantBuffer, 0, EShaderStage::Pixel },  // root 1 → b0 (CB_Tonemap)
    };
    blitLayout = device->CreateBindingLayout(blitBindings, 2);
    blitShader = device->CreateShader(shaderDesc);

    PipelineStateDesc psoDesc{};
    psoDesc.shader           = blitShader.get();
    psoDesc.bindingLayout    = blitLayout.get();
    psoDesc.vertexInputs     = nullptr;
    psoDesc.vertexInputCount = 0;
    psoDesc.rtvFormats[0]    = EPixelFormat::R8G8B8A8_UNORM;
    psoDesc.rtvCount         = 1;
    psoDesc.depthEnable      = false;
    blitPSO = device->CreatePipelineState(psoDesc);

    struct TonemapData { float exposure; float pad[3]; };
    tonemapBuffer = device->CreateBuffer({
        .size   = sizeof(TonemapData),
        .usage  = EBufferUsage::ConstantBuffer,
        .access = EMemoryAccess::Upload,
        .stride = 0
    });
    const TonemapData defaultTonemap{ 1.0f, {} };
    tonemapBuffer->Upload(&defaultTonemap, sizeof(defaultTonemap));
}

void SceneRenderer::InitializeSky(IRenderDevice* device, const ShaderDesc& shaderDesc)
{
    forwardRenderer.InitializeSky(device, shaderDesc);
}

void SceneRenderer::InitializeForward(IRenderDevice* device, const ShaderDesc& shaderDesc)
{
    forwardRenderer.Initialize(device, shaderDesc);
}

void SceneRenderer::InitializeTransparentForward(IRenderDevice* device, const ShaderDesc& shaderDesc,
                                                  const ShaderDesc& compositeShaderDesc)
{
    forwardRenderer.InitializeTransparent(device, shaderDesc, compositeShaderDesc, width, height);
}

void SceneRenderer::InitializeAnimation(IRenderDevice* device, const ShaderDesc& shaderDesc)
{
    animationRenderer.Initialize(device, shaderDesc);
}

void SceneRenderer::InitializeGBufferSkinned(IRenderDevice* device, const ShaderDesc& shaderDesc)
{
    gbufferRenderer.InitializeSkinned(device, shaderDesc);
}

void SceneRenderer::InitializeShadowMap(IRenderDevice* device, const ShaderDesc& shaderDesc, uint32_t resolution)
{
    shadowMapRenderer.Initialize(device, shaderDesc, resolution);
}

void SceneRenderer::InitializeShadowMapSkinned(IRenderDevice* device, const ShaderDesc& shaderDesc)
{
    shadowMapRenderer.InitializeSkinned(device, shaderDesc);
}

void SceneRenderer::InitializeBloom(IRenderDevice* device,
                                    const ShaderDesc& brightDesc, const ShaderDesc& blurHDesc,
                                    const ShaderDesc& blurVDesc,  const ShaderDesc& compositeDesc)
{
    bloomRenderer.Initialize(device, brightDesc, blurHDesc, blurVDesc, compositeDesc, width, height);
}

void SceneRenderer::InitializeDebugLines(IRenderDevice* device, const ShaderDesc& shaderDesc)
{
    forwardRenderer.InitializeDebugLines(device, shaderDesc);
}

void SceneRenderer::InitializeDebugText(IRenderDevice* device, const ShaderDesc& glyphDesc,
                                         const char* fontPath)
{
    forwardRenderer.InitializeDebugText(device, glyphDesc, fontPath);
}

void SceneRenderer::InitializeIBL(IRenderDevice* device,
                                  const ShaderDesc& irradianceShaderDesc,
                                  const ShaderDesc& prefilterShaderDesc,
                                  const ShaderDesc& integrateBRDFShaderDesc)
{
    iblRenderer.Initialize(device, irradianceShaderDesc, prefilterShaderDesc, integrateBRDFShaderDesc);
}

void SceneRenderer::InitializeSSAO(IRenderDevice* device, const ShaderDesc& ssaoDesc, const ShaderDesc& blurDesc)
{
    ssaoRenderer.Initialize(device, ssaoDesc, blurDesc, width, height);
}

// ── AddPasses ──────────────────────────────────────────────────────────────────

void SceneRenderer::AddPasses(RenderGraph& graph, const FrameOutput& output, const RenderScene& scene,
                               IRenderDevice* device)
{
    ITextureUAV* hdrOut = gbufferRenderer.GetHdrOut();

    // 별도 등록 불필요 — GBuffer RT/hdrOut/depth(BaseRHIResource)가 각자 자기 상태를 스스로 들고 있어
    // Compile()이 매 프레임 그 상태를 직접 읽고 갱신한다.

    // IBL(Diffuse Irradiance, Stage A) — 스카이박스 포인터가 처음 보는 것이면 ImmediateSubmit으로
    // 1회 계산 후 캐싱, 이미 계산된 포인터면 캐싱된 핸들만 반환(비용 무시 가능한 포인터 비교).
    // EnsurePrecomputed 자체는 토글과 무관하게 항상 호출(캐시 유지) — 스카이박스가 없거나
    // 사용자가 런타임 토글로 껐으면(scene.GetIBLEnabled()) 셰이더에 iblEnabled=false로 전달되어
    // 폴백 텍스처를 아예 샘플링하지 않는다.
    uint32_t iblHandle = iblRenderer.EnsurePrecomputed(device, scene.GetSkybox());
    ITextureUAV* irradianceMap  = iblRenderer.GetIrradianceMap(iblHandle);
    ITextureUAV* prefilteredMap = iblRenderer.GetPrefilteredMap(iblHandle);
    ITextureUAV* brdfLUT        = iblRenderer.GetBRDFLUT();
    bool iblEnabled = (iblHandle != IBLRenderer::kInvalidHandle) && scene.GetIBLEnabled();

    // 1. Sky → hdrOut (RenderTarget, Clear)
    graph.AddPass<DeferredSkyPass>(&forwardRenderer, hdrOut, output);

    // 2. Bone Palette Compute (스키닝 mesh 목록 반환) — Shadow Map의 스키닝 caster가 최신 본 데이터를
    //    읽어야 하므로 반드시 Shadow Map보다 먼저 실행되어야 한다.
    auto skinnedMeshes = animationRenderer.AddComputePasses(graph, scene);

    // 3. Shadow Map → 트랜지언트 depth 배열(CSM, Directional Light 기준, 화면과 다른 해상도로 alias 방지 §0-3)
    //    정적 + 스키닝 메시 모두 caster로 포함. 항상 kMaxCascadeCount(8)슬라이스로 고정 할당 — 실제 사용
    //    개수(activeCascadeCount)가 바뀌어도 재할당 없음(Mesh LOD 패턴, 260713-CompactLog#4).
    uint32_t shadowRes = shadowMapRenderer.GetResolution();
    uint32_t shadowDepthHandle = graph.DeclareTransientDepth({
        shadowRes, shadowRes, EPixelFormat::D24_UNORM_S8_UINT, ShadowMapRenderer::kMaxCascadeCount
    });
    // RenderScene이 매 프레임 실어온 값을 반영(§260713-CompactLog#4) — SetActiveCascadeCount 자체가
    // 1~kMaxCascadeCount로 clamp하므로 여기서는 그대로 전달.
    shadowMapRenderer.SetActiveCascadeCount(scene.GetActiveCascadeCount());
    shadowMapRenderer.AddPasses(graph, scene, shadowDepthHandle, skinnedMeshes);

    // 4. G-Buffer → RT0/RT1/RT2/Depth (정적 + 스키닝 메시 통합)
    gbufferRenderer.AddPasses(graph, skinnedMeshes);

    // 4.5. SSAO(Raw → Blur) → RT1(Normal)+Depth만 있으면 계산 가능, DeferredLighting 이전에 실행.
    //      1차 구현은 Deferred(불투명) ambient/IBL 항에만 적용(IBL Stage B와 동일한 점진적 확장 패턴).
    ssaoRenderer.AddPasses(graph, gbufferRenderer.GetRT1(), gbufferRenderer.GetDepth(), scene.GetCamera());

    // 5. Deferred Lighting → hdrOut (UAV; sky 픽셀은 depth>=1.0 이므로 보존). Shadow Map SRV + IBL(Stage A/B) + SSAO 포함
    lightingRenderer.AddPasses(graph, gbufferRenderer, shadowDepthHandle,
                                shadowMapRenderer.GetLightViewProj(), shadowMapRenderer.GetSplitDistances(),
                                shadowMapRenderer.GetActiveCascadeCount(), shadowMapRenderer.GetBlendWidth(),
                                shadowRes,
                                irradianceMap, prefilteredMap, brdfLUT, iblEnabled,
                                ssaoRenderer.GetBlurredSSAO(), scene.GetSSAOEnabled(),
                                scene.GetShowCascades());

    // 6. Transparent → hdrOut (Load, shared depth from GBufferPass) — Blit보다 먼저 그려야
    //    Tonemap이 Opaque+Transparent를 모두 포함한 결과에 적용됨 (Phase 1-1). IBL(Stage A/B)도
    //    Deferred와 동일 산출물을 공유.
    forwardRenderer.AddTransparentPasses(graph, output, depth.get(), hdrOut,
                                        irradianceMap, prefilteredMap, brdfLUT, iblEnabled);

    // 7. Bloom → hdrOut에 가산 블렌딩 (BrightPass → BlurH → BlurV → Composite, Phase 1-4)
    bloomRenderer.AddPasses(graph, hdrOut, output);

    // 8. Blit → backBuffer (hdrOut SRV → backBuffer RTV, Tonemap+Gamma)
    graph.AddPass<DeferredBlitPass>(blitPSO.get(), hdrOut, output, tonemapBuffer.get());

    // 9. Debug Lines → backBuffer (shared depth from GBufferPass)
    forwardRenderer.AddDebugLinePasses(graph, output, depth.get());

    // 10. Debug Text → backBuffer (최상단)
    forwardRenderer.AddDebugTextPasses(graph, output);
}
