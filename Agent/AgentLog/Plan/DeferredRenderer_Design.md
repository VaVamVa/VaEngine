# Deferred Renderer 설계 계획서

> 작성일: 2026-05-15  
> 목표: Compute Shader 기반 하이브리드 Deferred Renderer 구현  
> 방식: G-Buffer 패스(MRT) → Deferred Lighting(Compute) → Forward Transparent

---

## 1. 설계 목표

| 목표 | 내용 |
|---|---|
| 다수 광원 지원 | Deferred Lighting에서 수십~수백 개 동적 광원 처리 |
| 스키닝 통합 | 기존 AnimationRenderer가 GBuffer 패스에 통합 |
| Forward 재활용 | 반투명, 스카이박스, 디버그 — 기존 패스 그대로 사용 |
| 확장성 | Tiled / Clustered Deferred로 후속 확장 가능 |
| 추상화 연계 | `Refactoring_RenderAPI.md` 추상화 D항목과 함께 설계 |

---

## 1-1. 검토 필요

> 2026-05-15 코드 분석 기반 검토 결과. 본문 설계는 그대로 유지.

| # | 항목 | 위치 | 내용 |
|---|---|---|---|
| A | §5 G-Buffer RT 타입 불일치 | §5 ↔ §12.3 | §5 코드에서 G-Buffer RT를 `ITextureUAV`로 선언하고 있으나, §12.3에서 타입 분리 확정(G-Buffer RT = `IColorBuffer`, HDR Output = `ITextureUAV`). UAV 쓰기가 불필요한 RT에 `ITextureUAV`를 쓰는 것은 잘못된 타입. |
| B | GBufferRenderer ↔ IRenderer 초기화 시그니처 충돌 | §5, §7 | `IRenderer::Initialize(device, shaderDesc)`는 단일 ShaderDesc. GBufferRenderer는 정적 PSO + 스키닝 PSO 두 개 초기화가 필요하여 IRenderer 상속 시 시그니처 불일치. 독자 Initialize() 사용 또는 IRenderer 미상속 검토 필요. |
| C | Depth 소유권 이전 미설계 | §8 | 현재 ForwardRenderer가 Depth 소유. Deferred 이후 GBufferPass + SkyPass + TransparentPass 모두 동일 Depth를 재사용해야 하므로, GBufferRenderer로 Depth 소유권 이전 후 ForwardRenderer에 전달하는 설계가 필요. |
| D | AddOpaquePasses 내 Sky+Opaque 결합 | §7, §11 | ForwardRenderer::AddOpaquePasses()가 SkyPass + ForwardPass(불투명 정적)를 묶음. Deferred 이후 SceneRenderer에서 SkyPass만 별도 호출하려면 ForwardRenderer에 AddSkyPass() 분리가 필요하나 계획서에 미명시. |

---

## 2. 패스 흐름

### 현재 (Forward)

```
SkyPass
  → ForwardPass          (불투명 정적, DepthWrite)
  → AnimationPass        (불투명 스키닝, DepthWrite)
  → TransparentPass      (반투명 정적, DepthRead)
  → DebugLinePass
  → DebugTextPass
```

### 목표 (Deferred + Forward Hybrid)

```
GBufferPass              (불투명 정적 + 스키닝, MRT 4장 + Depth)
  → DeferredLightingPass (Compute: G-Buffer 읽기 → HDR 출력 UAV)
  → SkyPass              (HDR RT에 합성, Depth 재사용)
  → TransparentPass      (기존 ForwardRenderer TransparentPass 재사용)
  → PostProcessPass      (선택, HDR → LDR Tone Mapping)
  → DebugLinePass
  → DebugTextPass
```

**핵심 결정:**  
GBufferPass 하나에 정적 메시와 스키닝 메시를 모두 기록한다. AnimationRenderer는 GBuffer용 셰이더로 전환, 별도 패스를 유지하지 않는다.

---

## 3. G-Buffer 레이아웃

| RT | Format | 채널 |
|---|---|---|
| RT0 | `R8G8B8A8_UNORM` | Albedo(RGB) + AO(A) |
| RT1 | `R16G16B16A16_FLOAT` | WorldNormal(XYZ) + Roughness(W) |
| RT2 | `R8G8B8A8_UNORM` | Metallic(R) + 예약(GBA) |
| Depth | `D24_UNORM_S8_UINT` | 기존 재사용 |
| HDR Out | `R16G16B16A16_FLOAT` (UAV) | DeferredLighting 출력 |

