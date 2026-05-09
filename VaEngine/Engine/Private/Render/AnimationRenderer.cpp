#include "Render/AnimationRenderer.h"
#include "Render/RenderGraph.h"
#include "Render/Material.h"
#include "Scene/RenderScene.h"

#include "RHI/IRenderDevice.h"
#include "RHI/ICommandList.h"
#include "RHI/IRHIResource.h"
#include "RHI/IDepthBuffer.h"
#include "RHI/Pipeline/PipelineDesc.h"
#include "RHI/Common_RHI.h"
#include "RHI/Texture/ITexture.h"
#include "RHI/Texture/ITexture2DArray.h"
#include "Mesh/SkinnedMesh.h"
#include "Math/Container.h"

#include <algorithm>
#include <cstring>

// ── GPU 상수 버퍼 레이아웃 ────────────────────────────────────────────────

struct ViewProjData
{
    float viewProj[16];
};

static constexpr uint32_t MAX_POINT_LIGHTS_ANIM = 8;
static constexpr uint32_t MAX_SPOT_LIGHTS_ANIM  = 4;

struct LightsBufferData_Anim
{
    DirectionalLightData dirLight;
    PointLightData       pointLights[MAX_POINT_LIGHTS_ANIM];
    SpotLightData        spotLights[MAX_SPOT_LIGHTS_ANIM];
    MaterialData         material;
    float                eyePosW[3];
    int32_t              numPointLights;
    int32_t              numSpotLights;
    float                _lightPad[3];
};

// AnimationDemo.hlsl CB_TweenFrame 와 1:1 대응
struct AnimFrameData_GPU
{
    int32_t  clip;
    uint32_t curFrame;
    uint32_t nextFrame;
    float    time;
};

struct TweenFrameData_GPU
{
    float            tweenTime;
    float            _pad[3];
    AnimFrameData_GPU current;
    AnimFrameData_GPU next;
};

namespace {

struct AnimationPass : IRenderPass
{
    AnimationPass(AnimationRenderer* renderer, const FrameOutput& output, uint32_t depthHandle)
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
        passDesc.renderTargets[0].loadAction  = ELoadAction::Load;   // ForwardRenderer 결과 유지
        passDesc.renderTargets[0].storeAction = EStoreAction::Store;

        passDesc.depthStencil.view            = depthBuffer->GetView();
        passDesc.depthStencil.loadAction      = ELoadAction::Load;   // 깊이 테스트 결과 유지
        passDesc.depthStencil.storeAction     = EStoreAction::DontCare;

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

    AnimationRenderer* renderer;
    FrameOutput        output;
    uint32_t           depthHandle;
    IDepthBuffer*      depthBuffer = nullptr;
};

} // namespace

