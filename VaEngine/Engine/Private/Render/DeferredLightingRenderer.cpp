#include "DeferredLightingRenderer.h"
#include "GBufferRenderer.h"
#include "ShadowMapRenderer.h"  // kMaxCascadeCount 상수 공유(매직넘버 중복 방지)

#include "Render/RenderGraph.h"
#include "Render/IRenderPass.h"
#include "Render/ILight.h"
#include "Scene/RenderScene.h"

#include "RHI/IRenderDevice.h"
#include "RHI/ICommandList.h"
#include "RHI/Buffer/IDepthBuffer.h"
#include "RHI/Buffer/IColorBuffer.h"
#include "RHI/Texture/ITextureUAV.h"
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
    uint32_t ssaoEnabled;    // 0/1 — 런타임 디버그 토글(비교분석용)
    uint32_t showCascades;   // 0/1 — CSM 캐스케이드 색상 오버레이 토글(구 _pad2 슬롯 재사용)
    float    cameraForward[3];  // CSM 캐스케이드 선택 — split distance와 같은 기준(뷰 스페이스 깊이) 유지용
    float    _pad3;
};

// b2 — Lighting.hlsli CB_Lights 와 1:1 대응
static constexpr uint32_t MAX_DL_POINT_LIGHTS = 8;
static constexpr uint32_t MAX_DL_SPOT_LIGHTS  = 4;

struct DL_LightsBufferData
{
    DirectionalLightData dirLight;                             //   32 bytes
    PointLightData       pointLights[MAX_DL_POINT_LIGHTS];    //  384 bytes (48 * 8)
    SpotLightData        spotLights[MAX_DL_SPOT_LIGHTS];      //  256 bytes (64 * 4)
    float                eyePosW[3];                          //   12 bytes
    int32_t              numPointLights;                      //    4 bytes
    int32_t              numSpotLights;                       //    4 bytes
    uint32_t             iblEnabled;                           //    4 bytes — CB_Lights.gIBLEnabled
    float                _lightPad[2];                        //    8 bytes
    // total: 704 bytes → 768 (CBV 256-aligned)
};

// b1 — Shadow Map 광원 공간 행렬(CSM, 캐스케이드별). 항상 kMaxCascadeCount(8)분 크기로 고정 — 활성치는
// activeCascadeCount 필드로 셰이더에 전달(Mesh LOD와 동일 패턴, `2026-07-13_Q&A.md` Q7).
// splitDistances는 HLSL의 평범한 스칼라 배열 float[kMaxCascadeCount-1]과 바이트 단위로 동일하게 맞춘다 —
// HLSL cbuffer 규칙상 스칼라 배열은 원소마다 16바이트(float4 슬롯)로 패딩되므로, C++ 쪽도 원소당
// [값, pad, pad, pad] 4-float로 선언해야 정합이 맞는다(260713-CompactLog#7 — float4[2]+이중 동적 인덱싱이던
// 이전 레이아웃은 캐스케이드 6개 이상에서 그림자가 갈라지는 버그의 원인으로 확인돼 이 형태로 교체).
struct CB_ShadowLightVP
{
    float    viewProj[ShadowMapRenderer::kMaxCascadeCount][16];
    float    splitDistances[ShadowMapRenderer::kMaxCascadeCount - 1][4];  // [s][0]=경계값, [s][1..3]=패딩
    float    texelSize;
    float    blendWidth;
    uint32_t activeCascadeCount;
    float    _pad;
};

// ── RenderPass 정의 ────────────────────────────────────────────────────────────

namespace {

struct DeferredLightingPass : IRenderPass
{
    DeferredLightingPass(DeferredLightingRenderer* renderer, GBufferRenderer* gbuffer,
                         uint32_t shadowDepthHandle, const Matrix4x4* cascadeViewProj,
                         const float* splitDistances, uint32_t activeCascadeCount, float blendWidth,
                         uint32_t shadowResolution,
                         ITextureUAV* irradianceMap, ITextureUAV* prefilteredMap, ITextureUAV* brdfLUT,
                         bool iblEnabled, IColorBuffer* ssaoMap, bool ssaoEnabled, bool showCascades)
        : renderer(renderer), gbuffer(gbuffer), shadowDepthHandle(shadowDepthHandle),
          cascadeViewProj(cascadeViewProj), splitDistances(splitDistances),
          activeCascadeCount(activeCascadeCount), blendWidth(blendWidth),
          shadowResolution(shadowResolution), irradianceMap(irradianceMap),
          prefilteredMap(prefilteredMap), brdfLUT(brdfLUT), iblEnabled(iblEnabled), ssaoMap(ssaoMap),
          ssaoEnabled(ssaoEnabled), showCascades(showCascades) {}

    void OnCompile(RenderGraph& graph) override
    {
        shadowDepth = graph.GetTransientDepth(shadowDepthHandle);
    }

