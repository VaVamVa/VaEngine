#include "Render/ForwardRenderer.h"
#include "Render/RenderGraph.h"
#include "Render/Material.h"
#include "Scene/RenderScene.h"

#include "RHI/IRenderDevice.h"
#include "RHI/ICommandList.h"
#include "RHI/IRHIResource.h"
#include "RHI/Pipeline/PipelineDesc.h"
#include "RHI/Common_RHI.h"
#include "Math/Container.h"

#include "Mesh/IMesh.h"
#include "RHI/Texture/ITexture.h"
#include "RHI/IDepthBuffer.h"
#include "Utilities/DebuggingHelper.h"

#include <algorithm>
#include <cstring>
#include <utility>
#include <vector>
#include <cstdint>

// ── GPU 상수 버퍼 레이아웃 ────────────────────────────────────────────────

// b0 — 프레임별 ViewProj (64 bytes, 256 정렬)
struct ViewProjData
{
    float viewProj[16];
};

static constexpr uint32_t MAX_INSTANCES = 1024;

// b2 — 조명 + 재질 — Lighting.hlsli CB_Lights 와 1:1 대응
static constexpr uint32_t MAX_POINT_LIGHTS = 8;
static constexpr uint32_t MAX_SPOT_LIGHTS  = 4;

struct LightsBufferData
{
    DirectionalLightData dirLight;                        //   64 bytes
    PointLightData       pointLights[MAX_POINT_LIGHTS];  //  640 bytes
    SpotLightData        spotLights[MAX_SPOT_LIGHTS];    //  384 bytes
    MaterialData         material;                       //   64 bytes
    float                eyePosW[3];                     //   12 bytes
    int32_t              numPointLights;                 //    4 bytes
    int32_t              numSpotLights;                  //    4 bytes
    float                _lightPad[3];                   //   12 bytes
    // total: 1184 bytes → 1280 (CBV 256-aligned)
};

namespace {

struct SkyPass : IRenderPass
{
    SkyPass(ForwardRenderer* renderer, const FrameOutput& output)
        : renderer(renderer), output(output) {}

    void OnCompile(RenderGraph& /*graph*/) override {}

    void DeclareResources(std::vector<PassResourceDecl>& /*reads*/,
                          std::vector<PassResourceDecl>& writes) const override
    {
        writes.push_back({ output.backBuffer, EResourceState::RenderTarget });
    }

    void Execute(ICommandList* cmdList, const RenderScene& scene) override
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

        renderer->RenderSky(cmdList, scene);

        cmdList->EndRenderPass();
    }

    ForwardRenderer* renderer;
    FrameOutput      output;
};

struct ForwardPass : IRenderPass
{
    ForwardPass(ForwardRenderer* renderer, const FrameOutput& output, uint32_t depthHandle)
        : renderer(renderer), output(output), depthHandle(depthHandle) {}

    void OnCompile(RenderGraph& graph) override
    {
        depthBuffer = graph.GetTransientDepth(depthHandle);
    }

    void DeclareResources(std::vector<PassResourceDecl>& /*reads*/,
                          std::vector<PassResourceDecl>& writes) const override
    {
        writes.push_back({ output.backBuffer,          EResourceState::RenderTarget });
        writes.push_back({ depthBuffer->GetResource(), EResourceState::DepthWrite   });
    }

    void Execute(ICommandList* cmdList, const RenderScene& scene) override
    {
        RenderPassDesc passDesc;
        passDesc.renderTargetCount            = 1;
        passDesc.renderTargets[0].view        = output.backBufferView;
        passDesc.renderTargets[0].loadAction  = ELoadAction::Load;   // SkyPass가 Clear 담당
        passDesc.renderTargets[0].storeAction = EStoreAction::Store;

        passDesc.depthStencil.view            = depthBuffer->GetView();
        passDesc.depthStencil.loadAction      = ELoadAction::Clear;
        passDesc.depthStencil.storeAction     = EStoreAction::Store;   // DebugLinePass에서 depth test 재사용
        passDesc.depthStencil.clearColor[0]   = 1.0f;

        cmdList->BeginRenderPass(passDesc);
        cmdList->SetViewport(0.0f, 0.0f,
                             static_cast<float>(output.width),
                             static_cast<float>(output.height),
                             0.0f, 1.0f);
        cmdList->SetScissorRect(0, 0,
                                static_cast<int32_t>(output.width),
                                static_cast<int32_t>(output.height));

        renderer->Render(cmdList, scene);

        cmdList->EndRenderPass();
    }

