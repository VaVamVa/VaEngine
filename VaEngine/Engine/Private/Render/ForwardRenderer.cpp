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
#include "RHI/Buffer/IDepthBuffer.h"
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
    DirectionalLightData dirLight;                        //   32 bytes
    PointLightData       pointLights[MAX_POINT_LIGHTS];  //  384 bytes (48 * 8)
    SpotLightData        spotLights[MAX_SPOT_LIGHTS];    //  256 bytes (64 * 4)
    float                eyePosW[3];                     //   12 bytes
    int32_t              numPointLights;                 //    4 bytes
    int32_t              numSpotLights;                  //    4 bytes
    float                _lightPad[3];                   //   12 bytes
    // total: 704 bytes → 768 (CBV 256-aligned)
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

struct TransparentPass : IRenderPass
{
    TransparentPass(ForwardRenderer* renderer, const FrameOutput& output, IDepthBuffer* sharedDepth)
        : renderer(renderer), output(output), depthBuffer(sharedDepth) {}

    void OnCompile(RenderGraph& /*graph*/) override {}

    void DeclareResources(std::vector<PassResourceDecl>& reads,
                          std::vector<PassResourceDecl>& writes) const override
    {
        writes.push_back({ output.backBuffer,          EResourceState::RenderTarget });
        reads.push_back({ depthBuffer->GetResource(),  EResourceState::DepthRead    });
    }

    void Execute(ICommandList* cmdList, const RenderScene& scene) override
    {
        RenderPassDesc passDesc;
        passDesc.renderTargetCount            = 1;
        passDesc.renderTargets[0].view        = output.backBufferView;
        passDesc.renderTargets[0].loadAction  = ELoadAction::Load;
        passDesc.renderTargets[0].storeAction = EStoreAction::Store;

        passDesc.depthStencil.view            = depthBuffer->GetReadOnlyView();
        passDesc.depthStencil.loadAction      = ELoadAction::Load;
        passDesc.depthStencil.storeAction     = EStoreAction::Store;

        cmdList->BeginRenderPass(passDesc);
        cmdList->SetViewport(0.0f, 0.0f,
                             static_cast<float>(output.width),
                             static_cast<float>(output.height),
                             0.0f, 1.0f);
        cmdList->SetScissorRect(0, 0,
                                static_cast<int32_t>(output.width),
                                static_cast<int32_t>(output.height));

        renderer->RenderTransparent(cmdList, scene);

        cmdList->EndRenderPass();
    }

    ForwardRenderer* renderer;
    FrameOutput      output;
    IDepthBuffer*    depthBuffer = nullptr;
};

struct DebugLinePass : IRenderPass
{
    DebugLinePass(ForwardRenderer* renderer, const FrameOutput& output, IDepthBuffer* sharedDepth)
        : renderer(renderer), output(output), depthBuffer(sharedDepth) {}

    void OnCompile(RenderGraph& /*graph*/) override {}

    void DeclareResources(std::vector<PassResourceDecl>& reads,
                          std::vector<PassResourceDecl>& writes) const override
    {
        writes.push_back({ output.backBuffer,          EResourceState::RenderTarget });
        reads.push_back({ depthBuffer->GetResource(),  EResourceState::DepthRead    });
    }