**RT1 포맷 결정 근거:**  
Normal을 SNORM으로 저장하면 반구 방향 오차 발생 가능. FLOAT16으로 저장하면 XYZ 모두 정밀도 확보. 압축이 필요할 경우 Oct-encoding(R16G16)으로 전환 가능.

---

## 4. Deferred Lighting — Compute Shader 방식

### 단계 1: FullScreen Compute (초기 구현)

픽셀당 1 스레드, 전체 화면을 덮는 단순한 방식.

```
Dispatch(ceil(W/8), ceil(H/8), 1)
[numthreads(8, 8, 1)]
```

```hlsl
// DeferredLighting_CS.hlsl

Texture2D<float4>   gAlbedoAO    : register(t0);  // RT0
Texture2D<float4>   gNormalRough : register(t1);  // RT1
Texture2D<float4>   gMaterial    : register(t2);  // RT2
Texture2D<float>    gDepth       : register(t3);  // Depth SRV
RWTexture2D<float4> outHDR       : register(u0);  // HDR 출력

cbuffer CB_DeferredCamera : register(b0)
{
    float4x4 InvViewProj;
    float3   EyePos;
    float    _pad;
}

// CB_Lights (기존 Lighting.hlsli 재사용)

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    // 1. UV 계산
    float2 uv = (id.xy + 0.5f) / float2(screenW, screenH);

    // 2. G-Buffer 읽기
    float4 albedoAO    = gAlbedoAO[id.xy];
    float4 normalRough = gNormalRough[id.xy];
    float  depth       = gDepth[id.xy];

    // 3. NDC → WorldPos 재구성
    float4 ndcPos  = float4(uv * 2.0f - 1.0f, depth, 1.0f);
    ndcPos.y       = -ndcPos.y;
    float4 worldPos = mul(ndcPos, InvViewProj);
    worldPos       /= worldPos.w;

    // 4. PBR 조명 계산 (기존 Lighting.hlsli 함수 재사용)
    float3 N = normalize(normalRough.xyz);
    float  roughness = normalRough.w;
    float  metallic  = gMaterial[id.xy].r;
    float3 albedo    = albedoAO.rgb;

    float3 result = ComputeLighting(worldPos.xyz, N, albedo, roughness, metallic, EyePos);

    outHDR[id.xy] = float4(result, 1.0f);
}
```

### 단계 2: Tiled Deferred (선택적 확장)

16×16 타일 단위로 Light Culling + Lighting을 한 번에 처리. 광원 수가 64개 이상일 때 성능 이점.

```
[numthreads(TILE_SIZE, TILE_SIZE, 1)]
// Group Shared Memory로 타일 내 광원 목록 캐싱
groupshared uint tileLightIndices[MAX_LIGHTS_PER_TILE];
```

초기 구현에서는 단계 1로 시작, 광원 수가 성능 병목이 되는 시점에 단계 2로 전환.

---

## 5. 신규 구성 요소

### GBufferRenderer

```
Engine/Private/Render/GBufferRenderer.h
Engine/Private/Render/GBufferRenderer.cpp
```

책임:
- MRT(RT0~RT2) + Depth 바인딩
- 정적 메시 GBuffer 기록
- 스키닝 메시 GBuffer 기록 (AnimationRenderer 역할 흡수 또는 협력)

```cpp
class GBufferRenderer : public IRenderer
{
public:
    void Initialize(IRenderDevice* device, ...);
    void AddPasses(RenderGraph& graph, const FrameOutput& output, const RenderScene& scene) override;

private:
    // Static mesh PSO
    std::unique_ptr<IPipelineState> staticPSO;
    // Skinned mesh PSO (AnimationRenderer와 PSO 분리 or 공유)
    std::unique_ptr<IPipelineState> skinnedPSO;
    // G-Buffer RT들
    std::unique_ptr<ITextureUAV>    gbRT0;  // Albedo+AO
    std::unique_ptr<ITextureUAV>    gbRT1;  // Normal+Roughness
    std::unique_ptr<ITextureUAV>    gbRT2;  // Metallic
};
```

