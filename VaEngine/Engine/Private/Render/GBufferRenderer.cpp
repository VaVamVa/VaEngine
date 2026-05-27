#include "GBufferRenderer.h"

#include "Render/RenderGraph.h"
#include "Render/IRenderPass.h"
#include "Scene/RenderScene.h"

#include "RHI/IRenderDevice.h"
#include "RHI/ICommandList.h"
#include "RHI/Pipeline/PipelineDesc.h"
#include "RHI/Common_RHI.h"
#include "Math/Container.h"
#include "Mesh/IMesh.h"
#include "Mesh/SkinnedMesh.h"

#include <algorithm>
#include <cstring>
#include <vector>
#include <cstdint>

// ── GPU 상수 버퍼 레이아웃 ────────────────────────────────────────────────────

struct GBufferViewProjData { float viewProj[16]; };          // b0 — 64 bytes

struct GBufferMaterialData                                   // b1 — 16 bytes
{
    float roughness;
    float metallic;
    float _pad[2];
};

static constexpr uint32_t MAX_GB_INSTANCES = 1024;

// ── RenderPass 정의 ────────────────────────────────────────────────────────────

namespace {

struct GBufferPass : IRenderPass
{
    GBufferPass(GBufferRenderer* renderer, std::vector<SkinnedMesh*> meshes)
        : renderer(renderer), skinnedMeshes(std::move(meshes)) {}

    void OnCompile(RenderGraph& /*graph*/) override {}

    void DeclareResources(std::vector<PassResourceDecl>& reads,
                          std::vector<PassResourceDecl>& writes) const override
    {
        writes.push_back({ renderer->GetRT0(),                  EResourceState::RenderTarget });
        writes.push_back({ renderer->GetRT1(),                  EResourceState::RenderTarget });
        writes.push_back({ renderer->GetRT2(),                  EResourceState::RenderTarget });
        writes.push_back({ renderer->GetDepth()->GetResource(), EResourceState::DepthWrite   });
        // BonePalette: compute UAV write → VS SRV read barrier 자동 삽입
        for (auto* m : skinnedMeshes)
            reads.push_back({ m->GetBonePaletteBuffer(), EResourceState::NonPixelShaderResource });
    }

    void Execute(ICommandList* cmdList, const RenderScene& scene) override
    {
        RenderPassDesc passDesc;
        passDesc.renderTargetCount = 3;

        passDesc.renderTargets[0].view        = renderer->GetRT0()->GetRTV();
        passDesc.renderTargets[0].loadAction  = ELoadAction::Clear;
        passDesc.renderTargets[0].storeAction = EStoreAction::Store;

        passDesc.renderTargets[1].view        = renderer->GetRT1()->GetRTV();
        passDesc.renderTargets[1].loadAction  = ELoadAction::Clear;
        passDesc.renderTargets[1].storeAction = EStoreAction::Store;

        passDesc.renderTargets[2].view        = renderer->GetRT2()->GetRTV();
        passDesc.renderTargets[2].loadAction  = ELoadAction::Clear;
        passDesc.renderTargets[2].storeAction = EStoreAction::Store;

        passDesc.depthStencil.view            = renderer->GetDepth()->GetView();
        passDesc.depthStencil.loadAction      = ELoadAction::Clear;
        passDesc.depthStencil.storeAction     = EStoreAction::Store;
        passDesc.depthStencil.clearColor[0]   = 1.0f;

        cmdList->BeginRenderPass(passDesc);
        cmdList->SetViewport(0.0f, 0.0f,
                             static_cast<float>(renderer->GetWidth()),
                             static_cast<float>(renderer->GetHeight()),
                             0.0f, 1.0f);
        cmdList->SetScissorRect(0, 0,
                                static_cast<int32_t>(renderer->GetWidth()),
                                static_cast<int32_t>(renderer->GetHeight()));

        renderer->RenderGBuffer(cmdList, scene, skinnedMeshes);

        cmdList->EndRenderPass();
    }

    GBufferRenderer*          renderer;
    std::vector<SkinnedMesh*> skinnedMeshes;
};

} // namespace

// ── Initialize ─────────────────────────────────────────────────────────────────

