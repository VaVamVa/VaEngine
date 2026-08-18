# Material 중심 렌더링 설계 계획서

작성일: 2026-06-06  
최종 수정: 2026-07-06  
상태: **Phase 1~6 완료. Phase 7 (Normal Mapping) 대기**

---

## 확정된 결정사항

| 항목 | 결정 |
|---|---|
| 조명 모델 | Phong → PBR (roughness/metallic). G-Buffer 레이아웃과 일치시킴 |
| 재질 데이터 업로드 | per-material-group. Dirty flag 방식. 추후 Push Constants 전환 고려 |
| DeferredLighting 조명 모델 | 완전 PBR (Cook-Torrance GGX). Windows 전용 |
| Android 조명 모델 | 동일 PBR 파라미터, 간소화 셰이더 (ForwardSceneRenderer에서 처리) |
| UnlitMaterial 경로 | ForwardRenderer (GBuffer 우회, depth 공유). 전용 바인딩 레이아웃 |
| Cutout 분기 | 단일 PS with branch. alphaThreshold=0 시 GPU 최적화에 의존 |

### D-1 업로드 전략 — 대안 로드맵

| 단계 | 방식 | 교체 비용 | 전환 시점 |
|---|---|---|---|
| 현재 | Dirty flag | — | 즉시 |
| 1차 (Android) | Push Constants | 중간 (바인딩 레이아웃 변경) | ForwardSceneRenderer 구현 시 |
| 장기 | Structured Buffer + index | 중간 | 고유 Material 수가 많아질 때 |

Dirty flag 구현 방식:
- Material 내부에 `bool dirty = true` 보유
- `SetRoughness()`, `SetMetallic()` 등 모든 세터 호출 시 `dirty = true`
- GBufferRenderer가 그룹 변경 시 `material->UpdateBufferIfDirty()` 호출
- `UpdateBufferIfDirty()` 내부: dirty이면 `buffer->Upload(data)` 후 `dirty = false`
- 정적 오브젝트는 1프레임 후 업로드 없음

### Android PBR 전략

Material 시스템(파라미터)과 셰이더 구현은 분리된다.

```
Windows: DeferredSceneRenderer → DeferredLighting_CS.hlsl  (완전 GGX)
Android: ForwardSceneRenderer  → ForwardLit_PS.glsl        (간소화 PBR)
```

같은 roughness/metallic 값을 읽되 셰이더 내부 연산을 간소화한다. ISceneRenderer 추상화로 자연스럽게 분기. Material 코드는 플랫폼 분기 없음.

---

## 해소된 문제 (Phase 1~6 완료)

### G-Buffer와 IMaterial 불일치 → 해소

```
Phase 6 이전 (Phong):           Phase 6 이후 (PBR):
  IMaterial: ambient/diffuse       IMaterial: albedo/roughness/metallic/ao/emissive
  HLSL: GBufferMaterialData(16B)   HLSL: CB_GBufferMaterial(b1, 48B)
  하드코딩 roughness=0.5          MaterialData와 1:1 매핑
```

`Lighting.hlsli` 전면 교체 (D_GGX · F_Schlick · G_Smith · EvalBRDF).  
`GBufferMaterial.hlsli` 신규 생성 — `CB_GBufferMaterial` b1 레지스터.  
GBuffer / DeferredLighting / ForwardOpaque / AnimationDemo / Transparent 셰이더 PBR로 교체.

### 텍스처 소유권 분리 → 해소

`RenderCommand.texture` 필드 제거 (Phase 5).  
모든 렌더러가 `cmd.material->GetAlbedoTexture()`로만 텍스처에 접근.  
`AddMesh()` · `AddSkinnedMesh()` 시그니처에서 ITexture* 파라미터 제거.

### Material GPU 버퍼 업로드 경쟁 조건 → 해소

Material이 자신의 IBuffer를 영속 소유. Dirty flag 패턴 적용.  
GBufferRenderer·ForwardRenderer·AnimationRenderer 모두 그룹 변경 시  
`cmd.material->UpdateBufferIfDirty()` + `cmd.material->GetBuffer()` bind (b1) 순서 보장.