### DeferredLightingRenderer

```
Engine/Private/Render/DeferredLightingRenderer.h
Engine/Private/Render/DeferredLightingRenderer.cpp
```

책임:
- G-Buffer SRV 바인딩
- CB_DeferredCamera, CB_Lights 업로드
- Compute Dispatch
- HDR UAV 출력

```cpp
class DeferredLightingRenderer
{
public:
    void Initialize(IRenderDevice* device, const ShaderDesc& csDesc);
    void AddPasses(RenderGraph& graph, ITextureUAV* hdrOut,
                   const GBufferRenderer& gbuffer, const RenderScene& scene);
private:
    std::unique_ptr<IPipelineState> computePSO;
    std::unique_ptr<IBuffer>        cameraBuffer;  // CB_DeferredCamera
    std::unique_ptr<IBuffer>        lightsBuffer;
};
```

---

## 6. G-Buffer 텍스처 소유권

G-Buffer RT들은 프레임마다 재사용되는 영구 리소스다. 두 가지 선택이 있다.

| 옵션 | 소유 위치 | 특징 |
|---|---|---|
| A | `GBufferRenderer` 멤버 | 렌더러 내부 캡슐화, 외부 접근은 getter |
| B | `Execute` 또는 `RenderGraph` 관리 | 패스 간 공유 명시적, 수명 관리 한 곳에 집중 |

**권장: 옵션 A**  
`GBufferRenderer`가 생성 / 소유하고, `DeferredLightingRenderer::AddPasses`에 const-ref로 전달. RenderGraph에는 `ImportResource`로 등록하여 배리어 자동 추적.

---

## 7. 추상화 항목 D 연계

`Refactoring_RenderAPI.md` 추상화 D항목에서 `IRenderer` 패스 단계 분리가 필요하다고 정리했다.  
Deferred 도입으로 `Execute.cpp`에 등록되는 렌더러와 패스가 늘어나므로, D항목을 **Deferred 구현과 동시에** 해결하는 것이 자연스럽다.

**확정 — `SceneRenderer` 합성 클래스** (← `Refactoring_RenderAPI.md` D항목)

`EPassStage enum` 방식으로는 GBuffer → Lighting → Sky → Transparent → Debug 순서를 자연스럽게 표현할 수 없어 합성 클래스 방식으로 확정됐다.

**SceneRenderer 클래스 설계:**

```cpp
// Engine/Private/Render/SceneRenderer.h
class SceneRenderer
{
public:
    void Initialize(IRenderDevice* device, ...);
    // Execute.cpp는 이 하나만 호출
    void AddPasses(RenderGraph& graph, const FrameOutput& output, const RenderScene& scene);

private:
    GBufferRenderer          gbufferRenderer;    // 불투명 정적 + 스키닝 GBuffer 기록
    DeferredLightingRenderer lightingRenderer;   // Compute: G-Buffer → HDR 출력
    ForwardRenderer          forwardRenderer;    // SkyPass + TransparentPass + DebugPasses
};
```

**SceneRenderer::AddPasses 내부 패스 순서:**

```
BonePaletteComputePass (Compute — 본 팔레트 갱신)
→ GBufferPass          (불투명 정적 + 스키닝, MRT 4장 + Depth)
→ DeferredLightingPass (Compute: G-Buffer SRV → HDR UAV)
→ SkyPass              (HDR RT에 합성, Depth 재사용)
→ TransparentPass      (ForwardBlend, HDR RT에 합성)
→ DebugLinePass
→ DebugTextPass
```

**Execute.cpp 변경 전/후:**

```cpp
// 변경 전 — 구체 타입 직접 호출 (6개 호출, 순서 Execute.cpp가 관리)
renderer.AddOpaquePasses(renderGraph, output, scene);
auto skinnedMeshes = animationRenderer.AddComputePasses(renderGraph, scene);
animationRenderer.AddGraphicsPasses(renderGraph, output, skinnedMeshes);
renderer.AddTransparentPasses(renderGraph, output);
renderer.AddDebugLinePasses(renderGraph, output);
renderer.AddDebugTextPasses(renderGraph, output);

// 변경 후 — SceneRenderer 하나로 (1개 호출, 순서 SceneRenderer가 관리)
sceneRenderer.AddPasses(renderGraph, output, scene);
```