    void DeclareResources(std::vector<PassResourceDecl>& reads,
                          std::vector<PassResourceDecl>& writes) const override
    {
        reads.push_back({ gbuffer->GetRT0(),     EResourceState::NonPixelShaderResource });
        reads.push_back({ gbuffer->GetRT1(),     EResourceState::NonPixelShaderResource });
        reads.push_back({ gbuffer->GetRT2(),     EResourceState::NonPixelShaderResource });
        reads.push_back({ gbuffer->GetDepth(),   EResourceState::NonPixelShaderResource });
        reads.push_back({ shadowDepth,           EResourceState::NonPixelShaderResource });
        reads.push_back({ irradianceMap,         EResourceState::NonPixelShaderResource });
        reads.push_back({ prefilteredMap,        EResourceState::NonPixelShaderResource });
        reads.push_back({ brdfLUT,               EResourceState::NonPixelShaderResource });
        reads.push_back({ ssaoMap,               EResourceState::NonPixelShaderResource });
        writes.push_back({ gbuffer->GetHdrOut(), EResourceState::UnorderedAccess        });
    }

    void Execute(ICommandList* cmdList, const RenderScene& scene) override
    {
        renderer->Dispatch(cmdList, *gbuffer, scene, shadowDepth, cascadeViewProj, splitDistances,
                           activeCascadeCount, blendWidth, shadowResolution,
                           irradianceMap, prefilteredMap, brdfLUT, iblEnabled, ssaoMap, ssaoEnabled,
                           showCascades);
    }

    DeferredLightingRenderer* renderer;
    GBufferRenderer*          gbuffer;
    uint32_t                  shadowDepthHandle;
    const Matrix4x4*          cascadeViewProj;  // ShadowMapRenderer 소유(수명 렌더러 전체) — 비소유 포인터
    const float*              splitDistances;   // 위와 동일
    uint32_t                  activeCascadeCount;
    float                     blendWidth;
    uint32_t                  shadowResolution;
    ITextureUAV*              irradianceMap;
    ITextureUAV*              prefilteredMap;
    ITextureUAV*              brdfLUT;
    bool                      iblEnabled;
    IColorBuffer*             ssaoMap;
    bool                      ssaoEnabled;
    bool                      showCascades;
    IDepthBuffer*             shadowDepth = nullptr;  // graph 소유, 비소유 포인터
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
    // root 7~8: t4(shadowDepth SRV), b1(CB_ShadowLightVP) — append, 기존 root 0~6 불변 (Phase 1-3)
    // root 9: t5(irradianceMap SRV) — append, 기존 root 0~8 불변 (IBL Stage A)
    // root 10~11: t6(prefilteredMap SRV), t7(brdfLUT SRV) — append, 기존 root 0~9 불변 (IBL Stage B)
    // root 12: t8(ssaoMap SRV) — append, 기존 root 0~11 불변 (SSAO)
    BindingEntry bindings[] = {
        { EBindingType::ConstantBuffer, 0, EShaderStage::Compute },
        { EBindingType::ConstantBuffer, 2, EShaderStage::Compute },
        { EBindingType::Texture,        0, EShaderStage::Compute },
        { EBindingType::Texture,        1, EShaderStage::Compute },
        { EBindingType::Texture,        2, EShaderStage::Compute },
        { EBindingType::Texture,        3, EShaderStage::Compute },
        { EBindingType::TextureUAV,     0, EShaderStage::Compute },
        { EBindingType::Texture,        4, EShaderStage::Compute },  // root 7 — t4 shadowDepth SRV
        { EBindingType::ConstantBuffer, 1, EShaderStage::Compute },  // root 8 — b1 CB_ShadowLightVP
        { EBindingType::Texture,        5, EShaderStage::Compute },  // root 9 — t5 irradianceMap SRV
        { EBindingType::Texture,        6, EShaderStage::Compute },  // root 10 — t6 prefilteredMap SRV
        { EBindingType::Texture,        7, EShaderStage::Compute },  // root 11 — t7 brdfLUT SRV
        { EBindingType::Texture,        8, EShaderStage::Compute },  // root 12 — t8 ssaoMap SRV
    };
    computeBindingLayout = device->CreateBindingLayout(bindings, 13, /*isCompute*/ true);

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

