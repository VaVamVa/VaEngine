#include "ShadowMapRenderer.h"

#include "Render/RenderGraph.h"
#include "Render/IRenderPass.h"
#include "Scene/RenderScene.h"

#include "RHI/IRenderDevice.h"
#include "RHI/ICommandList.h"
#include "RHI/Buffer/IDepthBuffer.h"
#include "RHI/Pipeline/PipelineDesc.h"
#include "RHI/Common_RHI.h"
#include "Mesh/IMesh.h"
#include "Mesh/SkinnedMesh.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

static constexpr uint32_t MAX_SHADOW_INSTANCES = 1024;

struct ShadowViewProjData { float viewProj[16]; };

// ── RenderPass 정의 ────────────────────────────────────────────────────────────

namespace {

struct ShadowMapPass : IRenderPass
{
    ShadowMapPass(ShadowMapRenderer* renderer, uint32_t depthHandle, uint32_t cascadeIndex,
                  std::vector<SkinnedMesh*> skinnedMeshes)
        : renderer(renderer), depthHandle(depthHandle), cascadeIndex(cascadeIndex),
          skinnedMeshes(std::move(skinnedMeshes)) {}

    void OnCompile(RenderGraph& graph) override
    {
        depthBuffer = graph.GetTransientDepth(depthHandle);
    }

    void DeclareResources(std::vector<PassResourceDecl>& reads,
                          std::vector<PassResourceDecl>& writes) const override
    {
        writes.push_back({ depthBuffer, EResourceState::DepthWrite });
        // 스키닝 캐스터: compute UAV write → shadow VS SRV read barrier 자동 삽입
        // (호출 시점상 BonePaletteComputePass가 항상 먼저 실행되므로 최신 본 데이터를 읽는다)
        for (auto* m : skinnedMeshes)
            reads.push_back({ m->GetBonePaletteBuffer(), EResourceState::NonPixelShaderResource });
    }

    void Execute(ICommandList* cmdList, const RenderScene& scene) override
    {
        const uint32_t res = renderer->GetResolution();

        RenderPassDesc passDesc;
        passDesc.renderTargetCount          = 0;
        passDesc.depthStencil.view          = depthBuffer->GetView(cascadeIndex);
        passDesc.depthStencil.loadAction    = ELoadAction::Clear;
        passDesc.depthStencil.storeAction   = EStoreAction::Store;
        passDesc.depthStencil.clearColor[0] = 1.0f;

        cmdList->BeginRenderPass(passDesc);
        cmdList->SetViewport(0.0f, 0.0f, static_cast<float>(res), static_cast<float>(res), 0.0f, 1.0f);
        cmdList->SetScissorRect(0, 0, static_cast<int32_t>(res), static_cast<int32_t>(res));

        renderer->RenderShadowMap(cmdList, scene, cascadeIndex);

        cmdList->EndRenderPass();
    }

    ShadowMapRenderer*        renderer;
    uint32_t                  depthHandle;
    uint32_t                  cascadeIndex;
    std::vector<SkinnedMesh*> skinnedMeshes;
    IDepthBuffer*             depthBuffer = nullptr;  // graph 소유, 비소유 포인터
};

} // namespace

// ── Initialize ─────────────────────────────────────────────────────────────────