**IRenderer 인터페이스 처리:**
- `SceneRenderer`는 `IRenderer`를 상속하지 않아도 된다 (Execute.cpp에서 직접 소유).
- `GBufferRenderer`, `DeferredLightingRenderer`는 각자의 구체 인터페이스를 가진다.
- `IRenderer`는 유지하되, 장기적으로 SceneRenderer를 IRenderer 구현체로 올릴 수 있다.

**변경 파일 목록:**

| 파일 | 변경 내용 |
|---|---|
| `Engine/Private/Render/SceneRenderer.h` | 신규 — 합성 클래스 |
| `Engine/Private/Render/SceneRenderer.cpp` | 신규 — AddPasses 내부 순서 구현 |
| `Engine/Private/Execute.h` | `renderer` / `animationRenderer` → `sceneRenderer` |
| `Engine/Private/Execute.cpp` | ForwardRenderer·AnimationRenderer 직접 호출 → `sceneRenderer.AddPasses` |
| `Engine/Public/Render/IRenderer.h` | 유지 (SceneRenderer 합성 후 선택 정리) |

`Execute.cpp`는 `SceneRenderer::AddPasses` 하나만 호출하고, 내부 순서는 `SceneRenderer`가 보장한다.

---

## 8. 기존 시스템과의 연계

### 투명 처리
`ForwardRenderer`의 `TransparentPass`를 그대로 재사용. GBufferPass 이후에 HDR RT에 ForwardBlend로 그린다.

### 스카이박스
`SkyPass`를 DeferredLighting 이후에 실행. HDR RT에 `ELoadAction::Load`로 합성.  
(또는 스카이박스를 Deferred Lighting Compute에서 depth=1.0f 픽셀에 직접 처리 — 선택적)

### Normal Mapping
GBufferPass 셰이더에서 노말 맵을 샘플링하여 WorldNormal을 RT1에 기록. 추가 패스 불필요.

### Shadow
Deferred Lighting Compute에서 `Shadow Map SRV`를 추가 바인딩하여 shadow factor를 조명 계산에 반영.  
Shadow Map 생성 패스(ShadowPass)는 GBufferPass 이전에 실행.

---

## 9. 구현 단계

| 단계 | 작업 | 선행 조건 | 상태 |
|---|---|---|---|
| **0** | RHI 확장 — `IColorBuffer` / `ITextureUAV GetRTV()` / `IDepthBuffer BindSRV()` / `EPixelFormat` / `PipelineStateDesc` MRT | 없음 | ✅ 완료 |
| **1** | G-Buffer 포맷 확정 + RT 생성 (`IColorBuffer` 활용) | 단계 0 | 미착수 |
| **2** | GBufferPass 셰이더 작성 (Static mesh only) | 단계 1 | 미착수 |
| **3** | DeferredLighting Compute 셰이더 작성 (Directional Light만) | 단계 2 | 미착수 |
| **4** | GBufferRenderer + DeferredLightingRenderer 클래스 구현 | 단계 3 | 미착수 |
| **5** | SceneRenderer 합성 클래스 구현 + Execute.cpp 교체 | 단계 4 | 미착수 |
| **6** | 스키닝 메시를 GBufferPass에 통합 (AnimationRenderer GBuffer 셰이더) | 단계 5 | 미착수 |
| **7** | Point / Spot 광원 Compute 추가 | 단계 6 | 미착수 |
| **8** | (선택) Tiled Deferred Lighting 전환 | 단계 7 | 미착수 |

**단계 1~3**은 셰이더 + 포맷 검증이 핵심이므로, 먼저 하드코딩 방식으로 프로토타입을 만들고 화면이 나오는 것을 확인한 뒤 클래스로 정리한다.

---

## 10. 셰이더 파일 목록 (신규)

| 파일 | 용도 |
|---|---|
| `GBuffer_VS.hlsl` | G-Buffer 기록 — 정적 메시 Vertex |
| `GBuffer_PS.hlsl` | G-Buffer 기록 — Pixel (MRT 출력) |
| `GBufferSkinned_VS.hlsl` | G-Buffer 기록 — 스키닝 메시 Vertex |
| `DeferredLighting_CS.hlsl` | Compute Deferred Lighting |
| `(TiledDeferred_CS.hlsl)` | (선택) Tiled Deferred Lighting |

