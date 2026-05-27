#include "DeferredLightingRenderer.h"
#include "GBufferRenderer.h"

#include "Render/RenderGraph.h"
#include "Render/IRenderPass.h"
#include "Render/ILight.h"
#include "Render/IMaterial.h"
#include "Scene/RenderScene.h"

#include "RHI/IRenderDevice.h"
#include "RHI/ICommandList.h"
#include "RHI/Pipeline/PipelineDesc.h"
#include "RHI/Pipeline/ComputePipelineDesc.h"
#include "RHI/Common_RHI.h"
#include "Math/Container.h"

#include <algorithm>
#include <cstring>
#include <cstdint>
#include <vector>

// ── GPU 상수 버퍼 레이아웃 ────────────────────────────────────────────────────

// b0 — §12.6 레이아웃 (96 bytes)
struct CB_DeferredCamera
{
    float    invViewProj[16];
    float    eyePos[3];
    float    _pad;
    uint32_t screenW;
    uint32_t screenH;
    float    _pad2[2];
};

// b2 — Lighting.hlsli CB_Lights 와 1:1 대응
static constexpr uint32_t MAX_DL_POINT_LIGHTS = 8;
static constexpr uint32_t MAX_DL_SPOT_LIGHTS  = 4;

struct DL_LightsBufferData
{
    DirectionalLightData dirLight;
    PointLightData       pointLights[MAX_DL_POINT_LIGHTS];
    SpotLightData        spotLights[MAX_DL_SPOT_LIGHTS];
    MaterialData         material;
    float                eyePosW[3];
    int32_t              numPointLights;
    int32_t              numSpotLights;
    float                _lightPad[3];
};

// ── RenderPass 정의 ────────────────────────────────────────────────────────────

namespace {

struct DeferredLightingPass : IRenderPass
{
    DeferredLightingPass(DeferredLightingRenderer* renderer, GBufferRenderer* gbuffer)
        : renderer(renderer), gbuffer(gbuffer) {}

    void OnCompile(RenderGraph& /*graph*/) override {}

    void DeclareResources(std::vector<PassResourceDecl>& reads,
                          std::vector<PassResourceDecl>& writes) const override
    {
        reads.push_back({ gbuffer->GetRT0(),                  EResourceState::NonPixelShaderResource });
        reads.push_back({ gbuffer->GetRT1(),                  EResourceState::NonPixelShaderResource });
        reads.push_back({ gbuffer->GetRT2(),                  EResourceState::NonPixelShaderResource });
        reads.push_back({ gbuffer->GetDepth()->GetResource(), EResourceState::NonPixelShaderResource });
        writes.push_back({ gbuffer->GetHdrOut(),              EResourceState::UnorderedAccess        });
    }

    void Execute(ICommandList* cmdList, const RenderScene& scene) override
    {
        renderer->Dispatch(cmdList, *gbuffer, scene);
    }

    DeferredLightingRenderer* renderer;
    GBufferRenderer*          gbuffer;
};

} // namespace

// ── Initialize ─────────────────────────────────────────────────────────────────

void DeferredLightingRenderer::Initialize(IRenderDevice* device, const ShaderDesc& csDesc,
                                          uint32_t w, uint32_t h)
{
    width  = w;
    height = h;

    // Compute 바인딩 레이아웃
    // root 0: b0 (CB_DeferredCamera)
    // root 1: b2 (CB_Lights)
    // root 2~5: t0~t3 (G-Buffer SRV × 4)
    // root 6: u0 (outHDR UAV, RWTexture2D)
    BindingEntry bindings[] = {
        { EBindingType::ConstantBuffer, 0, EShaderStage::Compute },
        { EBindingType::ConstantBuffer, 2, EShaderStage::Compute },
        { EBindingType::Texture,        0, EShaderStage::Compute },
        { EBindingType::Texture,        1, EShaderStage::Compute },
        { EBindingType::Texture,        2, EShaderStage::Compute },
        { EBindingType::Texture,        3, EShaderStage::Compute },
        { EBindingType::TextureUAV,     0, EShaderStage::Compute },
    };
    computeBindingLayout = device->CreateBindingLayout(bindings, 7, /*isCompute*/ true);

    computeShader = device->CreateShader(csDesc);

    ComputePipelineStateDesc csPsoDesc{};
    csPsoDesc.shader        = computeShader.get();
    csPsoDesc.bindingLayout = computeBindingLayout.get();
    computePipelineState = device->CreateComputePipelineState(csPsoDesc);

    cameraBuffer = device->CreateBuffer({
        .size   = sizeof(CB_DeferredCamera),
        .usage  = EBufferUsage::ConstantBuffer,
        .access = EMemoryAccess::Upload,
        .stride = 0
    });

    lightsBuffer = device->CreateBuffer({
        .size   = sizeof(DL_LightsBufferData),
        .usage  = EBufferUsage::ConstantBuffer,
        .access = EMemoryAccess::Upload,
        .stride = 0
    });
}