---

## 목표 구조

### IMaterial 인터페이스

```cpp
class IMaterial
{
public:
    // 렌더링 모드 — Renderer가 PSO 선택에 사용
    virtual EBlendMode  GetBlendMode()       const = 0;
    virtual ECullMode   GetCullMode()        const = 0;
    virtual bool        IsDepthWrite()       const = 0;
    virtual float       GetAlphaThreshold()  const = 0;  // Cutout용 픽셀 폐기 임계값

    // 텍스처 슬롯 — non-owning 포인터. 소유권은 WorldObject 서브클래스
    virtual ITexture*   GetAlbedoTexture()   const = 0;
    virtual ITexture*   GetNormalTexture()   const = 0;
    virtual void        SetAlbedoTexture(ITexture*) = 0;
    virtual void        SetNormalTexture(ITexture*) = 0;

    // GPU 파라미터 버퍼 — Material 자신이 소유·관리
    virtual IBuffer*              GetBuffer() const = 0;
    virtual const MaterialData&   GetData()   const = 0;

    // PBR 파라미터
    virtual void SetAlbedo   (float r, float g, float b, float a = 1.f) = 0;
    virtual void SetEmissive (float r, float g, float b) = 0;
    virtual void SetRoughness(float v) = 0;
    virtual void SetMetallic (float v) = 0;
    virtual void SetAO       (float v) = 0;

    // 렌더링 모드 세터
    virtual void SetBlendMode     (EBlendMode) = 0;
    virtual void SetCullMode      (ECullMode)  = 0;
    virtual void SetDepthWrite    (bool)       = 0;
    virtual void SetAlphaThreshold(float)      = 0;

    // Dirty flag 업로드 — GBufferRenderer·ForwardRenderer 그룹 변경 시 호출
    virtual void UpdateBufferIfDirty() = 0;

    // GPU 버퍼 초기화 — WorldObject::Initialize(IRenderDevice*) 에서 호출
    virtual void Initialize(IRenderDevice* device) = 0;
};
```

### MaterialData — GPU 레이아웃 재정의 (Phong → PBR)

```cpp
struct MaterialData  // GBuffer b1 레이아웃. HLSL GBufferMaterial 구조체와 1:1
{
    float albedo[4]       = { 1,1,1,1 };  // base color tint (RT0.RGB 곱연산)
    float emissive[3]     = { 0,0,0 };    // 자체 발광
    float roughness       = 0.5f;         // → RT1.W
    float metallic        = 0.0f;         // → RT2.R
    float ao              = 1.0f;         // → RT0.A
    float alphaThreshold  = 0.0f;         // Cutout용. GBuffer PS에서 clip()
    float _pad            = 0.0f;
    // total: 48 bytes → 256 정렬 후 CBV
};
```

### materialID 부여

sortKey의 materialID 필드는 같은 Material을 쓰는 오브젝트를 인접 배치하기 위한 키다.  
Material 생성 시 정적 카운터에서 고유 ID를 자동 할당한다.

```cpp
// Material 생성자 내부
static uint16_t s_nextID = 0;
id = s_nextID++;
```

`CalculateSortKey`는 `RenderObjectDesc.materialID` 대신 `material->GetID()`를 사용한다.  
Application 코드에서 materialID를 수동으로 설정할 필요가 없어진다.

### G-Buffer 레이아웃 매핑

```
RT0: Albedo.RGB  = albedoTex 샘플 × material.albedo tint
     Albedo.A    = material.ao
RT1: Normal.XYZ  = normalTex 샘플 → 탄젠트 공간 변환 (Phase 2 — Normal Mapping)
     Normal.W    = material.roughness
RT2: R           = material.metallic
     GBA         = reserved
```

### 파생 클래스

```
IMaterial
├─ Material         — PBR 기본 구현체. Opaque, Back cull, depthWrite on.
├─ UnlitMaterial    — albedo만 출력. 조명 계산 없음. Forward 경유.
│                     GetBlendMode() = Opaque. IsDepthWrite() = true.
├─ CutoutMaterial   — alphaThreshold로 clip(). 불투명 정렬 순서 유지.
│                     GetBlendMode() = Opaque. alphaThreshold > 0.
└─ ParticleMaterial — Additive blend. depthWrite = false.
                      Particle 시스템 구현 시 추가.
```

