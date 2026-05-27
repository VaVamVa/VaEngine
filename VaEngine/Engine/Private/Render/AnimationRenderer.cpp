#include "Render/AnimationRenderer.h"
#include "Render/RenderGraph.h"
#include "Render/Material.h"
#include "Scene/RenderScene.h"

#include "RHI/IRenderDevice.h"
#include "RHI/ICommandList.h"
#include "RHI/IRHIResource.h"
#include "RHI/Buffer/IDepthBuffer.h"
#include "RHI/Pipeline/PipelineDesc.h"
#include "RHI/Pipeline/ComputePipelineDesc.h"
#include "RHI/Common_RHI.h"
#include "RHI/Texture/ITexture.h"
#include "RHI/Texture/ITexture2DArray.h"
#include "Mesh/SkinnedMesh.h"
#include "Math/Container.h"

#include <algorithm>
#include <cstring>
#include <unordered_set>
#include <vector>

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

namespace {

struct BonePaletteComputePass : IRenderPass
{
    BonePaletteComputePass(AnimationRenderer* renderer, std::vector<SkinnedMesh*> meshes)
        : renderer(renderer), meshes(std::move(meshes)) {}

    void DeclareResources(std::vector<PassResourceDecl>& /*reads*/,
                          std::vector<PassResourceDecl>& writes) const override
    {
        // 이번 프레임 등장한 모든 mesh의 BonePalette를 UAV write로 등록
        for (auto* m : meshes)
            writes.push_back({ m->GetBonePaletteBuffer(), EResourceState::UnorderedAccess });
    }

    void Execute(ICommandList* cmdList, const RenderScene& scene) override
    {
        renderer->RenderCompute(cmdList, scene);
    }

    AnimationRenderer*        renderer;
    std::vector<SkinnedMesh*> meshes;
};

struct AnimationPass : IRenderPass
{
    AnimationPass(AnimationRenderer* renderer, const FrameOutput& output, uint32_t depthHandle,
                  std::vector<SkinnedMesh*> meshes)
        : renderer(renderer), output(output), depthHandle(depthHandle), meshes(std::move(meshes)) {}

    void OnCompile(RenderGraph& graph) override
    {
        depthBuffer = graph.GetTransientDepth(depthHandle);
    }

    void DeclareResources(std::vector<PassResourceDecl>& reads,
                          std::vector<PassResourceDecl>& writes) const override
    {
        writes.push_back({ output.backBuffer,          EResourceState::RenderTarget });
        writes.push_back({ depthBuffer->GetResource(), EResourceState::DepthWrite   });
        // 각 mesh의 BonePalette: compute UAV write → graphics VS SRV read
        // graph가 자동으로 UnorderedAccess → NonPixelShaderResource barrier 삽입
        for (auto* m : meshes)
            reads.push_back({ m->GetBonePaletteBuffer(), EResourceState::NonPixelShaderResource });
    }

