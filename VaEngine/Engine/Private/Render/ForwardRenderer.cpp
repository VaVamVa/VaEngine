#include "Render/ForwardRenderer.h"
#include "Render/RenderGraph.h"
#include "Render/Material.h"
#include "Scene/RenderScene.h"

#include "RHI/IRenderDevice.h"
#include "RHI/ICommandList.h"
#include "RHI/BaseRHIResource.h"
#include "RHI/Pipeline/PipelineDesc.h"
#include "RHI/Common_RHI.h"
#include "Math/Container.h"

#include "Mesh/IMesh.h"
#include "RHI/Texture/ITexture.h"
#include "RHI/Texture/ITextureUAV.h"
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
    uint32_t              iblEnabled;                     //    4 bytes — CB_Lights.gIBLEnabled
    float                _lightPad[2];                   //    8 bytes
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
        writes.push_back({ output.backBuffer, EResourceState::RenderTarget });
        writes.push_back({ depthBuffer,       EResourceState::DepthWrite   });
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

// Weighted Blended OIT — 1-Pass로 accum/revealage MRT에 기록(정렬 불필요, 2026-07-12_Q&A.md 참조)
struct OITAccumPass : IRenderPass
{
    OITAccumPass(ForwardRenderer* renderer, const FrameOutput& output, IDepthBuffer* sharedDepth,
                IColorBuffer* accum, IColorBuffer* revealage,
                ITextureUAV* irradianceMap, ITextureUAV* prefilteredMap, ITextureUAV* brdfLUT,
                bool iblEnabled)
        : renderer(renderer), output(output), depthBuffer(sharedDepth), accum(accum), revealage(revealage),
          irradianceMap(irradianceMap), prefilteredMap(prefilteredMap), brdfLUT(brdfLUT),
          iblEnabled(iblEnabled) {}

    void OnCompile(RenderGraph& /*graph*/) override {}

    void DeclareResources(std::vector<PassResourceDecl>& reads,
                          std::vector<PassResourceDecl>& writes) const override
    {
        writes.push_back({ accum,         EResourceState::RenderTarget });
        writes.push_back({ revealage,     EResourceState::RenderTarget });
        reads.push_back({ depthBuffer,    EResourceState::DepthRead    });
        reads.push_back({ irradianceMap,  EResourceState::PixelShaderResource });
        reads.push_back({ prefilteredMap, EResourceState::PixelShaderResource });
        reads.push_back({ brdfLUT,        EResourceState::PixelShaderResource });
    }

    void Execute(ICommandList* cmdList, const RenderScene& scene) override
    {
        RenderPassDesc passDesc;
        passDesc.renderTargetCount            = 2;
        passDesc.renderTargets[0].view        = accum->GetRTV();
        passDesc.renderTargets[0].loadAction  = ELoadAction::Clear;   // (0,0,0,0)
        passDesc.renderTargets[0].storeAction = EStoreAction::Store;

        passDesc.renderTargets[1].view        = revealage->GetRTV();
        passDesc.renderTargets[1].loadAction  = ELoadAction::Clear;
        passDesc.renderTargets[1].storeAction = EStoreAction::Store;
        // revealage는 1.0에서 시작해 곱셈으로 수렴 — R32_FLOAT라 실제로는 R 채널만 쓰이지만,
        // D3D12 검증 레이어가 생성 시점 optimized clear value(1,1,1,1 4성분)와 클리어 호출 값을
        // 통째로 비교해 불일치 여부를 판단하므로 4성분 전부 맞춰야 CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE
        // 경고가 사라진다(R만 1로 두면 나머지 3개가 기본값 0이라 여전히 불일치로 잡힘).
        passDesc.renderTargets[1].clearColor[0] = 1.0f;
        passDesc.renderTargets[1].clearColor[1] = 1.0f;
        passDesc.renderTargets[1].clearColor[2] = 1.0f;
        passDesc.renderTargets[1].clearColor[3] = 1.0f;

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

        renderer->RenderTransparent(cmdList, scene, irradianceMap, prefilteredMap, brdfLUT, iblEnabled);

        cmdList->EndRenderPass();
    }

