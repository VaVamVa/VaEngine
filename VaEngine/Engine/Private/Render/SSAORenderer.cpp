#include "SSAORenderer.h"

#include "Render/RenderGraph.h"
#include "Render/IRenderPass.h"
#include "Scene/RenderScene.h"

#include "RHI/ICommandList.h"
#include "RHI/Buffer/IDepthBuffer.h"
#include "RHI/Pipeline/PipelineDesc.h"
#include "RHI/Common_RHI.h"
#include "Math/Container.h"

#include <cstring>
#include <cstdint>
#include <random>
#include <vector>

// ── GPU 상수 버퍼 레이아웃 ────────────────────────────────────────────────────

struct CB_SSAOCamera
{
    float    invViewProj[16];
    float    viewProj[16];
    float    eyePos[3];
    float    _pad;
    uint32_t screenW;
    uint32_t screenH;
    float    _pad2[2];
};

struct CB_SSAOParams
{
    float kernel[32][4];
    float radius;
    float bias;
    float power;
    float _pad;
};

struct CB_SSAOBlur { float texelSize[2]; float pad[2]; };

// ── RenderPass 정의 ────────────────────────────────────────────────────────────

namespace {

struct SSAORawPass : IRenderPass
{
    SSAORawPass(SSAORenderer* renderer, IColorBuffer* normalRough, IDepthBuffer* depth,
               IColorBuffer* target, const CameraData& cam, uint32_t w, uint32_t h)
        : renderer(renderer), normalRough(normalRough), depth(depth), target(target),
          cam(cam), w(w), h(h) {}

    void DeclareResources(std::vector<PassResourceDecl>& reads,
                          std::vector<PassResourceDecl>& writes) const override
    {
        reads.push_back({ normalRough, EResourceState::PixelShaderResource });
        reads.push_back({ depth,       EResourceState::PixelShaderResource });
        writes.push_back({ target,     EResourceState::RenderTarget        });
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
        renderer->RenderSSAO(cmdList, normalRough, depth, cam);
        cmdList->EndRenderPass();
    }

    SSAORenderer* renderer;
    IColorBuffer* normalRough;
    IDepthBuffer* depth;
    IColorBuffer* target;
    CameraData    cam;
    uint32_t      w, h;
};

struct SSAOBlurPass : IRenderPass
{
    SSAOBlurPass(SSAORenderer* renderer, IColorBuffer* src, IColorBuffer* dst, uint32_t w, uint32_t h)
        : renderer(renderer), src(src), dst(dst), w(w), h(h) {}

    void DeclareResources(std::vector<PassResourceDecl>& reads,
                          std::vector<PassResourceDecl>& writes) const override
    {
        reads.push_back({ src,  EResourceState::PixelShaderResource });
        writes.push_back({ dst, EResourceState::RenderTarget        });
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
        renderer->RenderBlur(cmdList);
        cmdList->EndRenderPass();
    }

    SSAORenderer* renderer;
    IColorBuffer* src;
    IColorBuffer* dst;
    uint32_t      w, h;
};

} // namespace

// ── Initialize ─────────────────────────────────────────────────────────────────

void SSAORenderer::Initialize(IRenderDevice* device, const ShaderDesc& ssaoDesc, const ShaderDesc& blurDesc,
                              uint32_t fullWidth, uint32_t fullHeight)
{
    width  = fullWidth;
    height = fullHeight;

    // root0: b0(CB_SSAOCamera) root1: b1(CB_SSAOParams) root2~4: t0~t2(normal/depth/noise)
    BindingEntry ssaoBindings[] = {
        { EBindingType::ConstantBuffer, 0, EShaderStage::Pixel },
        { EBindingType::ConstantBuffer, 1, EShaderStage::Pixel },
        { EBindingType::Texture,        0, EShaderStage::Pixel },
        { EBindingType::Texture,        1, EShaderStage::Pixel },
        { EBindingType::Texture,        2, EShaderStage::Pixel },
    };
    ssaoBindingLayout = device->CreateBindingLayout(ssaoBindings, 5);

    ssaoShader = device->CreateShader(ssaoDesc);

    PipelineStateDesc ssaoPsoDesc{};
    ssaoPsoDesc.shader           = ssaoShader.get();
    ssaoPsoDesc.vertexInputs     = nullptr;
    ssaoPsoDesc.vertexInputCount = 0;
    ssaoPsoDesc.rtvFormats[0]    = EPixelFormat::R32_FLOAT;
    ssaoPsoDesc.rtvCount         = 1;
    ssaoPsoDesc.depthEnable      = false;
    ssaoPsoDesc.blendMode        = EBlendMode::Opaque;
    ssaoPsoDesc.bindingLayout    = ssaoBindingLayout.get();
    ssaoPipelineState = device->CreatePipelineState(ssaoPsoDesc);

    cameraBuffer = device->CreateBuffer({
        .size = sizeof(CB_SSAOCamera), .usage = EBufferUsage::ConstantBuffer,
        .access = EMemoryAccess::Upload, .stride = 0
    });

    // root0: b0(CB_SSAOBlur) root1: t0(gSSAO)
    BindingEntry blurBindings[] = {
        { EBindingType::ConstantBuffer, 0, EShaderStage::Pixel },
        { EBindingType::Texture,        0, EShaderStage::Pixel },
    };
    blurBindingLayout = device->CreateBindingLayout(blurBindings, 2);

    blurShader = device->CreateShader(blurDesc);

    PipelineStateDesc blurPsoDesc{};
    blurPsoDesc.shader           = blurShader.get();
    blurPsoDesc.vertexInputs     = nullptr;
    blurPsoDesc.vertexInputCount = 0;
    blurPsoDesc.rtvFormats[0]    = EPixelFormat::R32_FLOAT;
    blurPsoDesc.rtvCount         = 1;
    blurPsoDesc.depthEnable      = false;
    blurPsoDesc.blendMode        = EBlendMode::Opaque;
    blurPsoDesc.bindingLayout    = blurBindingLayout.get();
    blurPipelineState = device->CreatePipelineState(blurPsoDesc);

    blurTexelBuffer = device->CreateBuffer({
        .size = sizeof(CB_SSAOBlur), .usage = EBufferUsage::ConstantBuffer,
        .access = EMemoryAccess::Upload, .stride = 0
    });
    const CB_SSAOBlur texelData{ { 1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height) }, {} };
    blurTexelBuffer->Upload(&texelData, sizeof(texelData));

    ssaoRaw     = device->CreateColorBuffer(EPixelFormat::R32_FLOAT, width, height);
    ssaoBlurred = device->CreateColorBuffer(EPixelFormat::R32_FLOAT, width, height);

    GenerateKernelAndNoise(device);
}