void ShadowMapRenderer::Initialize(IRenderDevice* device, const ShaderDesc& shaderDesc, uint32_t inResolution)
{
    resolution = inResolution;

    BindingEntry bindings[] = {
        { EBindingType::ConstantBuffer, 0, EShaderStage::Vertex },  // root 0 → b0
    };
    bindingLayout = device->CreateBindingLayout(bindings, 1);

    shader = device->CreateShader(shaderDesc);

    // POSITION만 소비 — 정적 메시(PrimitiveVertex)의 공유 정점 버퍼를 그대로 재사용
    VertexInputDesc inputs[] = {
        { "POSITION",          0, EPixelFormat::R32G32B32_FLOAT,    0,  0, false },
        { "INSTANCETRANSFORM", 0, EPixelFormat::R32G32B32A32_FLOAT, 0,  1, true  },
        { "INSTANCETRANSFORM", 1, EPixelFormat::R32G32B32A32_FLOAT, 16, 1, true  },
        { "INSTANCETRANSFORM", 2, EPixelFormat::R32G32B32A32_FLOAT, 32, 1, true  },
        { "INSTANCETRANSFORM", 3, EPixelFormat::R32G32B32A32_FLOAT, 48, 1, true  },
    };
    PipelineStateDesc psoDesc = {
        .shader           = shader.get(),
        .vertexInputs     = inputs,
        .vertexInputCount = 5,
        .rtvFormats       = {},
        .rtvCount         = 0,
        .dsvFormat        = EPixelFormat::D24_UNORM_S8_UINT,
        .depthEnable      = true,
        .bindingLayout    = bindingLayout.get()
    };
    pipelineState = device->CreatePipelineState(psoDesc);

    for (uint32_t c = 0; c < kMaxCascadeCount; ++c)
    {
        viewProjBuffers[c] = device->CreateBuffer({
            .size   = sizeof(ShadowViewProjData),
            .usage  = EBufferUsage::ConstantBuffer,
            .access = EMemoryAccess::Upload,
            .stride = 0
        });
    }

    instanceBuffer = device->CreateBuffer({
        .size   = MAX_SHADOW_INSTANCES * sizeof(Matrix4x4),
        .usage  = EBufferUsage::VertexBuffer,
        .access = EMemoryAccess::Upload,
        .stride = sizeof(Matrix4x4)
    });
}

// ── InitializeSkinned ──────────────────────────────────────────────────────────

void ShadowMapRenderer::InitializeSkinned(IRenderDevice* device, const ShaderDesc& shaderDesc)
{
    BindingEntry bindings[] = {
        { EBindingType::ConstantBuffer, 0, EShaderStage::Vertex },  // root 0 → b0 (viewProjBuffer 공용)
        { EBindingType::BufferSRV,      1, EShaderStage::Vertex },  // root 1 → t1 BonePalette
    };
    skinnedBindingLayout = device->CreateBindingLayout(bindings, 2);

    skinnedShader = device->CreateShader(shaderDesc);

    // POSITION/BONEINDEX/BONEWEIGHT만 소비 — SkinnedVertex(96B)의 NORMAL/COLOR/TEXCOORD/TANGENT는
    // depth-only라 불필요(GBufferSkinned.hlsl과 동일 오프셋, GBufferRenderer::InitializeSkinned 참조)
    VertexInputDesc inputs[] = {
        { "POSITION",          0, EPixelFormat::R32G32B32_FLOAT,    0,  0, false },
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
        .vertexInputCount = 7,
        .rtvFormats       = {},
        .rtvCount         = 0,
        .dsvFormat        = EPixelFormat::D24_UNORM_S8_UINT,
        .depthEnable      = true,
        .bindingLayout    = skinnedBindingLayout.get()
    };
    skinnedPipelineState = device->CreatePipelineState(psoDesc);

    skinnedInstanceBuffer = device->CreateBuffer({
        .size   = MAX_SHADOW_INSTANCES * sizeof(Matrix4x4),
        .usage  = EBufferUsage::VertexBuffer,
        .access = EMemoryAccess::Upload,
        .stride = sizeof(Matrix4x4)
    });
}

// ── AddPasses ──────────────────────────────────────────────────────────────────

void ShadowMapRenderer::AddPasses(RenderGraph& graph, const RenderScene& scene, uint32_t depthHandle,
                                   const std::vector<SkinnedMesh*>& skinnedMeshes)
{
    ComputeCascades(scene);

    // 한 기능 = Pass 구조체 여러 번 등록(Bloom/OIT와 동일 컨벤션) — 캐스케이드마다 독립된 Execute.
    // 활성치만큼만 등록 — 나머지 슬라이스는 이번 프레임에 그려지지 않는다(무해, 절대 샘플링 안 됨).
    for (uint32_t cascade = 0; cascade < activeCascadeCount; ++cascade)
        graph.AddPass<ShadowMapPass>(this, depthHandle, cascade, skinnedMeshes);
}