    ForwardRenderer* renderer;
    FrameOutput      output;
    IDepthBuffer*    depthBuffer = nullptr;
    IColorBuffer*    accum;
    IColorBuffer*    revealage;
    ITextureUAV*     irradianceMap;
    ITextureUAV*     prefilteredMap;
    ITextureUAV*     brdfLUT;
    bool             iblEnabled;
};

// OIT 합성 — accum/revealage를 풀어 hdrOut에 AlphaBlend (Phase 1-1: hdrOut에 그려 Tonemap이 포함하도록)
struct OITCompositePass : IRenderPass
{
    OITCompositePass(ForwardRenderer* renderer, const FrameOutput& output,
                     IColorBuffer* accum, IColorBuffer* revealage, ITextureUAV* hdrOut)
        : renderer(renderer), output(output), accum(accum), revealage(revealage), hdrOut(hdrOut) {}

    void OnCompile(RenderGraph& /*graph*/) override {}

    void DeclareResources(std::vector<PassResourceDecl>& reads,
                          std::vector<PassResourceDecl>& writes) const override
    {
        reads.push_back({ accum,     EResourceState::PixelShaderResource });
        reads.push_back({ revealage, EResourceState::PixelShaderResource });
        writes.push_back({ hdrOut,   EResourceState::RenderTarget        });
    }

    void Execute(ICommandList* cmdList, const RenderScene& /*scene*/) override
    {
        RenderPassDesc passDesc;
        passDesc.renderTargetCount            = 1;
        passDesc.renderTargets[0].view        = hdrOut->GetRTV();
        passDesc.renderTargets[0].loadAction  = ELoadAction::Load;   // DeferredLightingPass 결과 위에 블렌딩
        passDesc.renderTargets[0].storeAction = EStoreAction::Store;

        cmdList->BeginRenderPass(passDesc);
        cmdList->SetViewport(0.0f, 0.0f,
                             static_cast<float>(output.width),
                             static_cast<float>(output.height),
                             0.0f, 1.0f);
        cmdList->SetScissorRect(0, 0,
                                static_cast<int32_t>(output.width),
                                static_cast<int32_t>(output.height));

        renderer->RenderOITComposite(cmdList);

        cmdList->EndRenderPass();
    }

    ForwardRenderer* renderer;
    FrameOutput      output;
    IColorBuffer*    accum;
    IColorBuffer*    revealage;
    ITextureUAV*     hdrOut = nullptr;
};

struct DebugLinePass : IRenderPass
{
    DebugLinePass(ForwardRenderer* renderer, const FrameOutput& output, IDepthBuffer* sharedDepth)
        : renderer(renderer), output(output), depthBuffer(sharedDepth) {}

    void OnCompile(RenderGraph& /*graph*/) override {}