---

## RHI 변경 사항 (멀티 플랫폼)

### PipelineStateDesc — depthWrite 추가

```cpp
struct PipelineStateDesc {
    // 기존 필드들 ...
    bool depthEnable = false;
    bool depthWrite  = true;   // 신규. false = 투명·파티클용
};
```

| 플랫폼 | API 매핑 |
|---|---|
| DX12 | `D3D12_DEPTH_STENCIL_DESC.DepthWriteMask` → ALL / ZERO |
| Vulkan | `VkPipelineDepthStencilStateCreateInfo.depthWriteEnable` → VK_TRUE / VK_FALSE |

두 API 모두 동일 개념으로 매핑된다. RHI 수준에서 처리가 적합하다.

### Material GPU 버퍼 생성

`Material::Initialize(IRenderDevice*)` 에서 `device->CreateBuffer(...)` 호출.  
RHI 인터페이스를 통하므로 플랫폼 분기 없이 동일하게 동작한다.

---

## Barrier 처리 — 변경 불필요

Material 텍스처(albedo, normal map)는 CPU에서 Upload 후 PixelShaderResource 상태로 전환되며 이후 GPU 커맨드에 의해 상태가 변경되지 않는다.

| 리소스 | 상태 전환 | 관리 주체 |
|---|---|---|
| G-Buffer RT0/RT1/RT2 | RenderTarget ↔ ShaderResource | RenderGraph (기존과 동일) |
| Depth | DepthWrite ↔ DepthRead | RenderGraph (기존과 동일) |
| hdrOut | UnorderedAccess ↔ ShaderResource | RenderGraph (기존과 동일) |
| BonePalette | UnorderedAccess → ShaderResource | RenderGraph (기존과 동일) |
| albedo / normal 텍스처 | 변환 없음 (항상 PixelShaderResource) | 불필요 |

**결론: Material 중심 렌더링 전환으로 인한 Barrier 로직 변경 없음.**

---

## PSO 관리 전략

Material이 PSO를 소유하지 않는다. 렌더러가 `(BlendMode, CullMode, depthWrite)` 조합으로 PSO 세트를 보유한다.

### GBufferRenderer PSO 세트

| PSO | CullMode | depthWrite | 용도 |
|---|---|---|---|
| Opaque | Back | true | 기본 정적 메시 |
| DoubleSided | None | true | 양면 재질 |
| Opaque_Skinned | Back | true | 스키닝 메시 |
| DoubleSided_Skinned | None | true | 양면 스키닝 |

Cutout은 별도 PSO 불필요. 동일 Opaque PSO에서 HLSL `clip()` 처리 (단일 PS with branch, alphaThreshold=0 시 GPU 최적화).

### ForwardRenderer PSO 세트

| PSO | CullMode | BlendMode | depthWrite | 용도 |
|---|---|---|---|---|
| Transparent | Back | AlphaBlend | false | 반투명 (PBR 조명) |
| Additive | None | Additive | false | 파티클 (미래) |
| Unlit | Back | Opaque | true | 조명 없음 |
| Unlit_DoubleSided | None | Opaque | true | 양면 Unlit |

---

## 데이터 흐름 (목표)

```
WorldObject::Initialize(IRenderDevice*)
    └─ EnsureMaterial() → material->Initialize(device)   ← IBuffer 생성

WorldObject::AddToScene(RenderScene&)
    └─ scene.AddMesh(mesh, worldMatrix, material.get())
               ↓
    RenderCommand { mesh, worldMatrix, material* }
       ┌─ ITexture* texture 필드 제거
       └─ sortKey: material->GetBlendMode() → translucent bit
               ↓
    SortCommands() — materialID 기반 동일 재질 인접 배치
               ↓
    ┌─ GBufferPass (불투명·Cutout)
    │    DrawGroup 기준: (mesh, IMaterial*)
    │    그룹 변경 시:
    │      cmd.material->UpdateBufferIfDirty()
    │      cmd.material->GetBuffer() bind → b1
    │      cmd.material->GetAlbedoTexture() bind → t0
    │      cmd.material->GetCullMode() → PSO 선택
    │
    └─ TransparentPass / UnlitPass (ForwardRenderer)
         cmd.material->UpdateBufferIfDirty()
         cmd.material->GetAlbedoTexture() bind
         cmd.material->GetBlendMode() + IsDepthWrite() → PSO 선택
```