void SSAORenderer::GenerateKernelAndNoise(IRenderDevice* device)
{
    // 고정 시드 — 세션 간 결정론적으로 동일한 커널/노이즈 재현(디버깅 용이).
    std::mt19937 rng(20260712u);
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
    std::uniform_real_distribution<float> distSigned(-1.0f, 1.0f);

    CB_SSAOParams params{};
    params.radius = kDefaultRadius;
    params.bias   = kDefaultBias;
    params.power  = kDefaultPower;

    for (uint32_t i = 0; i < kKernelSize; ++i)
    {
        Vector3 sample{ distSigned(rng), distSigned(rng), dist01(rng) };  // 접선 공간 반구(z>=0)
        sample = sample.Normalized() * dist01(rng);

        // 원점(중심 픽셀) 근처로 샘플을 밀집시키는 가속 보간(LearnOpenGL 관례) —
        // 균등 분포보다 가까운 거리의 차폐를 더 촘촘히 샘플링해 품질 대비 샘플 수를 아낀다.
        float scale = static_cast<float>(i) / static_cast<float>(kKernelSize);
        scale = 0.1f + 0.9f * scale * scale;
        sample *= scale;

        params.kernel[i][0] = sample.x;
        params.kernel[i][1] = sample.y;
        params.kernel[i][2] = sample.z;
        params.kernel[i][3] = 0.0f;
    }

    paramsBuffer = device->CreateBuffer({
        .size = sizeof(CB_SSAOParams), .usage = EBufferUsage::ConstantBuffer,
        .access = EMemoryAccess::Upload, .stride = 0
    });
    paramsBuffer->Upload(&params, sizeof(params));

    // 4x4 랜덤 회전 벡터 타일 — R8G8B8A8_UNORM 팩킹(ITexture::LoadFromMemory 고정 포맷).
    // xy만 사용(접선 평면 회전), zw는 미사용.
    uint32_t noiseData[16];
    for (uint32_t i = 0; i < 16; ++i)
    {
        const uint8_t r = static_cast<uint8_t>((distSigned(rng) * 0.5f + 0.5f) * 255.0f);
        const uint8_t g = static_cast<uint8_t>((distSigned(rng) * 0.5f + 0.5f) * 255.0f);
        noiseData[i] = 0xFF000000u | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(r);
    }
    noiseTexture = device->CreateTexture();
    noiseTexture->LoadFromMemory(device, noiseData, 4, 4);
}

// ── AddPasses ──────────────────────────────────────────────────────────────────

void SSAORenderer::AddPasses(RenderGraph& graph, IColorBuffer* gNormalRough, IDepthBuffer* gDepth,
                             const CameraData& cam)
{
    graph.AddPass<SSAORawPass>(this, gNormalRough, gDepth, ssaoRaw.get(), cam, width, height);
    graph.AddPass<SSAOBlurPass>(this, ssaoRaw.get(), ssaoBlurred.get(), width, height);
}

// ── Render* ────────────────────────────────────────────────────────────────────

void SSAORenderer::RenderSSAO(ICommandList* cmdList, IColorBuffer* gNormalRough, IDepthBuffer* gDepth,
                              const CameraData& cam)
{
    Matrix4x4 vp    = cam.view * cam.proj;
    Matrix4x4 invVP = vp.Inverse();

    CB_SSAOCamera camData{};
    std::memcpy(camData.invViewProj, invVP.m, sizeof(camData.invViewProj));
    std::memcpy(camData.viewProj,    vp.m,    sizeof(camData.viewProj));
    camData.eyePos[0] = cam.eyePos[0];
    camData.eyePos[1] = cam.eyePos[1];
    camData.eyePos[2] = cam.eyePos[2];
    camData.screenW   = width;
    camData.screenH   = height;
    cameraBuffer->Upload(&camData, sizeof(camData));

    ssaoPipelineState->Bind(cmdList);
    cmdList->SetConstantBuffer(cameraBuffer.get(), 0);  // root0 → b0
    cmdList->SetConstantBuffer(paramsBuffer.get(), 1);  // root1 → b1
    gNormalRough->BindSRV(cmdList, 2, /*isCompute*/ false);  // root2 → t0
    gDepth->BindSRV(cmdList, 3, /*isCompute*/ false);        // root3 → t1
    noiseTexture->Bind(cmdList, 4, /*isCompute*/ false);     // root4 → t2
    cmdList->DrawInstanced(3, 1);
}

void SSAORenderer::RenderBlur(ICommandList* cmdList)
{
    blurPipelineState->Bind(cmdList);
    cmdList->SetConstantBuffer(blurTexelBuffer.get(), 0);  // root0 → b0
    ssaoRaw->BindSRV(cmdList, 1, /*isCompute*/ false);      // root1 → t0
    cmdList->DrawInstanced(3, 1);
}