void AnimationRenderer::Initialize(IRenderDevice* device, const ShaderDesc& shaderDesc)
{
    // root param 0: b0 Vertex, 1: b2 Pixel, 2: b3 Vertex, 3: t0 Pixel, 4: t1 Vertex
    BindingEntry bindings[] = {
        { EBindingType::ConstantBuffer, 0, EShaderStage::Vertex },
        { EBindingType::ConstantBuffer, 2, EShaderStage::Pixel  },
        { EBindingType::ConstantBuffer, 3, EShaderStage::Vertex },
        { EBindingType::Texture,        0, EShaderStage::Pixel  },
        { EBindingType::Texture,        1, EShaderStage::Vertex },
    };
    bindingLayout = device->CreateBindingLayout(bindings, 5);

    shader = device->CreateShader(shaderDesc);

    // slot 0: SkinnedVertex (per-vertex, stride 80)
    // slot 1: world matrix rows (per-instance, stride 64)
    VertexInputDesc inputs[] = {
        { "POSITION",          0, EPixelFormat::R32G32B32_FLOAT,    0,  0, false },
        { "NORMAL",            0, EPixelFormat::R32G32B32_FLOAT,    12, 0, false },
        { "COLOR",             0, EPixelFormat::R32G32B32A32_FLOAT, 24, 0, false },
        { "TEXCOORD",          0, EPixelFormat::R32G32_FLOAT,       40, 0, false },
        { "BONEINDEX",         0, EPixelFormat::R32G32B32A32_UINT,  48, 0, false },
        { "BONEWEIGHT",        0, EPixelFormat::R32G32B32A32_FLOAT, 64, 0, false },
        { "INSTANCETRANSFORM", 0, EPixelFormat::R32G32B32A32_FLOAT, 0,  1, true  },
        { "INSTANCETRANSFORM", 1, EPixelFormat::R32G32B32A32_FLOAT, 16, 1, true  },
        { "INSTANCETRANSFORM", 2, EPixelFormat::R32G32B32A32_FLOAT, 32, 1, true  },
        { "INSTANCETRANSFORM", 3, EPixelFormat::R32G32B32A32_FLOAT, 48, 1, true  },
    };
    PipelineStateDesc psoDesc = {
        .shader           = shader.get(),
        .vertexInputs     = inputs,
        .vertexInputCount = 10,
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

    lightsBuffer = device->CreateBuffer({
        .size   = sizeof(LightsBufferData_Anim),
        .usage  = EBufferUsage::ConstantBuffer,
        .access = EMemoryAccess::Upload,
        .stride = 0
    });

    // 사전 할당 — SkinnedRenderCommand.tweenBuffer가 없을 때 폴백용
    tweenFrameBuffer = device->CreateBuffer({
        .size   = MAX_INSTANCES * sizeof(TweenFrameData_GPU),
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

    material = std::make_unique<Material>();
}

void AnimationRenderer::AddPasses(RenderGraph& graph, const FrameOutput& output)
{
    // ForwardRenderer 와 동일한 desc → 같은 핸들 반환 (depth 버퍼 재사용)
    uint32_t depthHandle = graph.DeclareTransientDepth({
        output.width, output.height, EPixelFormat::D24_UNORM_S8_UINT
    });
    graph.AddPass<AnimationPass>(this, output, depthHandle);
}

void AnimationRenderer::Render(ICommandList* cmdList, const RenderScene& scene)
{
    const auto& cmds = scene.GetSkinnedCommands();
    if (cmds.empty())
        return;

    const CameraData& cam = scene.GetCamera();

    // ViewProj 업로드 (root param 0 → b0)
    {
        Matrix4x4 vp = cam.view * cam.proj;
        ViewProjData vpdata = {};
        std::memcpy(vpdata.viewProj, vp.m, sizeof(vpdata.viewProj));
        viewProjBuffer->Upload(&vpdata, sizeof(vpdata));
    }

    // Lights + 재질 업로드 (root param 1 → b2)
    {
        const LightingState& lighting = scene.GetLighting();
        LightsBufferData_Anim ldata = {};
        ldata.dirLight       = lighting.dirLight;
        ldata.numPointLights = static_cast<int32_t>(
            std::min(lighting.pointLights.size(), static_cast<size_t>(MAX_POINT_LIGHTS_ANIM)));
        for (int32_t i = 0; i < ldata.numPointLights; ++i)
            ldata.pointLights[i] = lighting.pointLights[i];
        ldata.numSpotLights = static_cast<int32_t>(
            std::min(lighting.spotLights.size(), static_cast<size_t>(MAX_SPOT_LIGHTS_ANIM)));
        for (int32_t i = 0; i < ldata.numSpotLights; ++i)
            ldata.spotLights[i] = lighting.spotLights[i];
        ldata.material   = material->GetData();
        ldata.eyePosW[0] = cam.eyePos[0];
        ldata.eyePosW[1] = cam.eyePos[1];
        ldata.eyePosW[2] = cam.eyePos[2];
        lightsBuffer->Upload(&ldata, sizeof(ldata));
    }

    pipelineState->Bind(cmdList);
    cmdList->SetConstantBuffer(viewProjBuffer.get(), 0);
    cmdList->SetConstantBuffer(lightsBuffer.get(), 1);
    cmdList->SetPrimitiveTopology(EPrimitiveTopology::TriangleList);

    for (const SkinnedRenderCommand& cmd : cmds)
    {
        if (!cmd.mesh || !cmd.tweenBuffer || !cmd.transformsMap)
            continue;

        const uint32_t count = std::min(cmd.instanceCount, MAX_INSTANCES);

        // TweenFrame 바인딩 (root param 2 → b3) — WorldAnimatedModel이 매 프레임 업로드
        cmdList->SetConstantBuffer(cmd.tweenBuffer, 2);

        // Diffuse 텍스처 바인딩 (root param 3 → t0)
        if (cmd.texture)
            cmd.texture->Bind(cmdList, 3);

        // 본 변환 배열 바인딩 (root param 4 → t1)
        cmd.transformsMap->Bind(cmdList, 4);

        // 인스턴스 월드 행렬 업로드 (slot 1 vertex buffer)
        instanceBuffer->Upload(&cmd.worldMatrix, count * sizeof(Matrix4x4));
        cmdList->SetVertexBufferAt(instanceBuffer.get(), 1,
                                   static_cast<uint32_t>(sizeof(Matrix4x4)),
                                   count * static_cast<uint32_t>(sizeof(Matrix4x4)),
                                   0);

        cmd.mesh->DrawInstanced(cmdList, count);
    }
}