// ── ComputeCascades ────────────────────────────────────────────────────────────

void ShadowMapRenderer::ComputeCascades(const RenderScene& scene)
{
    const CameraData& camera = scene.GetCamera();
    const float camNear = camera.nearZ;
    const float camFar  = camera.farZ;
    const uint32_t n    = activeCascadeCount;

    // 1. PSSM(Practical Split Scheme) — 균등/로그 분할을 splitLambda로 혼합.
    //    배열은 항상 kMaxCascadeCount 기준으로 잡고 앞쪽 n+1개만 사용(런타임 크기의 스택 배열 회피).
    float splits[kMaxCascadeCount + 1];
    splits[0] = camNear;
    splits[n] = camFar;
    for (uint32_t i = 1; i < n; ++i)
    {
        const float p            = static_cast<float>(i) / static_cast<float>(n);
        const float logSplit     = camNear * std::pow(camFar / camNear, p);
        const float uniformSplit = camNear + (camFar - camNear) * p;
        splits[i]              = Math::Lerp(uniformSplit, logSplit, splitLambda);
        splitDistances[i - 1]  = splits[i];
    }

    // 2. 카메라 프러스텀 전체의 근/원 평면 코너 4+4개(월드 스페이스) — NDC 큐브를 Inverse(view*proj)로 언프로젝션
    const Matrix4x4 invViewProj = (camera.view * camera.proj).Inverse();
    Vector3 nearCorners[4];
    Vector3 farCorners[4];
    constexpr float kNdcXY[4][2] = { { -1.0f, -1.0f }, { 1.0f, -1.0f }, { -1.0f, 1.0f }, { 1.0f, 1.0f } };
    for (int i = 0; i < 4; ++i)
    {
        nearCorners[i] = UnprojectPoint({ kNdcXY[i][0], kNdcXY[i][1], 0.0f }, invViewProj);
        farCorners[i]  = UnprojectPoint({ kNdcXY[i][0], kNdcXY[i][1], 1.0f }, invViewProj);
    }

    const DirectionalLightData& dirLight = scene.GetLighting().dirLight;
    const Vector3 lightDir = Vector3(dirLight.direction[0], dirLight.direction[1], dirLight.direction[2]).Normalized();
    const Vector3 up = (std::abs(lightDir.y) > 0.99f) ? Vector3(0.0f, 0.0f, 1.0f) : Vector3(0.0f, 1.0f, 0.0f);

    // 3. 캐스케이드별 서브 프러스텀(근/원 코너를 split 비율로 보간) → bounding sphere → 광원 view*proj.
    //    뷰 스페이스에서 프러스텀 옆면이 직선이라, 근/원 코너를 선형 보간하면 중간 split 코너가 정확히 나온다.
    //    (텍셀 스냅핑은 하지 않음 — 카메라 이동 시 미세한 shimmering 가능성, 후속 개선 과제로 남김)
    for (uint32_t c = 0; c < n; ++c)
    {
        const float tNear = (splits[c]     - camNear) / (camFar - camNear);
        const float tFar  = (splits[c + 1] - camNear) / (camFar - camNear);

        Vector3 corners[8];
        for (int i = 0; i < 4; ++i)
        {
            corners[i]     = nearCorners[i] + (farCorners[i] - nearCorners[i]) * tNear;
            corners[i + 4] = nearCorners[i] + (farCorners[i] - nearCorners[i]) * tFar;
        }

        Vector3 center = Vector3::Zero;
        for (const auto& p : corners) center += p;
        center /= 8.0f;

        float radius = 0.0f;
        for (const auto& p : corners)
            radius = std::max(radius, Vector3::Distance(p, center));

        // 광원을 스피어 중심에서 lightDir 반대 방향으로 충분히 물러나 배치 — Ortho near/far가
        // 스피어를 완전히 포함하도록 여유(2×radius 이격, 4×radius far)를 둔다.
        const Vector3    eye  = center - lightDir * (radius * 2.0f);
        const Matrix4x4  view = Matrix4x4::LookAtLH(eye, center, up);
        const Matrix4x4  proj = Matrix4x4::OrthographicLH(radius * 2.0f, radius * 2.0f, 0.0f, radius * 4.0f);
        lightViewProj[c]      = view * proj;
    }
}