---

## 11. 구현 진행 현황

> 기준일: 2026-05-15  
> 현재 상태: **미착수** — 설계 확정 단계. 빌드 검증 완료 후 단계 1부터 진행.

### 기존 시스템 역할 변화 (단계별)

| 파일 | 현재 역할 | 단계 5 (SceneRenderer) 이후 | 단계 6 (스키닝 통합) 이후 |
|---|---|---|---|
| `Execute.cpp` | `ForwardRenderer` + `AnimationRenderer` 직접 호출 (6개) | `SceneRenderer::AddPasses` 1개 호출 | 변화 없음 |
| `ForwardRenderer` | SkyPass + ForwardPass(Opaque) + TransparentPass + Debug | SkyPass + TransparentPass + Debug | 변화 없음 |
| `AnimationRenderer` | BonePaletteComputePass + AnimationPass | SceneRenderer 내부에서 BonePalette만 | GBufferRenderer에 흡수, 독립 패스 제거 |
| `GBufferRenderer` | (신규) | 불투명 정적 메시 GBuffer 기록 | 스키닝 메시 GBuffer 추가 |
| `DeferredLightingRenderer` | (신규) | G-Buffer SRV → Compute → HDR UAV 출력 | 변화 없음 |
| `SceneRenderer` | (신규) | 모든 렌더러 합성, 패스 순서 보장 | 변화 없음 |

### ForwardRenderer 역할 범위 변화

단계 5 이후 `ForwardRenderer`에서 `ForwardPass`(불투명 정적 메시 그래픽스) 기능은 제거된다.  
`AddOpaquePasses`는 `SceneRenderer` 내부에서 호출하지 않거나, `SkyPass`만 전달하는 형태로 축소된다.  
기존 ForwardPass PSO / BindingLayout은 GBufferRenderer PSO로 대체된다.

### 다음 착수 조건

- 이전 세션 변경분(RHI 추상화 A/B/C + WorldObject 리팩토링) 빌드 검증 완료 ✅  
- **단계 0 완료 후** 단계 1 시작 — §12 RHI 확장 항목 참조

---

## 12. 단계 0 — 필요한 RHI 확장

> G-Buffer 텍스처 생성과 MRT 바인딩을 위해 단계 1 착수 전에 완료해야 하는 항목.

| 항목 | 파일 | 상태 |
|---|---|---|
| `R16G16B16A16_FLOAT` 포맷 추가 | `Common_RHI.h` | ✅ 완료 |
| `PipelineStateDesc` MRT 지원 | `PipelineDesc.h` | ✅ 완료 |
| 신규 `IColorBuffer` 인터페이스 + DX12 구현 | `IColorBuffer.h`, `ColorBuffer_DirectX.h/.cpp` | ✅ 완료 |
| `IRenderDevice::CreateColorBuffer` 팩토리 | `IRenderDevice.h` + DX12 구현 | ✅ 완료 |
| `ITextureUAV`에 `GetRTV()` 추가 (HDR Output RT 바인딩용) | `ITextureUAV.h`, `TextureUAV_DirectX.h/.cpp` | ✅ 완료 |
| `IDepthBuffer`에 `BindSRV()` 추가 + typeless 포맷으로 생성 | `IDepthBuffer.h`, `DepthBuffer_DirectX.h/.cpp` | ✅ 완료 |

**확인된 기존 지원 현황:**

| 항목 | 상태 | 비고 |
|---|---|---|
| `RenderPassDesc::renderTargets[8]` | ✅ 지원 | 최대 8개 RTV 구조체 |
| `BeginRenderPass` MRT (`OMSetRenderTargets` 배열) | ✅ 지원 | `CommandList_DirectX.cpp:220` |
| `RenderGraph::ImportResource` | ✅ 재사용 가능 | `IColorBuffer`가 `IRHIResource` 상속 시 |
| Transient Depth 재사용 | ✅ 지원 | 동일 desc 재요청 시 같은 핸들 반환 |
| RenderGraph 배리어 자동화 | ✅ 지원 | `DeclareResources` reads/writes 기반 |