    ForwardRenderer* renderer;
    FrameOutput      output;
    uint32_t         depthHandle;
    IDepthBuffer*    depthBuffer = nullptr;  // graph 소유, 비소유 포인터
};

struct DebugLinePass : IRenderPass
{
    DebugLinePass(ForwardRenderer* renderer, const FrameOutput& output, uint32_t depthHandle)
        : renderer(renderer), output(output), depthHandle(depthHandle) {}

    void OnCompile(RenderGraph& graph) override
    {
        depthBuffer = graph.GetTransientDepth(depthHandle);
    }

    void DeclareResources(std::vector<PassResourceDecl>& /*reads*/,
                          std::vector<PassResourceDecl>& writes) const override
    {
        writes.push_back({ output.backBuffer,          EResourceState::RenderTarget });
        writes.push_back({ depthBuffer->GetResource(), EResourceState::DepthWrite   });
    }

    void Execute(ICommandList* cmdList, const RenderScene& scene) override
    {
        RenderPassDesc passDesc;
        passDesc.renderTargetCount            = 1;
        passDesc.renderTargets[0].view        = output.backBufferView;
        passDesc.renderTargets[0].loadAction  = ELoadAction::Load;
        passDesc.renderTargets[0].storeAction = EStoreAction::Store;

        passDesc.depthStencil.view        = depthBuffer->GetView();
        passDesc.depthStencil.loadAction  = ELoadAction::Load;      // ForwardPass depth 유지
        passDesc.depthStencil.storeAction = EStoreAction::DontCare;

        cmdList->BeginRenderPass(passDesc);
        cmdList->SetViewport(0.0f, 0.0f,
                             static_cast<float>(output.width),
                             static_cast<float>(output.height),
                             0.0f, 1.0f);
        cmdList->SetScissorRect(0, 0,
                                static_cast<int32_t>(output.width),
                                static_cast<int32_t>(output.height));

        renderer->RenderDebugLines(cmdList, scene);

        cmdList->EndRenderPass();
    }

    ForwardRenderer* renderer;
    FrameOutput      output;
    uint32_t         depthHandle;
    IDepthBuffer*    depthBuffer = nullptr;
};

struct DebugTextPass : IRenderPass
{
    DebugTextPass(ForwardRenderer* renderer, const FrameOutput& output)
        : renderer(renderer), output(output) {}

    void OnCompile(RenderGraph& /*graph*/) override {}

    void DeclareResources(std::vector<PassResourceDecl>& /*reads*/,
                          std::vector<PassResourceDecl>& writes) const override
    {
        writes.push_back({ output.backBuffer, EResourceState::RenderTarget });
    }

    void Execute(ICommandList* cmdList, const RenderScene& scene) override
    {
        RenderPassDesc passDesc;
        passDesc.renderTargetCount            = 1;
        passDesc.renderTargets[0].view        = output.backBufferView;
        passDesc.renderTargets[0].loadAction  = ELoadAction::Load;
        passDesc.renderTargets[0].storeAction = EStoreAction::Store;

        cmdList->BeginRenderPass(passDesc);
        cmdList->SetViewport(0.0f, 0.0f,
                             static_cast<float>(output.width),
                             static_cast<float>(output.height),
                             0.0f, 1.0f);
        cmdList->SetScissorRect(0, 0,
                                static_cast<int32_t>(output.width),
                                static_cast<int32_t>(output.height));

        renderer->RenderDebugText(cmdList, scene, output.width, output.height);

        cmdList->EndRenderPass();
    }

    ForwardRenderer* renderer;
    FrameOutput      output;
};

} // namespace