    shadowLightVPBuffer = device->CreateBuffer({
        .size   = sizeof(CB_ShadowLightVP),
        .usage  = EBufferUsage::ConstantBuffer,
        .access = EMemoryAccess::Upload,
        .stride = 0
    });
}

// ── AddPasses ──────────────────────────────────────────────────────────────────

void DeferredLightingRenderer::AddPasses(RenderGraph& graph, GBufferRenderer& gbuffer,
                                          uint32_t shadowDepthHandle, const Matrix4x4* cascadeViewProj,
                                          const float* splitDistances, uint32_t activeCascadeCount,
                                          float blendWidth,
                                          uint32_t shadowResolution, ITextureUAV* irradianceMap,
                                          ITextureUAV* prefilteredMap, ITextureUAV* brdfLUT,
                                          bool iblEnabled, IColorBuffer* ssaoMap, bool ssaoEnabled,
                                          bool showCascades)
{
    graph.AddPass<DeferredLightingPass>(this, &gbuffer, shadowDepthHandle, cascadeViewProj, splitDistances,
                                        activeCascadeCount, blendWidth, shadowResolution,
                                        irradianceMap, prefilteredMap, brdfLUT, iblEnabled, ssaoMap, ssaoEnabled,
                                        showCascades);
}

// ── Dispatch ───────────────────────────────────────────────────────────────────

void DeferredLightingRenderer::Dispatch(ICommandList* cmdList, GBufferRenderer& gbuffer,
                                        const RenderScene& scene, IDepthBuffer* shadowDepth,
                                        const Matrix4x4* cascadeViewProj, const float* splitDistances,
                                        uint32_t activeCascadeCount, float blendWidth, uint32_t shadowResolution,
                                        ITextureUAV* irradianceMap, ITextureUAV* prefilteredMap,
                                        ITextureUAV* brdfLUT, bool iblEnabled, IColorBuffer* ssaoMap,
                                        bool ssaoEnabled, bool showCascades)
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
    camData.ssaoEnabled   = ssaoEnabled   ? 1u : 0u;
    camData.showCascades  = showCascades  ? 1u : 0u;
    // 월드 스페이스 forward축 — view 행렬(world→view) 전용 추출자 GetViewForward 사용(260713-CompactLog#7
    // 버그 이후 260713-CompactLog#8에서 신설 — World용 GetWorldForward와 축 저장 위치가 반대라 섞어 쓰면 안 됨).
    const Vector3 camForward = GetViewForward(cam.view);
    camData.cameraForward[0] = camForward.x;
    camData.cameraForward[1] = camForward.y;
    camData.cameraForward[2] = camForward.z;
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
    ldata.iblEnabled = iblEnabled ? 1u : 0u;
    lightsBuffer->Upload(&ldata, sizeof(ldata));

    // CB_ShadowLightVP 업로드(CSM) — 항상 kMaxCascadeCount분 전체를 복사(활성치 이후는 셰이더가
    // gActiveCascadeCount로 걸러 읽지 않으므로 무해) + split distance + blend 폭 + 활성 캐스케이드 개수
    CB_ShadowLightVP shadowData = {};
    for (uint32_t c = 0; c < ShadowMapRenderer::kMaxCascadeCount; ++c)
        std::memcpy(shadowData.viewProj[c], cascadeViewProj[c].m, sizeof(shadowData.viewProj[c]));
    for (uint32_t s = 0; s < ShadowMapRenderer::kMaxCascadeCount - 1; ++s)
        shadowData.splitDistances[s][0] = splitDistances[s];  // [s][1..3]은 HLSL 배열 패딩용, 0 유지
    shadowData.texelSize          = 1.0f / static_cast<float>(shadowResolution);
    shadowData.blendWidth         = blendWidth;
    shadowData.activeCascadeCount = activeCascadeCount;
    shadowLightVPBuffer->Upload(&shadowData, sizeof(shadowData));

    // Compute pass 실행
    computePipelineState->Bind(cmdList);
    cmdList->SetComputeConstantBuffer(cameraBuffer.get(), 0);    // root 0 → b0
    cmdList->SetComputeConstantBuffer(lightsBuffer.get(), 1);    // root 1 → b2

    gbuffer.GetRT0()->BindSRV(cmdList, 2, true);                 // root 2 → t0
    gbuffer.GetRT1()->BindSRV(cmdList, 3, true);                 // root 3 → t1
    gbuffer.GetRT2()->BindSRV(cmdList, 4, true);                 // root 4 → t2
    gbuffer.GetDepth()->BindSRV(cmdList, 5, true);               // root 5 → t3
    gbuffer.GetHdrOut()->BindUAV(cmdList, 6, true);              // root 6 → u0
    shadowDepth->BindSRV(cmdList, 7, true);                      // root 7 → t4
    cmdList->SetComputeConstantBuffer(shadowLightVPBuffer.get(), 8);  // root 8 → b1
    irradianceMap->BindSRV(cmdList, 9, true);                    // root 9 → t5
    prefilteredMap->BindSRV(cmdList, 10, true);                  // root 10 → t6
    brdfLUT->BindSRV(cmdList, 11, true);                         // root 11 → t7
    ssaoMap->BindSRV(cmdList, 12, true);                         // root 12 → t8

    const uint32_t groupX = (width  + 7) / 8;
    const uint32_t groupY = (height + 7) / 8;
    cmdList->Dispatch(groupX, groupY, 1);
}
