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
struct DeferredBlitPass : IRenderPass
{
    DeferredBlitPass(IPipelineState* blitPSO, ITextureUAV* hdrOut, const FrameOutput& output)
        : blitPSO(blitPSO), hdrOut(hdrOut), output(output) {}

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
        passDesc.renderTargets[0].loadAction  = ELoadAction::Clear;
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
        hdrOut->BindSRV(cmdList, 0, /*isCompute*/ false);  // root 0 → t0
        cmdList->DrawInstanced(3, 1);

        cmdList->EndRenderPass();
    }

    IPipelineState* blitPSO;
    ITextureUAV*    hdrOut;
    FrameOutput     output;
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
    BindingEntry blitBindings[] = {
        { EBindingType::Texture, 0, EShaderStage::Pixel }  // t0 → hdrOut SRV
    };
    blitLayout = device->CreateBindingLayout(blitBindings, 1);
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
}

void SceneRenderer::InitializeSky(IRenderDevice* device, const ShaderDesc& shaderDesc)
{
    forwardRenderer.InitializeSky(device, shaderDesc);
}

void SceneRenderer::InitializeForward(IRenderDevice* device, const ShaderDesc& shaderDesc)
{
    forwardRenderer.Initialize(device, shaderDesc);
}

void SceneRenderer::InitializeAnimation(IRenderDevice* device, const ShaderDesc& shaderDesc)
{
    animationRenderer.Initialize(device, shaderDesc);
}

void SceneRenderer::InitializeGBufferSkinned(IRenderDevice* device, const ShaderDesc& shaderDesc)
{
    gbufferRenderer.InitializeSkinned(device, shaderDesc);
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

// ── AddPasses ──────────────────────────────────────────────────────────────────

void SceneRenderer::AddPasses(RenderGraph& graph, const FrameOutput& output, const RenderScene& scene)
{
    ITextureUAV* hdrOut = gbufferRenderer.GetHdrOut();

    // 영속 리소스 초기 상태 등록 (이전 프레임 UpdateResourceStates가 기록한 최종 상태)
    graph.ImportResource(gbufferRenderer.GetRT0(),  rt0State);
    graph.ImportResource(gbufferRenderer.GetRT1(),  rt1State);
    graph.ImportResource(gbufferRenderer.GetRT2(),  rt2State);
    graph.ImportResource(hdrOut,                    hdrState);
    graph.ImportResource(depth->GetResource(),      depthState);

    // 1. Sky → hdrOut (RenderTarget, Clear)
    graph.AddPass<DeferredSkyPass>(&forwardRenderer, hdrOut, output);

    // 2. Bone Palette Compute (스키닝 mesh 목록 반환)
    auto skinnedMeshes = animationRenderer.AddComputePasses(graph, scene);

    // 3. G-Buffer → RT0/RT1/RT2/Depth (정적 + 스키닝 메시 통합)
    gbufferRenderer.AddPasses(graph, skinnedMeshes);

    // 4. Deferred Lighting → hdrOut (UAV; sky 픽셀은 depth>=1.0 이므로 보존)
    lightingRenderer.AddPasses(graph, gbufferRenderer);

    // 5. Blit → backBuffer (hdrOut SRV → backBuffer RTV, Clear)
    graph.AddPass<DeferredBlitPass>(blitPSO.get(), hdrOut, output);

    // 6. Transparent → backBuffer (Load)
    forwardRenderer.AddTransparentPasses(graph, output);

    // 7. Debug Lines → backBuffer
    forwardRenderer.AddDebugLinePasses(graph, output);

    // 8. Debug Text → backBuffer (최상단)
    forwardRenderer.AddDebugTextPasses(graph, output);
}

// ── UpdateResourceStates ───────────────────────────────────────────────────────

void SceneRenderer::UpdateResourceStates(const RenderGraph& graph)
{
    rt0State   = graph.GetCurrentState(gbufferRenderer.GetRT0());
    rt1State   = graph.GetCurrentState(gbufferRenderer.GetRT1());
    rt2State   = graph.GetCurrentState(gbufferRenderer.GetRT2());
    hdrState   = graph.GetCurrentState(gbufferRenderer.GetHdrOut());
    depthState = graph.GetCurrentState(depth->GetResource());
}