void ForwardRenderer::Initialize(IRenderDevice* device, const ShaderDesc& shaderDesc)
{
    // b0: transform(Vertex), b2: lights(Pixel), t0: texture(Pixel)
    BindingEntry bindings[] = {
        { EBindingType::ConstantBuffer, 0, EShaderStage::Vertex },
        { EBindingType::ConstantBuffer, 2, EShaderStage::Pixel  },
        { EBindingType::Texture,        0, EShaderStage::Pixel  },
    };
    bindingLayout = device->CreateBindingLayout(bindings, 3);

    shader = device->CreateShader(shaderDesc);

    // slot 0: per-vertex, slot 1: per-instance world matrix rows
    VertexInputDesc inputs[] = {
        { "POSITION",          0, EPixelFormat::R32G32B32_FLOAT,    0,  0, false },
        { "NORMAL",            0, EPixelFormat::R32G32B32_FLOAT,    12, 0, false },
        { "COLOR",             0, EPixelFormat::R32G32B32A32_FLOAT, 24, 0, false },
        { "TEXCOORD",          0, EPixelFormat::R32G32_FLOAT,       40, 0, false },
        { "INSTANCETRANSFORM", 0, EPixelFormat::R32G32B32A32_FLOAT, 0,  1, true  },
        { "INSTANCETRANSFORM", 1, EPixelFormat::R32G32B32A32_FLOAT, 16, 1, true  },
        { "INSTANCETRANSFORM", 2, EPixelFormat::R32G32B32A32_FLOAT, 32, 1, true  },
        { "INSTANCETRANSFORM", 3, EPixelFormat::R32G32B32A32_FLOAT, 48, 1, true  },
    };
    PipelineStateDesc psoDesc = {
        .shader           = shader.get(),
        .vertexInputs     = inputs,
        .vertexInputCount = 8,
        .rtvFormat        = EPixelFormat::R8G8B8A8_UNORM,
        .dsvFormat        = EPixelFormat::D24_UNORM_S8_UINT,
        .depthEnable      = true,
        .bindingLayout    = bindingLayout.get()
    };
    pipelineState = device->CreatePipelineState(psoDesc);

    viewProjBuffer = device->CreateBuffer({
        .size   = sizeof(ViewProjData),
        .usage  = EBufferUsage::ConstantBuffer,
        .access = EMemoryAccess::Upload,
        .stride = 0
    });

    instanceBuffer = device->CreateBuffer({
        .size   = MAX_INSTANCES * sizeof(Matrix4x4),
        .usage  = EBufferUsage::VertexBuffer,
        .access = EMemoryAccess::Upload,
        .stride = sizeof(Matrix4x4)
    });

    lightsBuffer = device->CreateBuffer({
        .size   = sizeof(LightsBufferData),
        .usage  = EBufferUsage::ConstantBuffer,
        .access = EMemoryAccess::Upload,
        .stride = 0
    });

    constexpr uint32_t W = 256, H = 256, TILE = 32;
    std::vector<uint32_t> pixels(W * H);
    for (uint32_t py = 0; py < H; ++py)
        for (uint32_t px = 0; px < W; ++px)
            pixels[py * W + px] = ((px / TILE + py / TILE) % 2) ? 0xFFFFFFFF : 0xFF3F3FBF;
    texture = device->CreateTexture();
    texture->LoadFromMemory(device, pixels.data(), W, H);

    material = std::make_unique<Material>();
}

void ForwardRenderer::AddPasses(RenderGraph& graph, const FrameOutput& output, const RenderScene& /*scene*/)
{
    graph.AddPass<SkyPass>(this, output);  // 항상 먼저 — RT Clear + HDRi 렌더

    uint32_t depthHandle = graph.DeclareTransientDepth({
        output.width, output.height, EPixelFormat::D24_UNORM_S8_UINT
    });
    graph.AddPass<ForwardPass>(this, output, depthHandle);

    if (debugTextRenderer)
        graph.AddPass<DebugTextPass>(this, output);
}