    void Execute(ICommandList* cmdList, const RenderScene& scene) override
    {
        RenderPassDesc passDesc;
        passDesc.renderTargetCount            = 1;
        passDesc.renderTargets[0].view        = output.backBufferView;
        passDesc.renderTargets[0].loadAction  = ELoadAction::Load;
        passDesc.renderTargets[0].storeAction = EStoreAction::Store;

        passDesc.depthStencil.view        = depthBuffer->GetReadOnlyView();
        passDesc.depthStencil.loadAction  = ELoadAction::Load;
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
    // b0: ViewProj(Vertex), b1: Material(Pixel), b2: Lights(Pixel), t0: albedo(Pixel)
    BindingEntry bindings[] = {
        { EBindingType::ConstantBuffer, 0, EShaderStage::Vertex },
        { EBindingType::ConstantBuffer, 1, EShaderStage::Pixel  },
        { EBindingType::ConstantBuffer, 2, EShaderStage::Pixel  },
        { EBindingType::Texture,        0, EShaderStage::Pixel  },
    };
    bindingLayout = device->CreateBindingLayout(bindings, 4);

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
        .rtvFormats       = { EPixelFormat::R8G8B8A8_UNORM },
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
    material->Initialize(device);
}

void ForwardRenderer::AddOpaquePasses(RenderGraph& graph, const FrameOutput& output, const RenderScene& /*scene*/)
{
    graph.AddPass<SkyPass>(this, output);
    uint32_t depthHandle = graph.DeclareTransientDepth({
        output.width, output.height, EPixelFormat::D24_UNORM_S8_UINT
    });
    graph.AddPass<ForwardPass>(this, output, depthHandle);
}

void ForwardRenderer::AddTransparentPasses(RenderGraph& graph, const FrameOutput& output, IDepthBuffer* sharedDepth)
{
    graph.AddPass<TransparentPass>(this, output, sharedDepth);
}

void ForwardRenderer::AddDebugTextPasses(RenderGraph& graph, const FrameOutput& output)
{
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
        .rtvFormats       = { EPixelFormat::R16G16B16A16_FLOAT },
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

void ForwardRenderer::InitializeTransparent(IRenderDevice* device, const ShaderDesc& shaderDesc)
{
    transparentShader = device->CreateShader(shaderDesc);

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
    PipelineStateDesc baseDesc = {
        .shader           = transparentShader.get(),
        .vertexInputs     = inputs,
        .vertexInputCount = 8,
        .rtvFormats       = { EPixelFormat::R8G8B8A8_UNORM },
        .dsvFormat        = EPixelFormat::D24_UNORM_S8_UINT,
        .blendMode        = EBlendMode::AlphaBlend,
        .depthEnable      = true,
        .depthWrite       = false,
        .bindingLayout    = bindingLayout.get()
    };

    // Pass 1: CullMode::Front — 카메라 반대편 면(뒷면) 렌더링
    PipelineStateDesc backFaceDesc = baseDesc;
    backFaceDesc.cullMode = ECullMode::Front;
    transparentBackFacePSO = device->CreatePipelineState(backFaceDesc);

    // Pass 2: CullMode::Back — 카메라를 향한 면(앞면) 렌더링
    PipelineStateDesc frontFaceDesc = baseDesc;
    frontFaceDesc.cullMode = ECullMode::Back;
    transparentFrontFacePSO = device->CreatePipelineState(frontFaceDesc);
}

void ForwardRenderer::ResolveOIT(ICommandList* /*cmdList*/, const RenderScene& /*scene*/)
{
    // [미구현] Weighted Blended OIT: AccumBuffer Pre-Pass + Composite Pass
    // [미구현] A-Buffer: LinkedList UAV per pixel + depth sort + resolve
}

void ForwardRenderer::Render(ICommandList* cmdList, const RenderScene& scene)
{
    const CameraData& cam = scene.GetCamera();

    ViewProjData vpdata = {};
    std::memcpy(vpdata.viewProj, (cam.view * cam.proj).m, sizeof(vpdata.viewProj));
    viewProjBuffer->Upload(&vpdata, sizeof(vpdata));

    // 조명 데이터 구성 후 프레임당 1회 업로드
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
    ldata.eyePosW[0] = cam.eyePos[0];
    ldata.eyePosW[1] = cam.eyePos[1];
    ldata.eyePosW[2] = cam.eyePos[2];
    lightsBuffer->Upload(&ldata, sizeof(ldata));

    cmdList->SetPrimitiveTopology(EPrimitiveTopology::TriangleList);

    const auto& commands = scene.GetCommands();
    if (commands.empty())
        return;

    // (mesh, material) 쌍으로 그룹화 — 재질 변경 시에만 rebind
    struct DrawGroup { IMesh* mesh; IMaterial* mat; uint32_t count; };
    std::vector<DrawGroup> drawList;
    std::vector<Matrix4x4> allInstances;
    allInstances.reserve(commands.size());

    for (const RenderCommand& cmd : commands)
    {
        if (!cmd.mesh) continue;
        if (cmd.sortKey & (1ULL << 59)) continue;  // translucent → RenderTransparent 처리

        if (!drawList.empty()
            && drawList.back().mesh == cmd.mesh
            && drawList.back().mat  == cmd.material)
        {
            ++drawList.back().count;
        }
        else
        {
            drawList.push_back({ cmd.mesh, cmd.material, 1 });
        }
        allInstances.push_back(cmd.worldMatrix);
    }

    if (drawList.empty())
        return;

    instanceBuffer->Upload(allInstances.data(), allInstances.size() * sizeof(Matrix4x4));

    pipelineState->Bind(cmdList);
    cmdList->SetConstantBuffer(viewProjBuffer.get(), 0);  // root 0 → b0
    cmdList->SetConstantBuffer(lightsBuffer.get(), 2);    // root 2 → b2

    IMaterial* boundMat   = nullptr;
    uint32_t   byteOffset = 0;

    for (auto& [mesh, mat, count] : drawList)
    {
        if (mat != boundMat)
        {
            IMaterial* effMat = mat ? mat : material.get();
            if (effMat) effMat->UpdateBufferIfDirty();
            cmdList->SetConstantBuffer(effMat->GetBuffer(), 1);  // root 1 → b1

            ITexture* albedo = mat ? mat->GetAlbedoTexture() : nullptr;
            (albedo ? albedo : texture.get())->Bind(cmdList, 3);  // root 3 → t0

            boundMat = mat;
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

void ForwardRenderer::RenderTransparent(ICommandList* cmdList, const RenderScene& scene)
{
    if (!transparentBackFacePSO || !transparentFrontFacePSO) return;

    const CameraData& cam = scene.GetCamera();

    // Deferred 경로에서 Render()가 호출되지 않으므로 항상 업로드
    ViewProjData vpdata = {};
    std::memcpy(vpdata.viewProj, (cam.view * cam.proj).m, sizeof(vpdata.viewProj));
    viewProjBuffer->Upload(&vpdata, sizeof(vpdata));

    // 조명 데이터 구성
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
    ldata.eyePosW[0] = cam.eyePos[0];
    ldata.eyePosW[1] = cam.eyePos[1];
    ldata.eyePosW[2] = cam.eyePos[2];
    lightsBuffer->Upload(&ldata, sizeof(ldata));

    // 투명 커맨드 수집
    const auto& commands = scene.GetCommands();
    struct DrawGroup { IMesh* mesh; IMaterial* mat; uint32_t count; };
    std::vector<DrawGroup> drawList;
    std::vector<Matrix4x4> allInstances;
    allInstances.reserve(commands.size());

    for (const RenderCommand& cmd : commands)
    {
        if (!cmd.mesh) continue;
        if (!(cmd.sortKey & (1ULL << 59))) continue;  // opaque → Render() 처리

        if (!drawList.empty()
            && drawList.back().mesh == cmd.mesh
            && drawList.back().mat  == cmd.material)
            ++drawList.back().count;
        else
            drawList.push_back({ cmd.mesh, cmd.material, 1 });
        allInstances.push_back(cmd.worldMatrix);
    }

    if (drawList.empty()) return;

    instanceBuffer->Upload(allInstances.data(), allInstances.size() * sizeof(Matrix4x4));
    cmdList->SetPrimitiveTopology(EPrimitiveTopology::TriangleList);

    auto drawTransparentPass = [&](IPipelineState* pso)
    {
        pso->Bind(cmdList);
        cmdList->SetConstantBuffer(viewProjBuffer.get(), 0);  // root 0 → b0
        cmdList->SetConstantBuffer(lightsBuffer.get(), 2);    // root 2 → b2

        IMaterial* boundMat = nullptr;
        uint32_t   byteOffset = 0;

        for (auto& [mesh, mat, count] : drawList)
        {
            if (mat != boundMat)
            {
                IMaterial* effMat = mat ? mat : material.get();
                if (effMat) effMat->UpdateBufferIfDirty();
                cmdList->SetConstantBuffer(effMat->GetBuffer(), 1);  // root 1 → b1
                ITexture* albedo = mat ? mat->GetAlbedoTexture() : nullptr;
                (albedo ? albedo : texture.get())->Bind(cmdList, 3);  // root 3 → t0
                boundMat = mat;
            }
            uint32_t n = std::min(count, MAX_INSTANCES);
            cmdList->SetVertexBufferAt(instanceBuffer.get(), 1,
                                       static_cast<uint32_t>(sizeof(Matrix4x4)),
                                       n * static_cast<uint32_t>(sizeof(Matrix4x4)),
                                       byteOffset);
            mesh->DrawInstanced(cmdList, n);
            byteOffset += n * static_cast<uint32_t>(sizeof(Matrix4x4));
        }
    };

    // === Pass 1: 뒷면 (CullMode::Front — 카메라 반대편 면) ===
    drawTransparentPass(transparentBackFacePSO.get());

    // [OIT 확장 지점]
    // 비볼록 메시 / 오브젝트 간 교차: 2-Pass 뒷면→앞면 순서만으로 정렬 오류 발생.
    // 해결: Weighted Blended OIT (AccumBuffer Pre-Pass + Composite) 또는 A-Buffer.
    // ResolveOIT(cmdList, scene);

    // === Pass 2: 앞면 (CullMode::Back — 카메라를 향한 면) ===
    drawTransparentPass(transparentFrontFacePSO.get());
}

void ForwardRenderer::AddDebugLinePasses(RenderGraph& graph, const FrameOutput& output, IDepthBuffer* sharedDepth)
{
    if (!debugLineRenderer) return;
    graph.AddPass<DebugLinePass>(this, output, sharedDepth);
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