---

## 변경 대상 파일

### RHI

| 파일 | 변경 |
|---|---|
| `Engine/Public/RHI/Pipeline/PipelineDesc.h` | `bool depthWrite = true` 추가 |
| `Engine/Private/RHI/DirectX/RenderDevice_DirectX.cpp` | PSO 생성 시 DepthWriteMask 반영 |

### Material

| 파일 | 변경 |
|---|---|
| `Engine/Public/Render/IMaterial.h` | 전체 인터페이스 재정의. `Initialize(IRenderDevice*)` 추가 |
| `Engine/Private/Render/Material.h/.cpp` | PBR 파라미터, IBuffer 소유, dirty flag |

### Scene

| 파일 | 변경 |
|---|---|
| `Engine/Public/Scene/RenderScene.h` | `RenderCommand.texture` 제거. `CalculateSortKey`에서 `material->GetID()` 사용 |

### Object

| 파일 | 변경 |
|---|---|
| `Engine/Public/Object/WorldModel.h` | `unique_ptr<ITexture>` 유지 (소유권). material에 raw pointer 전달 |
| `Engine/Private/Object/WorldModel.cpp` | `Initialize`: `material->SetAlbedoTexture()`. `material->Initialize(device)` |
| `Engine/Public/Object/WorldAnimatedModel.h` | `material` 멤버 추가 |
| `Engine/Private/Object/WorldAnimatedModel.cpp` | `Initialize`: material 생성·초기화. texture 연결 |
| `Application/Source/Private/WorldObjects/WO_Cube.cpp` | `EnsureMaterial()` 이후 `material->Initialize(device)` 호출 |

### Renderer

| 파일 | 변경 |
|---|---|
| `Engine/Private/Render/GBufferRenderer.h` | DoubleSided PSO 멤버 추가 |
| `Engine/Private/Render/GBufferRenderer.cpp` | DrawGroup 기준 변경. `cmd.material` 기반 텍스처·버퍼 바인딩 |
| `Engine/Private/Render/ForwardRenderer.h` | Unlit PSO, Additive PSO 멤버 추가 |
| `Engine/Private/Render/ForwardRenderer.cpp` | PSO 세트 확장. Unlit 셰이더 초기화 |

### HLSL

| 파일 | 변경 |
|---|---|
| `GBuffer_PS.hlsl` | MaterialData → PBR 출력. Cutout: `clip()` |
| `GBufferSkinned_PS.hlsl` | 동일 |
| `DeferredLighting_CS.hlsl` | Phong → 완전 PBR (Cook-Torrance GGX) |
| `Lighting.hlsli` | PBR 조명 함수로 교체 |
| `ForwardOpaque_PS.hlsl` | PBR 파라미터 적용 |
| `ForwardUnlit_VS/PS.hlsl` | 신규. 조명 연산 없이 albedo 출력 |

---

## 구현 순서

> **주의**: Phase 3의 ⑦(texture 필드 제거)은 반드시 Phase 4의 렌더러 수정 이후에 수행한다.  
> 렌더러가 `cmd.texture`를 참조하는 상태에서 필드를 먼저 제거하면 컴파일 에러가 발생한다.