void ForwardRenderer::InitializeSky(IRenderDevice* device, const ShaderDesc& skyShaderDesc)
{
    // 바인딩 레이아웃: b0(CB, Pixel) + t0(Texture, Pixel)
    BindingEntry skyBindings[] = {
        { EBindingType::ConstantBuffer, 0, EShaderStage::Pixel },
        { EBindingType::Texture,        0, EShaderStage::Pixel },
    };
    skyBindingLayout = device->CreateBindingLayout(skyBindings, 2);

    skyShader = device->CreateShader(skyShaderDesc);

    // CullMode::None, depthEnable=false, vertexInput 없음
    PipelineStateDesc skyPsoDesc = {
        .shader           = skyShader.get(),
        .vertexInputs     = nullptr,
        .vertexInputCount = 0,
        .rtvFormat        = EPixelFormat::R8G8B8A8_UNORM,
        .dsvFormat        = EPixelFormat::Unknown,
        .cullMode         = ECullMode::None,
        .blendMode        = EBlendMode::Opaque,
        .depthEnable      = false,
        .bindingLayout    = skyBindingLayout.get()
    };
    skyPipelineState = device->CreatePipelineState(skyPsoDesc);

    // CB_SkyData: InvProj(64) + InvViewRot(64) = 128 bytes
    skyDataBuffer = device->CreateBuffer({
        .size   = 128,
        .usage  = EBufferUsage::ConstantBuffer,
        .access = EMemoryAccess::Upload,
        .stride = 0
    });
}

void ForwardRenderer::RenderSky(ICommandList* cmdList, const RenderScene& scene)
{
    if (!skyPipelineState || !scene.GetSkybox())
        return;

    const CameraData& cam = scene.GetCamera();

    // InvProj: Projection 역행렬
    Matrix4x4 invProj = cam.proj.Inverse();

    // InvViewRot: View translation 제거(row-major m[3][0..2]) 후 전치 (직교 → 역 = 전치)
    Matrix4x4 viewRot  = cam.view;
    viewRot.m[3][0] = 0.0f; viewRot.m[3][1] = 0.0f; viewRot.m[3][2] = 0.0f;
    Matrix4x4 invViewRot = viewRot.Transposed();

    struct SkyData { float invProj[16]; float invViewRot[16]; };
    SkyData skyData;
    std::memcpy(skyData.invProj,    invProj.m,    64);
    std::memcpy(skyData.invViewRot, invViewRot.m, 64);
    skyDataBuffer->Upload(&skyData, sizeof(skyData));

    skyPipelineState->Bind(cmdList);
    cmdList->SetConstantBuffer(skyDataBuffer.get(), 0);  // root param 0 → b0
    scene.GetSkybox()->Bind(cmdList, 1);                  // root param 1 → t0
    cmdList->SetPrimitiveTopology(EPrimitiveTopology::TriangleList);
    cmdList->DrawInstanced(3, 1);
}