---

### 12.1 EPixelFormat — R16G16B16A16_FLOAT 추가

G-Buffer RT1(Normal+Roughness, 16-bit float 정밀도)과 HDR Output에 필요.

```cpp
// Common_RHI.h — 추가
enum class EPixelFormat : uint32_t
{
    // ...
    R16G16B16A16_FLOAT = 10,  // DXGI_FORMAT_R16G16B16A16_FLOAT
    // ...
};
```

---

### 12.2 PipelineStateDesc — MRT 지원

GBuffer PSO는 RT0~RT2 총 3개의 RTV 포맷을 지정해야 한다. 현재 `rtvFormat`(단일 값)을 배열로 확장.

```cpp
// PipelineDesc.h — 변경
struct PipelineStateDesc
{
    // ...
    EPixelFormat  rtvFormats[8] = {};  // 변경: 단일 → 배열
    uint32_t      rtvCount      = 1;   // 신규 (기본 1 → 기존 코드 호환)
    // rtvFormat 제거
};
```

기존 ForwardRenderer·AnimationRenderer PSO 생성 코드: `rtvFormats[0]`에 포맷 설정, `rtvCount = 1`.  
DX12 구현(`PipelineState_DirectX`)에서 `rtvCount`만큼 `RTVFormats[]` 배열 채움.

---

### 12.3 신규 IColorBuffer 인터페이스 + ITextureUAV 확장

**검증 결과 — `allowUAV` 통합 방식의 LSP 문제:**

`allowUAV=false` G-Buffer RT에서도 `BindUAV()`가 pure virtual로 존재하면 LSP 위반이다.  
→ 두 타입을 명확히 분리한다.

**두 역할의 텍스처 요구사항:**

| 텍스처 | 쓰기 방식 | 읽기 방식 | D3D12 Resource Flags | 타입 |
|---|---|---|---|---|
| G-Buffer RT0~RT2 | `BeginRenderPass` (RT write) | Compute SRV | `ALLOW_RENDER_TARGET` | `IColorBuffer` (신규) |
| HDR Output | Compute UAV write | `BeginRenderPass` (RT), SRV | `ALLOW_RENDER_TARGET \| ALLOW_UNORDERED_ACCESS` | `ITextureUAV` + `GetRTV()` 추가 |

---

**`IColorBuffer` — G-Buffer RT 전용 (RT + SRV):**

```cpp
// Engine/Public/RHI/Texture/IColorBuffer.h
class IColorBuffer : public IRHIResource
{
public:
    virtual ~IColorBuffer() = default;

    virtual void Create(IRenderDevice* device,
                        EPixelFormat   format,
                        uint32_t       width,
                        uint32_t       height) = 0;

    // BeginRenderPass의 RenderPassAttachment::view에 전달
    virtual IResourceView* GetRTV() const = 0;

    // Compute / Graphics에서 SRV read
    virtual void BindSRV(ICommandList* cmdList, uint32_t slot, bool isCompute) = 0;
};
```

**`ITextureUAV` 확장 — HDR Output (기존 UAV+SRV + RTV 추가):**

```cpp
// ITextureUAV.h — GetRTV() 추가
class ITextureUAV : public IRHIResource
{
public:
    // ... 기존 메서드 ...
    virtual IResourceView* GetRTV() const = 0;  // 신규 — BeginRenderPass RT 슬롯용
};
```

`TextureUAV_DirectX`에서 RTV view 추가 생성:
- `D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET`
- RTV: 인스턴스별 1-slot RTV heap (`DepthBuffer_DirectX`의 DSV heap 패턴과 동일)

**DX12 구현 `ColorBuffer_DirectX` 설계:**
- 리소스 생성: `D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET`
- 초기 상태: `D3D12_RESOURCE_STATE_RENDER_TARGET`
- RTV: 인스턴스별 1-slot RTV heap (`DepthBuffer_DirectX` 패턴 그대로)
- SRV: 전역 SRV heap에서 슬롯 할당 (`AllocateSRVDescriptor` 기존 방식)

---

### 12.5 IDepthBuffer SRV 지원 추가

DeferredLighting Compute에서 Depth를 SRV로 읽어 WorldPos를 역투영한다.

