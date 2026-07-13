#include "BloomRenderer.h"

#include "Render/RenderGraph.h"
#include "Render/IRenderPass.h"

#include "RHI/IRenderDevice.h"
#include "RHI/ICommandList.h"
#include "RHI/Texture/ITextureUAV.h"
#include "RHI/Pipeline/PipelineDesc.h"
#include "RHI/Common_RHI.h"

#include <cstring>
#include <vector>

// ── GPU 상수 버퍼 레이아웃 ────────────────────────────────────────────────────

struct CB_BloomThreshold { float threshold; float pad[3]; };
struct CB_BlurTexel      { float texelSize[2]; float pad[2]; };
struct CB_BloomIntensity { float intensity; float pad[3]; };

// ── RenderPass 정의 ────────────────────────────────────────────────────────────

namespace {

struct BloomBrightPass : IRenderPass
{
    BloomBrightPass(BloomRenderer* renderer, ITextureUAV* hdrOut, IColorBuffer* target, uint32_t w, uint32_t h)
        : renderer(renderer), hdrOut(hdrOut), target(target), w(w), h(h) {}

    void DeclareResources(std::vector<PassResourceDecl>& reads,
                          std::vector<PassResourceDecl>& writes) const override
    {
        reads.push_back({ hdrOut, EResourceState::PixelShaderResource });
        writes.push_back({ target, EResourceState::RenderTarget });
    }

    void Execute(ICommandList* cmdList, const RenderScene& /*scene*/) override
    {
        RenderPassDesc passDesc;
        passDesc.renderTargetCount            = 1;
        passDesc.renderTargets[0].view        = target->GetRTV();
        passDesc.renderTargets[0].loadAction  = ELoadAction::DontCare;
        passDesc.renderTargets[0].storeAction = EStoreAction::Store;

        cmdList->BeginRenderPass(passDesc);
        cmdList->SetViewport(0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h), 0.0f, 1.0f);
        cmdList->SetScissorRect(0, 0, static_cast<int32_t>(w), static_cast<int32_t>(h));
        renderer->RenderBrightPass(cmdList, hdrOut);
        cmdList->EndRenderPass();
    }

    BloomRenderer* renderer;
    ITextureUAV*   hdrOut;
    IColorBuffer*  target;
    uint32_t       w, h;
};

struct BloomBlurPass : IRenderPass
{
    BloomBlurPass(BloomRenderer* renderer, bool horizontal, IColorBuffer* src, IColorBuffer* dst, uint32_t w, uint32_t h)
        : renderer(renderer), horizontal(horizontal), src(src), dst(dst), w(w), h(h) {}

    void DeclareResources(std::vector<PassResourceDecl>& reads,
                          std::vector<PassResourceDecl>& writes) const override
    {
        reads.push_back({ src, EResourceState::PixelShaderResource });
        writes.push_back({ dst, EResourceState::RenderTarget });
    }

    void Execute(ICommandList* cmdList, const RenderScene& /*scene*/) override
    {
        RenderPassDesc passDesc;
        passDesc.renderTargetCount            = 1;
        passDesc.renderTargets[0].view        = dst->GetRTV();
        passDesc.renderTargets[0].loadAction  = ELoadAction::DontCare;
        passDesc.renderTargets[0].storeAction = EStoreAction::Store;

        cmdList->BeginRenderPass(passDesc);
        cmdList->SetViewport(0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h), 0.0f, 1.0f);
        cmdList->SetScissorRect(0, 0, static_cast<int32_t>(w), static_cast<int32_t>(h));
        if (horizontal) renderer->RenderBlurH(cmdList);
        else            renderer->RenderBlurV(cmdList);
        cmdList->EndRenderPass();
    }

    BloomRenderer* renderer;
    bool           horizontal;
    IColorBuffer*  src;
    IColorBuffer*  dst;
    uint32_t       w, h;
};