void ForwardRenderer::Render(ICommandList* cmdList, const RenderScene& scene)
{
    const CameraData& cam = scene.GetCamera();

    // ── 프레임 당 1회: viewProj 업로드 (root param 0 → b0) ──────────────
    {
        Matrix4x4 vp = cam.view * cam.proj;
        ViewProjData vpdata = {};
        std::memcpy(vpdata.viewProj, vp.m, sizeof(vpdata.viewProj));
        viewProjBuffer->Upload(&vpdata, sizeof(vpdata));
    }

    // ── 프레임 당 1회: 조명 + 재질 업로드 (root param 1 → b2) ────────────
    {
        const LightingState& lighting = scene.GetLighting();
        LightsBufferData ldata = {};
        ldata.dirLight = lighting.dirLight;
        ldata.numPointLights = static_cast<int32_t>(
            std::min(lighting.pointLights.size(), static_cast<size_t>(MAX_POINT_LIGHTS)));
        for (int32_t i = 0; i < ldata.numPointLights; ++i)
            ldata.pointLights[i] = lighting.pointLights[i];
        ldata.numSpotLights = static_cast<int32_t>(
            std::min(lighting.spotLights.size(), static_cast<size_t>(MAX_SPOT_LIGHTS)));
        for (int32_t i = 0; i < ldata.numSpotLights; ++i)
            ldata.spotLights[i] = lighting.spotLights[i];
        ldata.material   = material->GetData();
        ldata.eyePosW[0] = cam.eyePos[0];
        ldata.eyePosW[1] = cam.eyePos[1];
        ldata.eyePosW[2] = cam.eyePos[2];
        lightsBuffer->Upload(&ldata, sizeof(ldata));
    }

    pipelineState->Bind(cmdList);
    cmdList->SetConstantBuffer(viewProjBuffer.get(), 0);  // root param 0 → b0
    cmdList->SetConstantBuffer(lightsBuffer.get(), 1);    // root param 1 → b2
    cmdList->SetPrimitiveTopology(EPrimitiveTopology::TriangleList);

    const auto& commands = scene.GetCommands();
    if (commands.empty())
        return;

    // ── (mesh, texture) 쌍으로 그룹화 → 텍스처 변경 시에만 rebind ─────────
    struct DrawGroup { IMesh* mesh; ITexture* tex; uint32_t count; };
    std::vector<DrawGroup> drawList;
    std::vector<Matrix4x4> allInstances;
    allInstances.reserve(commands.size());

    for (const RenderCommand& cmd : commands)
    {
        ITexture* tex = cmd.texture ? cmd.texture : texture.get();
        if (!drawList.empty()
            && drawList.back().mesh == cmd.mesh
            && drawList.back().tex  == tex)
        {
            ++drawList.back().count;
        }
        else
        {
            drawList.push_back({ cmd.mesh, tex, 1 });
        }
        allInstances.push_back(cmd.worldMatrix);
    }

    instanceBuffer->Upload(allInstances.data(), allInstances.size() * sizeof(Matrix4x4));

    ITexture* boundTex  = nullptr;
    uint32_t  byteOffset = 0;
    for (auto& [mesh, tex, count] : drawList)
    {
        if (tex != boundTex)
        {
            tex->Bind(cmdList, 2);  // root param 2 → t0
            boundTex = tex;
        }
        uint32_t clampedCount = std::min(count, MAX_INSTANCES);
        cmdList->SetVertexBufferAt(instanceBuffer.get(), 1,
                                   static_cast<uint32_t>(sizeof(Matrix4x4)),
                                   clampedCount * static_cast<uint32_t>(sizeof(Matrix4x4)),
                                   byteOffset);
        mesh->DrawInstanced(cmdList, clampedCount);
        byteOffset += clampedCount * static_cast<uint32_t>(sizeof(Matrix4x4));
    }
}

void ForwardRenderer::AddDebugLinePasses(RenderGraph& graph, const FrameOutput& output)
{
    if (!debugLineRenderer) return;
    // DeclareTransientDepth — 동일 desc면 기존 핸들 반환 (AnimationPass와 같은 depth buffer 공유)
    uint32_t depthHandle = graph.DeclareTransientDepth({
        output.width, output.height, EPixelFormat::D24_UNORM_S8_UINT
    });
    graph.AddPass<DebugLinePass>(this, output, depthHandle);
}

void ForwardRenderer::InitializeDebugLines(IRenderDevice* device, const ShaderDesc& lineShaderDesc)
{
    renderDevice = device;
    debugLineRenderer = std::make_unique<DebugLineRenderer>();
    debugLineRenderer->Initialize(device, lineShaderDesc);
}

void ForwardRenderer::RenderDebugLines(ICommandList* cmdList, const RenderScene& scene)
{
    if (!debugLineRenderer) return;
    const CameraData& cam = scene.GetCamera();
    const Matrix4x4 vp = cam.view * cam.proj;
    debugLineRenderer->Render(cmdList, DebuggingHelper::GetLineEntries(), vp);
}

void ForwardRenderer::InitializeDebugText(IRenderDevice* device, const ShaderDesc& glyphShaderDesc, const char* ttfPath)
{
    renderDevice = device;
    debugTextRenderer = std::make_unique<DebugTextRenderer>();
    debugTextRenderer->Initialize(device, glyphShaderDesc, ttfPath);
}

void ForwardRenderer::RenderDebugText(ICommandList* cmdList, const RenderScene& /*scene*/,
                                      uint32_t screenW, uint32_t screenH)
{
    if (!debugTextRenderer) return;
    debugTextRenderer->Render(cmdList, renderDevice,
                              DebuggingHelper::GetTextEntries(),
                              DebuggingHelper::GetPanelEntries(),
                              screenW, screenH);
}