```
[완료] Phase 1 — RHI 인프라
  [x] ① PipelineStateDesc.depthWrite 추가
  [x] ② RenderDevice_DirectX DepthWriteMask 반영

[완료] Phase 2 — Material 재정의
  [x] ③ MaterialData 재정의 (Phong → PBR)
  [x] ④ IMaterial 인터페이스 전체 교체
       · UpdateBufferIfDirty() 추가
       · GetID() 추가 (Material 생성 시 정적 카운터로 고유 ID 부여)
  [x] ⑤ Material 구현체 (IBuffer 소유, dirty flag, 고유 ID)

[완료] Phase 3 — Object·Scene 연결
  [x] ⑥ WorldModel: albedoTex → material->SetAlbedoTexture(). material->Initialize(device)
  [x] ⑦ WorldAnimatedModel: material 멤버 추가, texture 연결
  [x] ⑧ WO_Cube: material->Initialize(device) 추가
  [x] ⑨ RenderScene: CalculateSortKey → material->GetID()를 materialID로 사용

[완료] Phase 4 — 렌더러 반영
  [x] ⑩ GBufferRenderer: DrawGroup 기준 (mesh, IMaterial*) 변경
                           UpdateBufferIfDirty() + GetBuffer() + GetAlbedoTexture() 바인딩
                           GetCullMode() → PSO 선택 (DoubleSided PSO 추가)
  [x] ⑪ ForwardRenderer: DrawGroup 기준 (mesh, IMaterial*) 변경
                           UpdateBufferIfDirty() + GetAlbedoTexture() 바인딩
                           GetBlendMode() → PSO 선택 (SelectForwardPSO 헬퍼)

[완료] Phase 5 — RenderCommand.texture 제거
  [x] ⑫ RenderCommand.texture 필드 제거
  [x] ⑬ AddMesh / AddSkinnedMesh 시그니처에서 ITexture* 파라미터 제거

[완료] Phase 6 — HLSL Phong → PBR 전면 교체
  [x] ⑭ GBuffer.hlsl / GBufferSkinned.hlsl: GBufferMaterial.hlsli include. Cutout clip(). PBR G-Buffer 출력
  [x] ⑮ DeferredLighting.hlsl: 완전 PBR (Cook-Torrance GGX). EvalBRDF() 사용
  [x] ⑯ ForwardOpaque.hlsl: PBR PSMain. CB_Lights에서 material 분리 (b1/b2 독립)
  [x] ⑰' Common/Transparent.hlsli + DirectX/ForwardTransparent.hlsl 신규 (PBR 투명)
       ※ ForwardUnlit 미구현 — Unlit 수요 발생 시 추가
  [x] AnimationDemo.hlsl: PBR PSMain (스키닝 포함)
  [x] Lighting.hlsli: PBR 구조체 + D_GGX · F_Schlick · G_Smith · EvalBRDF
  [x] GBufferMaterial.hlsli: 신규. CB_GBufferMaterial (b1, 48B)
  [x] 모든 렌더러 LightsBufferData MaterialData 필드 제거 (704B 정렬)

  ※ 실제 구현과 계획의 차이
  - 2-Pass 투명 렌더링(transparentBackFacePSO / transparentFrontFacePSO)은 Phase 4 확장으로 구현됨
  - PBR ambient 상수는 IBL 미구현 상태에서 0.03 → 0.10으로 상향 조정 (임시)
  - CB_Lights b2에서 material 필드 완전 분리 완료 (ForwardRenderer · AnimationRenderer · DeferredLightingRenderer)

[ ] Phase 7 — Normal Mapping (별도 작업)
  [ ] ⑱ WorldModel normalTex 추가 → material->SetNormalTexture()
  [ ] ⑲ GBuffer binding layout 확장 (t1: normal map)
  [ ] ⑳ GBuffer_PS / GBufferSkinned_PS normal map 샘플링 + TBN 행렬 계산
```

---

## 결정 완료 — 미결 사항 없음

| 항목 | 결정 |
|---|---|
| D-1 Material 버퍼 업로드 | Dirty flag. 변경 시에만 업로드. 대안 로드맵은 확정된 결정사항 섹션 참고 |
| D-2 조명 모델 범위 | 완전 PBR (Cook-Torrance GGX). Windows 전용. Android는 간소화 분리 |
| D-3 Unlit 바인딩 레이아웃 | 전용 레이아웃 (`b0 ViewProj + t0 Albedo`) |
| D-4 Cutout 분기 방식 | 단일 PS with branch. GPU 최적화 의존 |