struct BloomCompositePass : IRenderPass
{
    BloomCompositePass(BloomRenderer* renderer, IColorBuffer* bloomSrc, ITextureUAV* hdrOut, const FrameOutput& output)
        : renderer(renderer), bloomSrc(bloomSrc), hdrOut(hdrOut), output(output) {}

    void DeclareResources(std::vector<PassResourceDecl>& reads,
                          std::vector<PassResourceDecl>& writes) const override
    {
        reads.push_back({ bloomSrc, EResourceState::PixelShaderResource });
        writes.push_back({ hdrOut,  EResourceState::RenderTarget });
    }

    void Execute(ICommandList* cmdList, const RenderScene& /*scene*/) override
    {
        RenderPassDesc passDesc;
        passDesc.renderTargetCount            = 1;
        passDesc.renderTargets[0].view        = hdrOut->GetRTV();
        passDesc.renderTargets[0].loadAction  = ELoadAction::Load;  // 기존 hdrOut 위에 가산
        passDesc.renderTargets[0].storeAction = EStoreAction::Store;

        cmdList->BeginRenderPass(passDesc);
        cmdList->SetViewport(0.0f, 0.0f,
                             static_cast<float>(output.width),
                             static_cast<float>(output.height),
                             0.0f, 1.0f);
        cmdList->SetScissorRect(0, 0,
                                static_cast<int32_t>(output.width),
                                static_cast<int32_t>(output.height));
        renderer->RenderComposite(cmdList);
        cmdList->EndRenderPass();
    }

    BloomRenderer* renderer;
    IColorBuffer*  bloomSrc;
    ITextureUAV*   hdrOut;
    FrameOutput    output;
};

} // namespace

// ── Initialize ─────────────────────────────────────────────────────────────────

void BloomRenderer::Initialize(IRenderDevice* device,
                               const ShaderDesc& brightDesc, const ShaderDesc& blurHDesc,
                               const ShaderDesc& blurVDesc,  const ShaderDesc& compositeDesc,
                               uint32_t fullWidth, uint32_t fullHeight)
{
    halfWidth  = fullWidth  / 2;
    halfHeight = fullHeight / 2;

    // 4개 PSO 공용 바인딩 레이아웃 — root0: t0(Texture), root1: b0(ConstantBuffer)
    BindingEntry bindings[] = {
        { EBindingType::Texture,        0, EShaderStage::Pixel },
        { EBindingType::ConstantBuffer, 0, EShaderStage::Pixel },
    };
    bindingLayout = device->CreateBindingLayout(bindings, 2);

    auto makeFullscreenPSO = [&](IShader* shader, EBlendMode blend) -> std::unique_ptr<IPipelineState>
    {
        PipelineStateDesc desc{};
        desc.shader           = shader;
        desc.vertexInputs     = nullptr;
        desc.vertexInputCount = 0;
        desc.rtvFormats[0]    = EPixelFormat::R16G16B16A16_FLOAT;
        desc.rtvCount         = 1;
        desc.depthEnable      = false;
        desc.blendMode        = blend;
        desc.bindingLayout    = bindingLayout.get();
        return device->CreatePipelineState(desc);
    };

    brightShader = device->CreateShader(brightDesc);
    brightPSO    = makeFullscreenPSO(brightShader.get(), EBlendMode::Opaque);

    blurHShader = device->CreateShader(blurHDesc);
    blurHPSO    = makeFullscreenPSO(blurHShader.get(), EBlendMode::Opaque);

    blurVShader = device->CreateShader(blurVDesc);
    blurVPSO    = makeFullscreenPSO(blurVShader.get(), EBlendMode::Opaque);

    compositeShader = device->CreateShader(compositeDesc);
    compositePSO    = makeFullscreenPSO(compositeShader.get(), EBlendMode::Additive);

    thresholdBuffer = device->CreateBuffer({
        .size = sizeof(CB_BloomThreshold), .usage = EBufferUsage::ConstantBuffer,
        .access = EMemoryAccess::Upload, .stride = 0
    });
    const CB_BloomThreshold thresholdData{ 1.0f, {} };
    thresholdBuffer->Upload(&thresholdData, sizeof(thresholdData));

    texelSizeBuffer = device->CreateBuffer({
        .size = sizeof(CB_BlurTexel), .usage = EBufferUsage::ConstantBuffer,
        .access = EMemoryAccess::Upload, .stride = 0
    });
    const CB_BlurTexel texelData{ { 1.0f / static_cast<float>(halfWidth), 1.0f / static_cast<float>(halfHeight) }, {} };
    texelSizeBuffer->Upload(&texelData, sizeof(texelData));

    intensityBuffer = device->CreateBuffer({
        .size = sizeof(CB_BloomIntensity), .usage = EBufferUsage::ConstantBuffer,
        .access = EMemoryAccess::Upload, .stride = 0
    });
    const CB_BloomIntensity intensityData{ 0.6f, {} };
    intensityBuffer->Upload(&intensityData, sizeof(intensityData));

    bloomA = device->CreateColorBuffer(EPixelFormat::R16G16B16A16_FLOAT, halfWidth, halfHeight);
    bloomB = device->CreateColorBuffer(EPixelFormat::R16G16B16A16_FLOAT, halfWidth, halfHeight);
}