// ── AddPasses ──────────────────────────────────────────────────────────────────

void DeferredLightingRenderer::AddPasses(RenderGraph& graph, GBufferRenderer& gbuffer)
{
    graph.AddPass<DeferredLightingPass>(this, &gbuffer);
}

// ── Dispatch ───────────────────────────────────────────────────────────────────

void DeferredLightingRenderer::Dispatch(ICommandList* cmdList, GBufferRenderer& gbuffer,
                                        const RenderScene& scene)
{
    const CameraData& cam = scene.GetCamera();

    // CB_DeferredCamera 업로드
    Matrix4x4 vp    = cam.view * cam.proj;
    Matrix4x4 invVP = vp.Inverse();

    CB_DeferredCamera camData = {};
    std::memcpy(camData.invViewProj, invVP.m, sizeof(camData.invViewProj));
    camData.eyePos[0] = cam.eyePos[0];
    camData.eyePos[1] = cam.eyePos[1];
    camData.eyePos[2] = cam.eyePos[2];
    camData.screenW   = width;
    camData.screenH   = height;
    cameraBuffer->Upload(&camData, sizeof(camData));

    // CB_Lights 업로드
    const LightingState& lighting = scene.GetLighting();
    DL_LightsBufferData ldata = {};
    ldata.dirLight = lighting.dirLight;
    ldata.numPointLights = static_cast<int32_t>(
        std::min(lighting.pointLights.size(), static_cast<size_t>(MAX_DL_POINT_LIGHTS)));
    for (int32_t i = 0; i < ldata.numPointLights; ++i)
        ldata.pointLights[i] = lighting.pointLights[i];
    ldata.numSpotLights = static_cast<int32_t>(
        std::min(lighting.spotLights.size(), static_cast<size_t>(MAX_DL_SPOT_LIGHTS)));
    for (int32_t i = 0; i < ldata.numSpotLights; ++i)
        ldata.spotLights[i] = lighting.spotLights[i];
    ldata.eyePosW[0] = cam.eyePos[0];
    ldata.eyePosW[1] = cam.eyePos[1];
    ldata.eyePosW[2] = cam.eyePos[2];
    lightsBuffer->Upload(&ldata, sizeof(ldata));

    // Compute pass 실행
    computePipelineState->Bind(cmdList);
    cmdList->SetComputeConstantBuffer(cameraBuffer.get(), 0);    // root 0 → b0
    cmdList->SetComputeConstantBuffer(lightsBuffer.get(), 1);    // root 1 → b2

    gbuffer.GetRT0()->BindSRV(cmdList, 2, true);                 // root 2 → t0
    gbuffer.GetRT1()->BindSRV(cmdList, 3, true);                 // root 3 → t1
    gbuffer.GetRT2()->BindSRV(cmdList, 4, true);                 // root 4 → t2
    gbuffer.GetDepth()->BindSRV(cmdList, 5, true);               // root 5 → t3
    gbuffer.GetHdrOut()->BindUAV(cmdList, 6, true);              // root 6 → u0

    const uint32_t groupX = (width  + 7) / 8;
    const uint32_t groupY = (height + 7) / 8;
    cmdList->Dispatch(groupX, groupY, 1);
}