**문제:** D3D12는 `DXGI_FORMAT_D24_UNORM_S8_UINT`로 생성된 리소스에 SRV를 생성하는 것을 허용하지 않는다.

**해결:** 리소스를 typeless 포맷으로 생성하고 DSV/SRV view를 별도 포맷으로 생성.

```
리소스 생성 포맷: DXGI_FORMAT_R24G8_TYPELESS
DSV view 포맷:   DXGI_FORMAT_D24_UNORM_S8_UINT  (기존과 동일)
SRV view 포맷:   DXGI_FORMAT_R24_UNORM_X8_TYPELESS  (depth channel)
```

`IDepthBuffer.h` 변경:
```cpp
class IDepthBuffer
{
public:
    virtual IRHIResource*  GetResource() const = 0;
    virtual IResourceView* GetView()     const = 0;
    virtual void           BindSRV(ICommandList* cmdList, uint32_t slot, bool isCompute) = 0;  // 신규
};
```

`DepthBuffer_DirectX`에서 추가로 SRV 슬롯 할당 + `D3D12_SHADER_RESOURCE_VIEW_DESC` 생성.

---

### 12.4 IRenderDevice — CreateColorBuffer 팩토리

```cpp
// IRenderDevice.h — 추가
[[nodiscard]] virtual std::unique_ptr<IColorBuffer> CreateColorBuffer(
    EPixelFormat format, uint32_t width, uint32_t height) = 0;
```

---

### 12.6 CB_DeferredCamera — 스크린 크기 필드 추가

HLSL에서 UV 계산에 `screenW`, `screenH`가 필요하다.

```cpp
// DeferredLightingRenderer 내부 CB 레이아웃
struct CB_DeferredCamera
{
    float4x4 InvViewProj;
    float3   EyePos;
    float    _pad;
    uint32_t ScreenW;    // 추가
    uint32_t ScreenH;    // 추가
    float    _pad2[2];
};
```

```hlsl
// DeferredLighting_CS.hlsl
cbuffer CB_DeferredCamera : register(b0)
{
    float4x4 InvViewProj;
    float3   EyePos; float _pad;
    uint     ScreenW; uint ScreenH; float2 _pad2;
}
```

---

### 12.7 HDR Output → 백버퍼 연결 경로

Deferred 도입 후 패스 흐름 끝에서 HDR buffer를 백버퍼에 전달해야 한다.  
`FrameOutput.backBuffer`는 그대로 백버퍼를 가리키고, HDR buffer는 SceneRenderer 내부 리소스로 관리한다.

**두 가지 선택지:**

| 방식 | 구현 | 비용 |
|---|---|---|
| **Blit Pass** | FullScreen quad — HDR SRV → 백버퍼 RT | Vertex/Pixel 셰이더 1쌍 추가 |
| **ToneMapping Compute** | Compute — HDR UAV read + LDR 출력 → 백버퍼 | `ITextureUAV`에 UAV read 가능하면 OK |

**초기 구현**: Blit Pass (HDR → 백버퍼 단순 복사, Tone Mapping 없음). 추후 PostProcess 단계에서 Tone Mapping Compute로 교체.

`FrameOutput` 구조체는 변경하지 않는다. SceneRenderer가 내부적으로 HDR buffer를 보유하고, Blit Pass에서 백버퍼(FrameOutput.backBuffer)로 출력.

---

### 12.8 CB_Lights 중복 정책 결정

Deferred 도입 후 lights 데이터가 두 곳에서 사용된다.

| 위치 | 용도 | 결론 |
|---|---|---|
| `ForwardRenderer.lightsBuffer` | TransparentPass (Forward 방식 유지) | **유지** |
| `DeferredLightingRenderer.lightsBuffer` | Deferred Lighting Compute | **신규 추가** |

TransparentPass가 Forward Shading을 계속 쓰는 한 두 버퍼가 공존한다.  
SceneRenderer에서 lights 데이터를 한 번만 업로드하고 두 렌더러에 포인터로 전달하는 방식도 가능하지만, 복잡도가 올라간다.  
**현재 결정: 각자 독립 업로드 유지.** 성능 병목이 되는 시점에 SceneRenderer 공유 방식으로 전환.