void GBufferRenderer::Initialize(IRenderDevice* device, IDepthBuffer* sharedDepth,
                                  const ShaderDesc& shaderDesc,
                                  uint32_t w, uint32_t h)
{
    depth  = sharedDepth;
    width  = w;
    height = h;

    // G-Buffer RT 생성
    gbRT0 = device->CreateColorBuffer(EPixelFormat::R8G8B8A8_UNORM,     width, height);
    gbRT1 = device->CreateColorBuffer(EPixelFormat::R16G16B16A16_FLOAT,  width, height);
    gbRT2 = device->CreateColorBuffer(EPixelFormat::R8G8B8A8_UNORM,     width, height);

    hdrOut = device->CreateTextureUAV();
    hdrOut->Create(device, EPixelFormat::R16G16B16A16_FLOAT, width, height);

    // 바인딩 레이아웃 (b0: ViewProj/VS, b1: Material/PS, t0: Diffuse/PS)
    BindingEntry bindings[] = {
        { EBindingType::ConstantBuffer, 0, EShaderStage::Vertex },
        { EBindingType::ConstantBuffer, 1, EShaderStage::Pixel  },
        { EBindingType::Texture,        0, EShaderStage::Pixel  },
    };
    bindingLayout = device->CreateBindingLayout(bindings, 3);

    shader = device->CreateShader(shaderDesc);

    // ForwardOpaque와 동일한 정점 레이아웃 (PSO 공유 가능 구조)
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
        .rtvFormats       = { EPixelFormat::R8G8B8A8_UNORM,
                              EPixelFormat::R16G16B16A16_FLOAT,
                              EPixelFormat::R8G8B8A8_UNORM },
        .rtvCount         = 3,
        .dsvFormat        = EPixelFormat::D24_UNORM_S8_UINT,
        .depthEnable      = true,
        .bindingLayout    = bindingLayout.get()
    };
    pipelineState = device->CreatePipelineState(psoDesc);

    viewProjBuffer = device->CreateBuffer({
        .size   = sizeof(GBufferViewProjData),
        .usage  = EBufferUsage::ConstantBuffer,
        .access = EMemoryAccess::Upload,
        .stride = 0
    });

    materialBuffer = device->CreateBuffer({
        .size   = sizeof(GBufferMaterialData),
        .usage  = EBufferUsage::ConstantBuffer,
        .access = EMemoryAccess::Upload,
        .stride = 0
    });

    instanceBuffer = device->CreateBuffer({
        .size   = MAX_GB_INSTANCES * sizeof(Matrix4x4),
        .usage  = EBufferUsage::VertexBuffer,
        .access = EMemoryAccess::Upload,
        .stride = sizeof(Matrix4x4)
    });

    // albedo 폴백용 1×1 흰색 텍스처
    constexpr uint32_t white = 0xFFFFFFFF;
    defaultTexture = device->CreateTexture();
    defaultTexture->LoadFromMemory(device, &white, 1, 1);
}

// ── InitializeSkinned ──────────────────────────────────────────────────────────

void GBufferRenderer::InitializeSkinned(IRenderDevice* device, const ShaderDesc& shaderDesc)
{
    BindingEntry bindings[] = {
        { EBindingType::ConstantBuffer, 0, EShaderStage::Vertex },  // b0 ViewProj
        { EBindingType::ConstantBuffer, 1, EShaderStage::Pixel  },  // b1 GBufferMaterial
        { EBindingType::Texture,        0, EShaderStage::Pixel  },  // t0 Diffuse
        { EBindingType::BufferSRV,      1, EShaderStage::Vertex },  // t1 BonePalette
    };
    skinnedBindingLayout = device->CreateBindingLayout(bindings, 4);

    skinnedShader = device->CreateShader(shaderDesc);

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
        .shader           = skinnedShader.get(),
        .vertexInputs     = inputs,
        .vertexInputCount = 10,
        .rtvFormats       = { EPixelFormat::R8G8B8A8_UNORM,
                              EPixelFormat::R16G16B16A16_FLOAT,
                              EPixelFormat::R8G8B8A8_UNORM },
        .rtvCount         = 3,
        .dsvFormat        = EPixelFormat::D24_UNORM_S8_UINT,
        .depthEnable      = true,
        .bindingLayout    = skinnedBindingLayout.get()
    };
    skinnedPipelineState = device->CreatePipelineState(psoDesc);

    skinnedInstanceBuffer = device->CreateBuffer({
        .size   = MAX_GB_INSTANCES * sizeof(Matrix4x4),
        .usage  = EBufferUsage::VertexBuffer,
        .access = EMemoryAccess::Upload,
        .stride = sizeof(Matrix4x4)
    });
}

// ── AddPasses ──────────────────────────────────────────────────────────────────