// ── AddPasses ──────────────────────────────────────────────────────────────────

void BloomRenderer::AddPasses(RenderGraph& graph, ITextureUAV* hdrOut, const FrameOutput& output)
{
    // 별도 등록 불필요 — bloomA/bloomB(BaseRHIResource)가 자기 상태를 스스로 들고 있다.
    graph.AddPass<BloomBrightPass>(this, hdrOut, bloomA.get(), halfWidth, halfHeight);
    graph.AddPass<BloomBlurPass>(this, /*horizontal*/ true,  bloomA.get(), bloomB.get(), halfWidth, halfHeight);
    graph.AddPass<BloomBlurPass>(this, /*horizontal*/ false, bloomB.get(), bloomA.get(), halfWidth, halfHeight);
    graph.AddPass<BloomCompositePass>(this, bloomA.get(), hdrOut, output);
}

// ── Render* ────────────────────────────────────────────────────────────────────

void BloomRenderer::RenderBrightPass(ICommandList* cmdList, ITextureUAV* hdrOut)
{
    brightPSO->Bind(cmdList);
    hdrOut->BindSRV(cmdList, 0, /*isCompute*/ false);        // root 0 → t0
    cmdList->SetConstantBuffer(thresholdBuffer.get(), 1);    // root 1 → b0
    cmdList->DrawInstanced(3, 1);
}

void BloomRenderer::RenderBlurH(ICommandList* cmdList)
{
    blurHPSO->Bind(cmdList);
    bloomA->BindSRV(cmdList, 0, /*isCompute*/ false);        // root 0 → t0
    cmdList->SetConstantBuffer(texelSizeBuffer.get(), 1);    // root 1 → b0
    cmdList->DrawInstanced(3, 1);
}

void BloomRenderer::RenderBlurV(ICommandList* cmdList)
{
    blurVPSO->Bind(cmdList);
    bloomB->BindSRV(cmdList, 0, /*isCompute*/ false);        // root 0 → t0
    cmdList->SetConstantBuffer(texelSizeBuffer.get(), 1);    // root 1 → b0
    cmdList->DrawInstanced(3, 1);
}

void BloomRenderer::RenderComposite(ICommandList* cmdList)
{
    compositePSO->Bind(cmdList);
    bloomA->BindSRV(cmdList, 0, /*isCompute*/ false);        // root 0 → t0
    cmdList->SetConstantBuffer(intensityBuffer.get(), 1);    // root 1 → b0
    cmdList->DrawInstanced(3, 1);
}