    void DeclareResources(std::vector<PassResourceDecl>& reads,
                          std::vector<PassResourceDecl>& writes) const override
    {
        writes.push_back({ output.backBuffer, EResourceState::RenderTarget });
        reads.push_back({ depthBuffer,        EResourceState::DepthRead    });
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
    // 새 슬롯은 항상 끝에 append — 기존 root index(0~3) 불변 보장
    BindingEntry bindings[] = {
        { EBindingType::ConstantBuffer, 0, EShaderStage::Vertex },
        { EBindingType::ConstantBuffer, 1, EShaderStage::Pixel  },
        { EBindingType::ConstantBuffer, 2, EShaderStage::Pixel  },
        { EBindingType::Texture,        0, EShaderStage::Pixel  },
        { EBindingType::Texture,        1, EShaderStage::Pixel  },  // root 4 — Normal Map
        { EBindingType::Texture,        2, EShaderStage::Pixel  },  // root 5 — IBL Diffuse Irradiance
        { EBindingType::Texture,        3, EShaderStage::Pixel  },  // root 6 — IBL Specular Prefiltered
        { EBindingType::Texture,        4, EShaderStage::Pixel  },  // root 7 — IBL BRDF LUT
    };
    bindingLayout = device->CreateBindingLayout(bindings, 8);

    shader = device->CreateShader(shaderDesc);

    // slot 0: per-vertex, slot 1: per-instance world matrix rows
    VertexInputDesc inputs[] = {
        { "POSITION",          0, EPixelFormat::R32G32B32_FLOAT,    0,  0, false },
        { "NORMAL",            0, EPixelFormat::R32G32B32_FLOAT,    12, 0, false },
        { "COLOR",             0, EPixelFormat::R32G32B32A32_FLOAT, 24, 0, false },
        { "TEXCOORD",          0, EPixelFormat::R32G32_FLOAT,       40, 0, false },
        { "TANGENT",           0, EPixelFormat::R32G32B32A32_FLOAT, 48, 0, false },
        { "INSTANCETRANSFORM", 0, EPixelFormat::R32G32B32A32_FLOAT, 0,  1, true  },
        { "INSTANCETRANSFORM", 1, EPixelFormat::R32G32B32A32_FLOAT, 16, 1, true  },
        { "INSTANCETRANSFORM", 2, EPixelFormat::R32G32B32A32_FLOAT, 32, 1, true  },
        { "INSTANCETRANSFORM", 3, EPixelFormat::R32G32B32A32_FLOAT, 48, 1, true  },
    };
    PipelineStateDesc psoDesc = {
        .shader           = shader.get(),
        .vertexInputs     = inputs,
        .vertexInputCount = 9,
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

    // normal map 폴백용 1×1 tangent-up 텍스처 — RGB(128,128,255) → decode 시 (0,0,1)에 근접
    constexpr uint32_t flatNormal = 0xFFFF8080;  // A=FF,B=FF,G=80,R=80
    defaultNormalTexture = device->CreateTexture();
    defaultNormalTexture->LoadFromMemory(device, &flatNormal, 1, 1);

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

void ForwardRenderer::AddTransparentPasses(RenderGraph& graph, const FrameOutput& output,
                                            IDepthBuffer* sharedDepth, ITextureUAV* hdrOut,
                                            ITextureUAV* irradianceMap, ITextureUAV* prefilteredMap,
                                            ITextureUAV* brdfLUT, bool iblEnabled)
{
    graph.AddPass<OITAccumPass>(this, output, sharedDepth, oitAccum.get(), oitRevealage.get(),
                                irradianceMap, prefilteredMap, brdfLUT, iblEnabled);
    graph.AddPass<OITCompositePass>(this, output, oitAccum.get(), oitRevealage.get(), hdrOut);
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

void ForwardRenderer::InitializeTransparent(IRenderDevice* device, const ShaderDesc& shaderDesc,
                                            const ShaderDesc& compositeShaderDesc,
                                            uint32_t width, uint32_t height)
{
    transparentShader = device->CreateShader(shaderDesc);

    VertexInputDesc inputs[] = {
        { "POSITION",          0, EPixelFormat::R32G32B32_FLOAT,    0,  0, false },
        { "NORMAL",            0, EPixelFormat::R32G32B32_FLOAT,    12, 0, false },
        { "COLOR",             0, EPixelFormat::R32G32B32A32_FLOAT, 24, 0, false },
        { "TEXCOORD",          0, EPixelFormat::R32G32_FLOAT,       40, 0, false },
        { "TANGENT",           0, EPixelFormat::R32G32B32A32_FLOAT, 48, 0, false },
        { "INSTANCETRANSFORM", 0, EPixelFormat::R32G32B32A32_FLOAT, 0,  1, true  },
        { "INSTANCETRANSFORM", 1, EPixelFormat::R32G32B32A32_FLOAT, 16, 1, true  },
        { "INSTANCETRANSFORM", 2, EPixelFormat::R32G32B32A32_FLOAT, 32, 1, true  },
        { "INSTANCETRANSFORM", 3, EPixelFormat::R32G32B32A32_FLOAT, 48, 1, true  },
    };
    // Weighted Blended OIT — 1-Pass, CullMode::None(정렬이 필요 없어짐), MRT(accum, revealage)
    PipelineStateDesc transparentDesc = {
        .shader           = transparentShader.get(),
        .vertexInputs     = inputs,
        .vertexInputCount = 9,
        .rtvFormats       = { EPixelFormat::R16G16B16A16_FLOAT, EPixelFormat::R32_FLOAT },
        .rtvCount         = 2,
        .dsvFormat        = EPixelFormat::D24_UNORM_S8_UINT,
        .cullMode         = ECullMode::None,
        .blendMode        = EBlendMode::OITAccumulate,
        .depthEnable      = true,
        .depthWrite       = false,
        .bindingLayout    = bindingLayout.get()
    };
    transparentPSO = device->CreatePipelineState(transparentDesc);

    oitAccum = device->CreateColorBuffer(EPixelFormat::R16G16B16A16_FLOAT, width, height);  // (0,0,0,0)로 클리어 — 기본값과 일치

    // revealage는 1.0에서 시작해야 하므로(곱셈 블렌드) optimized clear value를 실제 클리어 값과
    // 맞춰준다 — 안 맞추면 CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE 경고(느린 clear 경로, 정확성엔 무관).
    const float kRevealageClear[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    oitRevealage = device->CreateColorBuffer(EPixelFormat::R32_FLOAT, width, height, kRevealageClear);

    // OIT 합성 PSO — 풀스크린 삼각형, 기존 AlphaBlend 재사용(신규 블렌드 모드 불필요)
    BindingEntry compositeBindings[] = {
        { EBindingType::Texture, 0, EShaderStage::Pixel },  // t0 — accum
        { EBindingType::Texture, 1, EShaderStage::Pixel },  // t1 — revealage
    };
    compositeBindingLayout = device->CreateBindingLayout(compositeBindings, 2);
    compositeShader = device->CreateShader(compositeShaderDesc);

    PipelineStateDesc compositeDesc{};
    compositeDesc.shader           = compositeShader.get();
    compositeDesc.vertexInputs     = nullptr;
    compositeDesc.vertexInputCount = 0;
    compositeDesc.rtvFormats[0]    = EPixelFormat::R16G16B16A16_FLOAT;
    compositeDesc.rtvCount         = 1;
    compositeDesc.depthEnable      = false;
    compositeDesc.blendMode        = EBlendMode::AlphaBlend;
    compositeDesc.bindingLayout    = compositeBindingLayout.get();
    compositePSO = device->CreatePipelineState(compositeDesc);
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

            ITexture* normalMap = mat ? mat->GetNormalTexture() : nullptr;
            (normalMap ? normalMap : defaultNormalTexture.get())->Bind(cmdList, 4);  // root 4 → t1

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

void ForwardRenderer::RenderTransparent(ICommandList* cmdList, const RenderScene& scene,
                                        ITextureUAV* irradianceMap, ITextureUAV* prefilteredMap,
                                        ITextureUAV* brdfLUT, bool iblEnabled)
{
    if (!transparentPSO) return;

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
    ldata.iblEnabled = iblEnabled ? 1u : 0u;
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

    // Weighted Blended OIT — 1-Pass(CullMode::None), 정렬 불필요. accum/revealage MRT에 기록.
    transparentPSO->Bind(cmdList);
    cmdList->SetConstantBuffer(viewProjBuffer.get(), 0);  // root 0 → b0
    cmdList->SetConstantBuffer(lightsBuffer.get(), 2);    // root 2 → b2
    irradianceMap->BindSRV(cmdList, 5, /*isCompute*/ false);   // root 5 → t2
    prefilteredMap->BindSRV(cmdList, 6, /*isCompute*/ false);  // root 6 → t3
    brdfLUT->BindSRV(cmdList, 7, /*isCompute*/ false);         // root 7 → t4

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
            ITexture* normalMap = mat ? mat->GetNormalTexture() : nullptr;
            (normalMap ? normalMap : defaultNormalTexture.get())->Bind(cmdList, 4);  // root 4 → t1
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
}

void ForwardRenderer::RenderOITComposite(ICommandList* cmdList)
{
    compositePSO->Bind(cmdList);
    oitAccum->BindSRV(cmdList, 0, /*isCompute*/ false);      // root 0 → t0
    oitRevealage->BindSRV(cmdList, 1, /*isCompute*/ false);  // root 1 → t1
    cmdList->DrawInstanced(3, 1);
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