void GBufferRenderer::AddPasses(RenderGraph& graph, const std::vector<SkinnedMesh*>& skinnedMeshes)
{
    graph.AddPass<GBufferPass>(this, skinnedMeshes);
}

// ── RenderGBuffer ──────────────────────────────────────────────────────────────

void GBufferRenderer::RenderGBuffer(ICommandList* cmdList, const RenderScene& scene,
                                     const std::vector<SkinnedMesh*>& skinnedMeshes)
{
    const CameraData& cam = scene.GetCamera();

    // ViewProj 업로드
    Matrix4x4 vp = cam.view * cam.proj;
    GBufferViewProjData vpData;
    std::memcpy(vpData.viewProj, vp.m, sizeof(vpData.viewProj));
    viewProjBuffer->Upload(&vpData, sizeof(vpData));

    // GBufferMaterial 업로드 (기본값 — 향후 per-mesh 재질로 교체)
    GBufferMaterialData matData = { 0.5f, 0.0f, { 0.0f, 0.0f } };
    materialBuffer->Upload(&matData, sizeof(matData));

    pipelineState->Bind(cmdList);
    cmdList->SetConstantBuffer(viewProjBuffer.get(), 0);   // root param 0 → b0
    cmdList->SetConstantBuffer(materialBuffer.get(), 1);   // root param 1 → b1
    cmdList->SetPrimitiveTopology(EPrimitiveTopology::TriangleList);

    const auto& commands = scene.GetCommands();
    if (commands.empty())
        return;

    // 불투명 정적 메시 처리
    struct DrawGroup { IMesh* mesh; ITexture* tex; uint32_t count; };
    std::vector<DrawGroup> drawList;
    std::vector<Matrix4x4> allInstances;
    allInstances.reserve(commands.size());

    for (const RenderCommand& cmd : commands)
    {
        if (!cmd.mesh) continue;
        bool translucent = (cmd.sortKey & (1ULL << 59)) != 0;
        if (translucent) continue;

        ITexture* tex = cmd.texture ? cmd.texture : defaultTexture.get();
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

    if (!drawList.empty())
    {
    instanceBuffer->Upload(allInstances.data(), allInstances.size() * sizeof(Matrix4x4));

    ITexture* boundTex   = nullptr;
    uint32_t  byteOffset = 0;
    for (auto& [mesh, tex, count] : drawList)
    {
        if (tex != boundTex)
        {
            tex->Bind(cmdList, 2);   // root param 2 → t0
            boundTex = tex;
        }
        uint32_t clampedCount = std::min(count, MAX_GB_INSTANCES);
        cmdList->SetVertexBufferAt(instanceBuffer.get(), 1,
                                   static_cast<uint32_t>(sizeof(Matrix4x4)),
                                   clampedCount * static_cast<uint32_t>(sizeof(Matrix4x4)),
                                   byteOffset);
        mesh->DrawInstanced(cmdList, clampedCount);
        byteOffset += clampedCount * static_cast<uint32_t>(sizeof(Matrix4x4));
    }
    } // !drawList.empty()

    // ── 스키닝 메시 ─────────────────────────────────────────────────────────────
    if (skinnedMeshes.empty() || !skinnedPipelineState)
        return;

    skinnedPipelineState->Bind(cmdList);
    cmdList->SetConstantBuffer(viewProjBuffer.get(), 0);   // root 0 → b0
    cmdList->SetConstantBuffer(materialBuffer.get(), 1);   // root 1 → b1
    cmdList->SetPrimitiveTopology(EPrimitiveTopology::TriangleList);

    for (const RenderCommand& cmd : commands)
    {
        if (!cmd.skinnedMesh)
            continue;

        const uint32_t count = std::min(cmd.instanceCount, cmd.skinnedMesh->GetMaxInstances());

        if (cmd.texture)
            cmd.texture->Bind(cmdList, 2);              // root 2 → t0

        cmdList->SetGraphicsSRV(cmd.skinnedMesh->GetBonePaletteSRV(), 3);  // root 3 → t1

        skinnedInstanceBuffer->Upload(&cmd.worldMatrix, count * sizeof(Matrix4x4));
        cmdList->SetVertexBufferAt(skinnedInstanceBuffer.get(), 1,
                                   static_cast<uint32_t>(sizeof(Matrix4x4)),
                                   count * static_cast<uint32_t>(sizeof(Matrix4x4)),
                                   0);

        cmd.skinnedMesh->DrawInstanced(cmdList, count);
    }
}