    void Execute(ICommandList* cmdList, const RenderScene& scene) override
    {
        RenderPassDesc passDesc;
        passDesc.renderTargetCount              = 1;
        passDesc.renderTargets[0].view          = output.backBufferView;
        passDesc.renderTargets[0].loadAction    = ELoadAction::Load;   // ForwardRenderer 결과 유지
        passDesc.renderTargets[0].storeAction   = EStoreAction::Store;

        passDesc.depthStencil.view              = depthBuffer->GetView();
        passDesc.depthStencil.loadAction        = ELoadAction::Clear;   // Deferred 적용 이후 Load->Clear
        passDesc.depthStencil.storeAction       = EStoreAction::Store; // TransparentPass가 Load해야 하므로
        passDesc.depthStencil.clearColor[0]     = 0.f;

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

    AnimationRenderer*        renderer;
    FrameOutput               output;
    uint32_t                  depthHandle;
    IDepthBuffer*             depthBuffer = nullptr;
    std::vector<SkinnedMesh*> meshes;
};

} // namespace

void AnimationRenderer::Initialize(IRenderDevice* device, const ShaderDesc& shaderDesc)
{
    // root param 0: b0 Vertex, 1: b2 Pixel, 2: t0 Pixel, 3: t1 Vertex (BonePalette buffer)
    // ※ b3 (CB_TweenFrame), t1 (TransformsMap) 은 compute pass로 이전 — graphics는 본 팔레트만 읽음
    BindingEntry bindings[] = {
        { EBindingType::ConstantBuffer, 0, EShaderStage::Vertex },
        { EBindingType::ConstantBuffer, 2, EShaderStage::Pixel  },
        { EBindingType::Texture,        0, EShaderStage::Pixel  },
        { EBindingType::BufferSRV,      1, EShaderStage::Vertex },
    };
    bindingLayout = device->CreateBindingLayout(bindings, 4);

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

    lightsBuffer = device->CreateBuffer({
        .size   = sizeof(LightsBufferData_Anim),
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

    // ── Compute infrastructure (Step 3) ─────────────────────────────────
    // root param 0: b0 (CB_TweenFrame), 1: t0 (TransformsMap), 2: u0 (BonePalette)
    BindingEntry computeBindings[] = {
        { EBindingType::ConstantBuffer, 0, EShaderStage::Compute },
        { EBindingType::Texture,        0, EShaderStage::Compute },
        { EBindingType::UAV,            0, EShaderStage::Compute },
    };
    computeBindingLayout = device->CreateBindingLayout(computeBindings, 3, /*isCompute*/ true);

    ShaderDesc csDesc{};
    csDesc.csPath  = SHADER_DIR L"/BonePaletteCompute_CS.cso";
    csDesc.csEntry = "CSMain";
    computeShader = device->CreateShader(csDesc);

    ComputePipelineStateDesc csPsoDesc{};
    csPsoDesc.shader        = computeShader.get();
    csPsoDesc.bindingLayout = computeBindingLayout.get();
    computePipelineState = device->CreateComputePipelineState(csPsoDesc);

    // BonePalette buffer는 SkinnedMesh가 소유 (mesh별로 한 buffer를 모든 인스턴스가 공유)
}

std::vector<SkinnedMesh*> AnimationRenderer::AddComputePasses(RenderGraph& graph, const RenderScene& scene)
{
    std::vector<SkinnedMesh*> uniqueMeshes;
    {
        std::unordered_set<SkinnedMesh*> seen;
        for (const RenderCommand& cmd : scene.GetCommands())
        {
            if (cmd.skinnedMesh && seen.insert(cmd.skinnedMesh).second)
                uniqueMeshes.push_back(cmd.skinnedMesh);
        }
    }
    for (auto* m : uniqueMeshes)
        graph.ImportResource(m->GetBonePaletteBuffer(), EResourceState::UnorderedAccess);

    graph.AddPass<BonePaletteComputePass>(this, uniqueMeshes);
    return uniqueMeshes;
}

void AnimationRenderer::AddGraphicsPasses(RenderGraph& graph, const FrameOutput& output,
                                           const std::vector<SkinnedMesh*>& uniqueMeshes)
{
    uint32_t depthHandle = graph.DeclareTransientDepth({
        output.width, output.height, EPixelFormat::D24_UNORM_S8_UINT
    });
    graph.AddPass<AnimationPass>(this, output, depthHandle, uniqueMeshes);
}

void AnimationRenderer::AddPasses(RenderGraph& graph, const FrameOutput& output, const RenderScene& scene)
{
    auto meshes = AddComputePasses(graph, scene);
    AddGraphicsPasses(graph, output, meshes);
}

void AnimationRenderer::Render(ICommandList* cmdList, const RenderScene& scene)
{
    const auto& cmds = scene.GetCommands();
    const bool hasSkinnedCmd = std::any_of(cmds.begin(), cmds.end(),
        [](const RenderCommand& c) { return c.skinnedMesh != nullptr; });
    if (!hasSkinnedCmd)
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
    cmdList->SetConstantBuffer(viewProjBuffer.get(), 0);  // b0
    cmdList->SetConstantBuffer(lightsBuffer.get(),   1);  // b2
    cmdList->SetPrimitiveTopology(EPrimitiveTopology::TriangleList);

    for (const RenderCommand& cmd : cmds)
    {
        if (!cmd.skinnedMesh)
            continue;

        const uint32_t count = std::min(cmd.instanceCount, cmd.skinnedMesh->GetMaxInstances());

        // Diffuse 텍스처 바인딩 (root param 2 → t0)
        if (cmd.texture)
            cmd.texture->Bind(cmdList, 2);

        // BonePalette SRV — mesh별 buffer (root param 3 → t1)
        cmdList->SetGraphicsSRV(cmd.skinnedMesh->GetBonePaletteSRV(), 3);

        // 인스턴스 월드 행렬 업로드 (slot 1 vertex buffer)
        instanceBuffer->Upload(&cmd.worldMatrix, count * sizeof(Matrix4x4));
        cmdList->SetVertexBufferAt(instanceBuffer.get(), 1,
                                   static_cast<uint32_t>(sizeof(Matrix4x4)),
                                   count * static_cast<uint32_t>(sizeof(Matrix4x4)),
                                   0);

        cmd.skinnedMesh->DrawInstanced(cmdList, count);
    }
}

void AnimationRenderer::RenderCompute(ICommandList* cmdList, const RenderScene& scene)
{
    const auto& cmds = scene.GetCommands();
    if (cmds.empty()) return;

    computePipelineState->Bind(cmdList);  // SetComputeRootSignature + SetPipelineState

    for (const RenderCommand& cmd : cmds)
    {
        if (!cmd.skinnedMesh || !cmd.tweenBuffer || !cmd.transformsMap)
            continue;

        const uint32_t count = std::min(cmd.instanceCount, cmd.skinnedMesh->GetMaxInstances());

        // root param 0: b0 ← TweenFrame
        cmdList->SetComputeConstantBuffer(cmd.tweenBuffer, 0);
        // root param 1: t0 ← TransformsMap (descriptor table, compute mode)
        cmd.transformsMap->Bind(cmdList, 1, /*isCompute*/ true);
        // root param 2: u0 ← mesh의 BonePalette UAV (root descriptor)
        cmdList->SetComputeUAV(cmd.skinnedMesh->GetBonePaletteUAV(), 2);

        // [numthreads(MAX_BONES, 1, 1)] × Dispatch(1, count, 1)
        cmdList->Dispatch(1, count, 1);
    }
}