// ── RenderShadowMap ────────────────────────────────────────────────────────────

void ShadowMapRenderer::RenderShadowMap(ICommandList* cmdList, const RenderScene& scene, uint32_t cascadeIndex)
{
    ShadowViewProjData vpData;
    std::memcpy(vpData.viewProj, lightViewProj[cascadeIndex].m, sizeof(vpData.viewProj));
    IBuffer* viewProjBuffer = viewProjBuffers[cascadeIndex].get();
    viewProjBuffer->Upload(&vpData, sizeof(vpData));

    cmdList->SetPrimitiveTopology(EPrimitiveTopology::TriangleList);

    const auto& commands = scene.GetCommands();
    if (commands.empty())
        return;

    // 정적 불투명 메시 캐스터
    struct DrawGroup { IMesh* mesh; uint32_t count; };
    std::vector<DrawGroup> drawList;
    std::vector<Matrix4x4> allInstances;
    allInstances.reserve(commands.size());

    for (const RenderCommand& cmd : commands)
    {
        if (!cmd.mesh) continue;
        if (cmd.sortKey & (1ULL << 59)) continue;  // translucent — skip

        if (!drawList.empty() && drawList.back().mesh == cmd.mesh)
            ++drawList.back().count;
        else
            drawList.push_back({ cmd.mesh, 1 });
        allInstances.push_back(cmd.worldMatrix);
    }

    if (!drawList.empty())
    {
        instanceBuffer->Upload(allInstances.data(), allInstances.size() * sizeof(Matrix4x4));

        pipelineState->Bind(cmdList);
        cmdList->SetConstantBuffer(viewProjBuffer, 0);  // root 0 → b0

        uint32_t byteOffset = 0;
        for (auto& [mesh, count] : drawList)
        {
            const uint32_t clampedCount = std::min(count, MAX_SHADOW_INSTANCES);
            cmdList->SetVertexBufferAt(instanceBuffer.get(), 1,
                                       static_cast<uint32_t>(sizeof(Matrix4x4)),
                                       clampedCount * static_cast<uint32_t>(sizeof(Matrix4x4)),
                                       byteOffset);
            mesh->DrawInstanced(cmdList, clampedCount);
            byteOffset += clampedCount * static_cast<uint32_t>(sizeof(Matrix4x4));
        }
    }

    // 스키닝 메시 캐스터 — 커맨드마다 BonePalette가 달라 정적 메시처럼 배치할 수 없음
    // (GBufferRenderer::RenderGBuffer의 스키닝 드로우 루프와 동일 패턴)
    bool skinnedPSOBound = false;
    for (const RenderCommand& cmd : commands)
    {
        if (!cmd.skinnedMesh) continue;
        if (cmd.sortKey & (1ULL << 59)) continue;  // translucent — skip

        if (!skinnedPSOBound)
        {
            skinnedPipelineState->Bind(cmdList);
            cmdList->SetConstantBuffer(viewProjBuffer, 0);  // root 0 → b0
            cmdList->SetPrimitiveTopology(EPrimitiveTopology::TriangleList);
            skinnedPSOBound = true;
        }

        cmdList->SetGraphicsSRV(cmd.skinnedMesh->GetBonePaletteSRV(), 1);  // root 1 → t1

        const uint32_t count = std::min(cmd.instanceCount, cmd.skinnedMesh->GetMaxInstances());
        skinnedInstanceBuffer->Upload(&cmd.worldMatrix, count * sizeof(Matrix4x4));
        cmdList->SetVertexBufferAt(skinnedInstanceBuffer.get(), 1,
                                   static_cast<uint32_t>(sizeof(Matrix4x4)),
                                   count * static_cast<uint32_t>(sizeof(Matrix4x4)),
                                   0);

        cmd.skinnedMesh->DrawInstanced(cmdList, count);
    }
}
