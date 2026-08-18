#pragma once

#include "RHI/IRenderDevice.h"
#include "RHI/Buffer/IBuffer.h"
#include "RHI/Shader/IShader.h"
#include "RHI/Pipeline/IBindingLayout.h"
#include "RHI/Pipeline/IPipelineState.h"
#include "Math/Container.h"

#include <memory>
#include <cstdint>
#include <vector>
#include <array>
#include <algorithm>

class RenderGraph;
class RenderScene;
class ICommandList;
class IDepthBuffer;
class SkinnedMesh;

// Directional Light 기준 Cascaded Shadow Map. kMaxCascadeCount장 슬라이스를 Texture2DArray depth에
// 항상 확보해두고, 매 프레임 activeCascadeCount(런타임 조정 가능)개만 실제로 다시 그린다 — Mesh LOD와
// 동일한 패턴("최대치는 항상 할당, 실제 사용 개수만 런타임 제한")으로, 활성치를 바꿔도 리소스 재할당이
// 없다(§0-3 alias 방지 원칙 유지). 근거: `2026-07-13_Q&A.md` Q7.
// Point·Spot Light Shadow는 별도 과제로 남아있음(Plan_RenderQuality.md 기술 부채 표).
class ShadowMapRenderer
{
public:
    static constexpr uint32_t kMaxCascadeCount = 8;

    void Initialize(IRenderDevice* device, const ShaderDesc& shaderDesc, uint32_t resolution = 2048);
    void InitializeSkinned(IRenderDevice* device, const ShaderDesc& shaderDesc);

    // depthHandle: SceneRenderer가 graph.DeclareTransientDepth({.., arraySize=kMaxCascadeCount})로 미리
    //   확보한 핸들(§0-3 — 영속 아님, 크기 항상 고정). 캐스케이드별 view-proj는 이 안에서 매 프레임 재계산한다.
    // skinnedMeshes: AnimationRenderer::AddComputePasses가 반환한 이번 프레임 스키닝 메시 목록.
    //   BonePalette 버퍼를 read로 선언하는 데 쓰이므로, 호출 시점은 반드시 BonePalette Compute 이후여야 한다.
    void AddPasses(RenderGraph& graph, const RenderScene& scene, uint32_t depthHandle,
                   const std::vector<SkinnedMesh*>& skinnedMeshes);

    // ShadowMapPass::Execute 에서 호출 — cascadeIndex번째 슬라이스에 씬을 그린다.
    void RenderShadowMap(ICommandList* cmdList, const RenderScene& scene, uint32_t cascadeIndex);

    // kMaxCascadeCount개 원소 배열(활성치 이후는 미사용/무의미) — DeferredLightingRenderer가
    // CB_ShadowLightVP에 그대로 복사. 활성치는 GetActiveCascadeCount()로 별도 전달.
    const Matrix4x4* GetLightViewProj()  const { return lightViewProj.data(); }
    // kMaxCascadeCount-1개 원소 배열(카메라 기준 거리) — 캐스케이드 경계값, 활성치 이후는 미사용
    const float*     GetSplitDistances() const { return splitDistances.data(); }
    uint32_t         GetResolution()     const { return resolution; }

    // 실제 사용할 캐스케이드 개수(1~kMaxCascadeCount) — Debug/GUI에서 런타임 조정 가능. 리소스는
    // 항상 kMaxCascadeCount만큼 확보돼 있으므로 이 값을 바꿔도 재할당이 없다(Mesh LOD와 동일 패턴).
    void     SetActiveCascadeCount(uint32_t value) { activeCascadeCount = std::clamp(value, 1u, kMaxCascadeCount); }
    uint32_t GetActiveCascadeCount() const         { return activeCascadeCount; }

    // PSSM(Practical Split Scheme) 균등/로그 분할 혼합 비율 — 0=균등 분할, 1=완전 로그 분할.
    // Debug/GUI에서 런타임 조정 가능하도록 노출(카메라 가까이일수록 좁은 캐스케이드를 원하면 1에 가깝게).
    void  SetSplitLambda(float value) { splitLambda = value; }
    float GetSplitLambda() const      { return splitLambda; }

    // 캐스케이드 전환 경계 smooth blend 폭(카메라 거리 단위) — Debug/GUI 튜닝용
    void  SetBlendWidth(float value) { blendWidth = value; }
    float GetBlendWidth() const      { return blendWidth; }

private:
    // 카메라 프러스텀을 activeCascadeCount구간으로 분할하고, 구간마다 광원 시점 bounding-sphere fit으로
    // view*proj를 재계산해 lightViewProj[]/splitDistances[]를 채운다. AddPasses에서 매 프레임 호출.
    void ComputeCascades(const RenderScene& scene);

    // 정적 메시 캐스터
    std::unique_ptr<IBindingLayout> bindingLayout;
    std::unique_ptr<IShader>        shader;
    std::unique_ptr<IPipelineState> pipelineState;
    // b0: light space view*proj (정적/스키닝 공용) — 캐스케이드마다 별도 버퍼, 항상 kMaxCascadeCount개
    // 생성. 커맨드 리스트는 CPU가 전부 기록한 뒤 GPU가 나중에 한꺼번에 실행하므로, 버퍼 하나를 여러 번
    // 재사용(Upload로 덮어쓰기)하면 GPU가 실제로 그릴 때는 마지막에 업로드된 값만 남아 여러 슬라이스가
    // 같은(잘못된) 투영으로 그려지는 버그가 생긴다(260713-CompactLog#3 실증) — 그래서 캐스케이드 수만큼
    // 독립된 버퍼가 항상 필요하다.
    std::array<std::unique_ptr<IBuffer>, kMaxCascadeCount> viewProjBuffers;
    std::unique_ptr<IBuffer>        instanceBuffer;  // slot 1: per-instance world (정적) — 캐스케이드마다 같은
                                                       // 내용을 재업로드하므로 재사용해도 안전(캐스터 위치는
                                                       // 캐스케이드와 무관)

    // 스키닝 메시 캐스터
    std::unique_ptr<IBindingLayout> skinnedBindingLayout;
    std::unique_ptr<IShader>        skinnedShader;
    std::unique_ptr<IPipelineState> skinnedPipelineState;
    std::unique_ptr<IBuffer>        skinnedInstanceBuffer;  // slot 1: per-instance world (스키닝)

    std::array<Matrix4x4, kMaxCascadeCount>     lightViewProj{};
    std::array<float, kMaxCascadeCount - 1>     splitDistances{};

    uint32_t activeCascadeCount = 4;  // 기존 동작(4캐스케이드)과 동일하게 시작
    float    splitLambda        = 0.7f;
    float    blendWidth         = 2.0f;
    uint32_t resolution         = 2048;
};
