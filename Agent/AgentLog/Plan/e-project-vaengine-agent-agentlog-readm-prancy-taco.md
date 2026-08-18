# Plan_RenderQuality.md 개정 + Refactoring_At260711.md 신설 + 전체 구현

## 범위 변경 (사용자 확정)

최초 계획은 "문서만 갱신"으로 스코프를 좁혔으나, 사용자가 명시적으로 확장했다:
- **`.hlsl`/`.cpp` 소스코드를 Claude가 직접 편집**한다 (CLAUDE.md의 "소스코드는 사용자가 직접 작성한다" 기본 원칙을 이번 작업에 한해 사용자가 명시적으로 override).
- **"auto mode"**로 진행 — Pre-0~3, Phase 1-1~4 각 항목마다 진행 승인을 다시 묻지 않고 의존관계 순서대로 연속 구현한다.

단, 다음 제약은 그대로 유지된다 (사용자가 이 부분까지 override하지 않았고, 별도 메모리 규칙이 명시적으로 존재):
- **빌드/실행은 사용자가 직접 수행** (`cmake --build`, exe 실행) — Claude는 빌드 명령을 실행하지 않는다. 코드 수정 후 "빌드 필요" 시점을 명확히 알린다.
- **파일 이동/이름변경은 VS/Rider에서만** — 새 파일이 필요하면(예: `ShadowMap.hlsl`, `IPostProcessPass.h`) 경로를 사용자에게 안내하고 Write로 생성하되, 기존 파일의 이동/리네임은 절대 하지 않는다.
- **VaImportTool 실행 + `_Assets/`로의 수동 복사는 사용자 수행** — Normal Mapping(Phase 1-2)은 `.mesh`/`.smesh` 포맷이 바뀌므로 기존 FBX 에셋의 재-임포트가 필요한데, Converter/Exporter 코드는 Claude가 수정하지만 VaImportTool.exe 실행과 결과물 복사는 사용자가 수행해야 한다.

## Context

`Plan_RenderQuality.md`(2026-07-11 작성, 상태: 대기)는 PBR 파이프라인(Phase 1~6) 완성 후 남은 렌더링 품질 개선 로드맵(Pre-0~3 선결 수정 → Phase 1-1 HDR/Tonemap → 1-2 Normal Mapping → 1-3 Shadow → 1-4 PostProcessing)을 담은 초안이다.

이번 조사(Explore 에이전트 3개 병렬 + Plan 에이전트 1개)에서 초안 작성 시점에는 몰랐던 사실들이 다수 드러났다:
- 각 Phase 항목의 실제 영향 범위가 초안보다 넓다 (예: Normal Mapping은 Tangent가 `.mesh`/`.smesh`/절차적 메시/임포트툴 어디에도 없어 예상보다 훨씬 큰 작업)
- 새로운 버그 발견 (Transparent 패스가 Blit 이후 backBuffer에 직접 그려져 Tonemap을 건너뜀 — Phase 1-1 구현 시 회귀 유발)
- README의 일부 서술이 이미 낡음 (`WorldAnimatedModel`은 이미 Material을 보유 — "미보유"라는 README 서술은 오류)
- Plan 에이전트가 설계 과정에서 편의상 뒤로 미룬 항목들이 존재 (CB 필드 정리, `.matl` 파서, `EBindingType::Sampler` 등)

사용자 지시: **"편의를 위해 뒤로 미뤘던 작업"도 방치하지 말고 해결되길 원함.** 단, 범위 분리 기준은 사용자가 명시한 대로:
- **`Plan_RenderQuality.md`와 직접 연관된 것** → 기존 Pre-0~3 / Phase 1-1~4 섹션에 항목으로 추가
- **연관은 있지만 별도 리팩터링 성격인 것** → 신설 `Agent/AgentLog/Plan/Refactoring_At260711.md`에 기술

또한 사용자의 기존 워크플로 규칙(`feedback_improvement_workflow` 메모리)에 따라, 이후 실제 구현이 진행될 때마다 완료된 항목은 **삭제하지 않고 `~~취소선~~`으로 감싼 뒤 바로 아래에 해결 내용을 기술**하는 방식으로 문서를 갱신해 나간다. 이번 작업은 그 갱신 대상이 될 "해결 전" 최종 항목 목록을 확정하는 것이다.

이번 작업은 **두 계획 문서 갱신 + Pre-0~3/Phase 1-1~4 전체의 실제 코드 구현**을 함께 진행한다(사용자가 소스코드 직접 편집을 명시적으로 허용). 문서(산출물 1·2)를 먼저 확정한 뒤, 그 문서에 적힌 순서대로 코드를 구현하며(산출물 3), 각 항목이 완료될 때마다 `Plan_RenderQuality.md`의 해당 항목을 `~~취소선~~` 처리하고 바로 아래에 해결 내용을 기술한다.

---

## 산출물 1: `Agent/AgentLog/Plan/Plan_RenderQuality.md` 개정

기존 구조(개요 → 선결 수정 Pre-0~3 → Phase 1-1~4 → 기술 부채 트래킹 → 진행 순서 요약)를 유지하면서, 아래 내용을 반영해 전면 갱신한다.

### 신규 섹션 0. "사전 조사 발견사항" 추가 (선결 수정 앞)

Plan 에이전트 조사에서 나온 4가지 신규 발견을 요약 삽입:
1. **[중대] TransparentPass가 Blit 이후 backBuffer에 직접 그려짐** — `SceneRenderer::AddPasses`의 실제 패스 순서 확인 결과, `ForwardRenderer::AddTransparentPasses`가 `output.backBuffer`에 씀(문서/주석은 hdrOut을 전제로 작성되어 실제 구현과 불일치). Phase 1-1에서 Tonemap을 Blit에 추가하는 순간 투명 오브젝트만 톤맵을 건너뛰는 회귀가 발생 → Phase 1-1 범위에 "Transparent를 hdrOut 타겟으로 재배치" 추가.
2. Normal Map 에셋 파이프라인이 절반 완성 상태 — `.matl` export는 이미 `normal_tex=`를 기록하고 실제 에셋(`Kachujin_normal.png`)도 존재하나, 런타임 로더(`WorldModel.cpp`, `WorldAnimatedModel`)가 이를 읽지 않음.
3. Shadow Map은 `RenderGraph::DeclareTransientDepth`로 충분 — 영속 리소스 패턴(`UpdateResourceStates`) 불필요. 단, 화면과 동일 해상도/포맷을 쓰면 트랜지언트 캐시가 다른 depth와 알리아싱될 위험 → 반드시 화면과 다른 해상도(예: 2048×2048) 사용.
4. PCF에 필요한 비교 샘플러(`SamplerComparisonState`, s2) 인프라 부재 — `BindingLayout_DirectX.cpp`가 `EBindingType::Sampler` 케이스를 처리하지 않음(`default: throw`). Phase 1-3에서 s2 비교 샘플러를 추가하는 최소 구현이 선결 필요.

### Pre-0. roughness 클램프 — 내용 변경 없음 (초안 그대로 확정)

### Pre-1. Point Light 물리 감쇠 교체 — 보강

- 감쇠 계산이 `Lighting.hlsli`가 아니라 `DeferredLighting.hlsl` / `ForwardOpaque.hlsl` / `Common/Transparent.hlsli` 3곳에 중복 인라인되어 있음을 명시. 신규 공유 함수 `CalcDistanceAttenuation(dist, range)`를 `Lighting.hlsli`에 추가하고 3곳 모두 교체.
- **Spot Light도 동일 패턴으로 감쇠 중복** — Point만 고치면 두 광원의 감쇠 모델이 어긋나므로 Pre-1 범위를 Point+Spot 공통으로 확정.
- **[신규 추가 — 편의상 미루지 않고 완료]** `PointLightData`/`SpotLightData::attenuation` 필드를 C++(`ILight.h`)와 HLSL(`Lighting.hlsli`) 양쪽에서 완전히 제거하고, `IPointLight::SetAttenuation`/`ISpotLight::SetAttenuation` 인터페이스도 삭제. `VaProgramName.cpp`의 `SetAttenuation(...)` 호출부 제거(이미 `SetRange()`만으로 충분). CB 구조체 offset/padding을 재계산해 정합성 확인.

### Pre-2. Deferred Emissive 복원 — 보강

- `GBuffer.hlsl` 뿐 아니라 **`GBufferSkinned.hlsl`도 동일하게 emissive 누락**되어 있음을 명시하고 두 파일을 동시에 수정 대상에 포함(초안 누락분).

### Pre-3. DepthBuffer 포맷 파라미터화 — 보강

- "경량 Shadow 전용 경로를 새로 만들지" 검토 결과와 결론(파라미터화만으로 충분, 별도 경로 불필요 — 근거 4가지) 추가.
- Vulkan 백엔드의 `EPixelFormat`→`VkFormat` 매핑 문제는 **범위 밖으로 분리, Refactoring 문서로 이동**(아래 산출물 2 참조).

### Phase 1-1. HDR/Tonemapping — 대폭 보강

- 발견사항 1(Transparent/Blit 순서 문제)에 따른 **"Transparent 패스 재배치"를 정식 하위 작업으로 추가**: `ForwardRenderer::AddTransparentPasses`가 `hdrOut`을 RTV로 받도록 변경, `InitializeTransparent`의 `rtvFormats[0]`을 `R16G16B16A16_FLOAT`으로 변경, `SceneRenderer::AddPasses`에서 Transparent 등록을 Blit보다 앞으로 이동.
- 수정 대상 파일에 `ForwardRenderer.h/.cpp`, `SceneRenderer.cpp` 추가.
- 백버퍼가 `R8G8B8A8_UNORM`(non-sRGB) 확정 재확인 — 감마 보정 수동 필요.

### Phase 1-2. Normal Mapping — 대폭 보강 (가장 큰 변경)

- **Tangent 부재 범위 명시**: `MeshData.h::PrimitiveVertex`, `SkinnedVertex.h::SkinnedVertex` 둘 다 Tangent 없음. VaImportTool도 `aiProcess_CalcTangentSpace` 미사용.
- **절차적 메시도 대상에 포함**: `CubeShape.cpp`, `IcoSphereShape.cpp`, `UVSphereShape.cpp`가 정점을 직접 채우므로 Tangent 값을 각 도형 파라미터화에 맞게 새로 계산해야 함.
- **에셋 버전 관리**: `Exporter.cpp::ExportMesh`의 하드코딩된 `vertexStride = 48` 수정, `MESH_VERSION`(및 `.smesh` 버전) bump. **[신규 추가]** 버전 불일치 시 경고만 출력하고 계속 진행하는 현재 로직(`MeshLoader.cpp`)을 로드 실패로 강화 — Tangent 추가로 스트라이드가 바뀌는 시점이므로 지금 처리.
- **[신규 추가 — 편의상 미루지 않고 완료] Forward 경로 Normal Map**: Deferred(GBuffer)뿐 아니라 `ForwardRenderer`(Opaque/Transparent)에도 동일 Normal Map 바인딩/샘플링 적용 — 같은 기능의 반쪽만 구현하지 않음.
- **[신규 추가] `.matl` 로딩 배선(최소 구현)**: `WorldModel.cpp::ParseDiffuseTex` 옆에 `normal_tex=` 라인을 읽어 `SetNormalTexture` 호출하는 최소 파싱 추가(기존 라인스캔 패턴 재사용 수준 — 파서 자체의 구조적 리팩터링은 Refactoring 문서로 분리). `WorldAnimatedModel::Initialize`에 `matlPath` 파라미터 추가, `WO_Kachujin.cpp` 호출부 갱신.
- **Root Signature 슬롯 전략 — "append-only" 컨벤션**: 정적 GBuffer(`b0,b1,t0` → Normal Map을 t1/root3에 추가), 스키닝 GBuffer(`b0,b1,t0,t1(BonePalette)` → Normal Map을 t2/root4로 **끝에** 추가해 BonePalette의 기존 root index 3을 보존). Forward도 동일 원칙. 표로 정리해 삽입.
- **[README 정정]** `Agent/AgentLog/README.md`의 "`WorldAnimatedModel`은 아직 Material 미보유(기술 부채)" 서술 삭제 — 실제로는 이미 `unique_ptr<IMaterial>`을 보유하고 Albedo까지 바인딩 중, Normal Map만 미연결이었던 것으로 정정. 섹션 8 관련 문장 수정.
- 선결 조건 재확인: Pre-2 완료 후. Phase 1-1과는 독립(파일은 겹치나 로직 의존 없음). 범위가 가장 넓어 먼저 착수해 가장 늦게 끝나는 항목으로 배치 권장.

### Phase 1-3. Shadow 시스템 — 보강

- depth-only 리소스 생성 경로 검토 결과 반영: 별도 경량 클래스 신설 안 함, Pre-3 파라미터화로 해결(근거 4가지 명시).
- `RenderGraph::DeclareTransientDepth` 사용, 해상도를 화면과 다르게(2048×2048) 잡아 캐시 alias 방지.
- **[신규 추가 — 편의상 미루지 않고 완료] PCF용 비교 샘플러 최소 구현**: `BindingLayout_DirectX.cpp`에 `hasTexture`일 때 s0(linear-wrap) + s2(comparison, `D3D12_COMPARISON_FUNC_LESS_EQUAL`) 정적 샘플러를 함께 추가하는 실용적 구현. (`EBindingType::Sampler`의 범용 커스터마이징 자체는 Refactoring 문서로 분리 — 이번 라운드는 Shadow가 실제로 필요로 하는 s2 하나만 배선)
- `DeferredLightingRenderer` 바인딩에 shadowDepth SRV + 광원 공간 행렬 CB를 append 전략(기존 root0~6 다음, root7/8)으로 추가.
- 재검증된 독립성: Phase 1-1/1-2와 완전 독립, 병렬 진행 가능. 단 `DeferredLighting.hlsl`을 Pre-2와 동시에 건드리므로 병합 충돌 주의 문구 추가.

### Phase 1-4. PostProcessing — 보강

- **SSAO 선결조건 정정**: 초안의 "Phase 1-2(Normal Mapping) 이후"는 부정확 — SSAO에 필요한 Normal+Depth는 이미 지금 시점에 GBuffer RT1+Depth로 존재. 정정: SSAO는 Phase 1-2와 무관하게 이미 가능하나, **구조적으로 Bloom과 다른 위치**(GBufferPass와 DeferredLightingPass **사이**의 별도 `SSAOPass`)에 넣어야 물리적으로 올바름. 이 설계를 Phase 1-4 하위 섹션으로 명시.
- Ping-Pong RT 관리를 위한 `IPostProcessPass` 인터페이스 초안 유지, Bloom 구현 순서 명시.

### 진행 순서 요약 갱신

Plan 에이전트가 재검증한 의존관계 반영:
```
Pre-0, Pre-1, Pre-3 → 병렬/독립
Pre-2 → Phase 1-1·1-2 착수 전 필수 공통 선결
Phase 1-2 (범위 최대) → 가장 먼저 착수, 가장 늦게 완료 (다른 Phase 착수를 막지 않음)
Phase 1-3 → Pre-3 이후, 1-1/1-2와 독립적으로 병행 가능
Phase 1-1 → Pre-2 이후, 1-2와 독립
Phase 1-4 → Phase 1-1(Tonemap+Transparent 재배치) 완료 후
```

### 기술 부채 트래킹 표 갱신

- `~~WorldAnimatedModel Material 지원~~` 취소선 처리 + "이미 구현되어 있음이 확인됨(2026-07-11 조사) — Normal Map 미연결만 남음, Phase 1-2로 흡수" 기술.
- CSM(Stage B), Point/Spot Light Shadow, Transparent shadow lookup, IBL, OIT, G_Smith k 분리 — **기존 그대로 유지**(이번 라운드에서 편의상 미룬 것이 아니라 원래부터 별도 기능 단위이므로 범위 변경 없음).
- 문서 상단에 취소선 워크플로 안내 문구 1줄 추가: "완료된 항목은 삭제하지 않고 `~~취소선~~` 처리 후 바로 아래에 해결 내용을 기술한다."

---

## 산출물 2: `Agent/AgentLog/Plan/Refactoring_At260711.md` 신규 작성

Plan_RenderQuality.md와 연관되어 발견되었지만, 그 자체는 렌더링 품질이 아니라 **엔진 인프라/구조 정리**에 해당하는 항목을 모아 별도 문서로 분리한다. 각 항목은 어떤 조사에서 왜 발견됐는지 출처를 `Plan_RenderQuality.md`의 관련 섹션으로 역참조한다.

### 항목 1. Vulkan `EPixelFormat` ↔ `VkFormat` 매핑 부재
- 출처: Pre-3 조사. `EPixelFormat`이 DXGI_FORMAT 숫자를 그대로 미러링하는 설계라 DX12에서는 동작하지만, Vulkan 백엔드(`Common_Vulkan.h`, 현재 스텁)가 실제 구현될 때 별도 매핑 테이블이 필요.
- 지금 처리하지 않는 이유: 현재 Vulkan 백엔드 자체가 스텁 상태라 실질적 영향 없음. Vulkan 렌더러 착수 시점에 처리.

### 항목 2. `EBindingType::Sampler` 범용화
- 출처: Phase 1-3 조사. `BindingLayout_DirectX.cpp`의 switch문에 `EBindingType::Sampler` 케이스가 없어 `default: throw`. Shadow PCF는 s2 하드코딩 추가로 최소 대응(Plan_RenderQuality.md Phase 1-3에 포함)하지만, PSO마다 임의의 샘플러(필터 모드, 비교 함수 등)를 선언 가능하게 하는 일반화는 별도 작업.
- 지금 처리하지 않는 이유: 현재 필요한 샘플러는 s0(linear-wrap)+s2(shadow comparison) 2종으로 고정 가능해 일반화 없이도 Phase 1-3 완료 가능. 세 번째 커스텀 샘플러 요구가 생기는 시점(예: Anisotropic 옵션, Clamp 샘플러 등)에 일반화.

### 항목 3. `.matl` 파서를 key-value 파서로 리팩터링
- 출처: Phase 1-2 조사. `WorldModel.cpp::ParseDiffuseTex`가 `diffuse_tex=`만 읽는 임시 라인 스캔 방식. Phase 1-2에서 `normal_tex=`도 같은 패턴으로 복붙 추가(최소 구현)하지만, 향후 `specular_tex=`/`emissive_tex=` 등이 늘어나면 라인마다 복붙이 반복되어 중복이 커짐.
- 지금 처리하지 않는 이유: 현재 필요한 키가 2개(diffuse, normal)뿐이라 일반 파서 도입의 이득이 크지 않음. 3번째 텍스처 슬롯이 추가되는 시점에 일반화.

각 항목은 "왜 지금 안 하는지"에 대한 근거를 명시해, 단순 방치가 아니라 **판단에 따른 유예**임을 문서화한다(CLAUDE.md "기술 부채는 구현 전에 고지" 원칙 반영).

---

## 산출물 3: 코드 구현 (auto mode, 의존관계 순서)

문서(산출물 1·2) 확정 후 아래 순서로 연속 구현한다. 각 단계 끝에 "빌드 필요" 지점과 "사용자 수동 작업 필요" 지점을 명시한다.

### 1. Pre-0 — roughness 클램프
`Engine/_Shaders/Common/Lighting.hlsli` D_GGX 1줄 수정.

### 2. Pre-1 — Point/Spot Light 감쇠 교체 + CB 필드 정리
- `Lighting.hlsli`에 `CalcDistanceAttenuation(dist, range)` 추가, `PointLightData`/`SpotLightData`에서 `attenuation` 필드 완전 제거.
- `DeferredLighting.hlsl`, `ForwardOpaque.hlsl`, `Common/Transparent.hlsli` 3곳의 Point+Spot 인라인 감쇠를 새 함수 호출로 교체.
- `Engine/Public/Render/ILight.h`(또는 해당 데이터 구조체 정의 위치), `IPointLight`/`ISpotLight`의 `SetAttenuation` 인터페이스 제거.
- `Application/Source/.../VaProgramName.cpp`의 `SetAttenuation(...)` 호출 삭제.
- CB 레이아웃이 줄어드는 변경이므로 C++/HLSL 양쪽 struct offset을 함께 확인.

### 3. Pre-3 — DepthBuffer 포맷 파라미터화
- `Engine/Public/RHI/Common_RHI.h`에 `D32_FLOAT` 추가.
- `Engine/Private/RHI/DirectX/DepthBuffer_DirectX.cpp`에 포맷 매핑 헬퍼 추가, dead parameter 버그 수정.

### 4. Pre-2 — Deferred Emissive 복원
`GBuffer.hlsl`, `GBufferSkinned.hlsl`의 RT2 출력에 emissive 추가, `DeferredLighting.hlsl` CSMain에서 `.gba` 읽어 최종 출력에 가산.

**→ 이 시점에서 1차 빌드 권장** (Pre-0~3 전체가 회귀 없이 동작하는지 확인 후 Phase 1로 진행).

### 5. Phase 1-2 — Normal Mapping (가장 넓은 범위)
1. `MeshData.h::PrimitiveVertex`, `SkinnedVertex.h::SkinnedVertex`에 Tangent(`float4`, xyz+handedness) 필드 추가.
2. `MESH_VERSION`(`MeshAsset.h`) 및 `.smesh` 버전 상수 bump. `MeshLoader.cpp`/`SkmLoader.cpp`가 버전 불일치 시 예외를 던지도록 강화.
3. `Tools/ImportTool/Converter.cpp`에 `aiProcess_CalcTangentSpace` 추가, `mTangents`/`mBitangents`로 handedness 계산해 정점에 기록.
4. `Tools/ImportTool/Exporter.cpp::ExportMesh`의 하드코딩된 stride를 `sizeof(PrimitiveVertex)`/`sizeof(SkinnedVertex)`로 교체.
5. `CubeShape.cpp`/`IcoSphereShape.cpp`/`UVSphereShape.cpp`에 각 도형 UV 파라미터화 기준 Tangent 계산 추가.
6. `GBuffer.hlsl`/`GBufferSkinned.hlsl` VSMain에 TANGENT 입력 추가, PSMain에 TBN 구성 + Normal Map 샘플링 → RT1.XYZ.
7. `ForwardOpaque.hlsl`/`Common/Transparent.hlsli`에도 동일 Normal Map 적용(Forward 경로).
8. `GBufferRenderer.cpp/.h`, `ForwardRenderer.cpp/.h`의 BindingLayout에 Normal Map 슬롯을 **append 전략**으로 추가(기존 root index 불변 확인), `defaultNormalTexture`(1×1 tangent-up) 추가.
9. `WorldModel.cpp`에 `normal_tex=` 파싱 + `SetNormalTexture` 호출 추가. `WorldAnimatedModel.h/.cpp`에 `matlPath` 파라미터 추가. `WO_Kachujin.cpp` 호출부 갱신.
10. `Agent/AgentLog/README.md`의 WorldAnimatedModel Material 서술 정정.

**→ 사용자 수동 작업 필요**: 정점 포맷/스트라이드가 바뀌므로 VaImportTool 재빌드 후 기존 FBX(Kachujin 등)를 재-임포트하고 `_Assets/`로 결과물을 수동 복사해야 함 — 이 단계는 Claude가 대행하지 않고 시점을 명확히 안내한다.

### 6. Phase 1-1 — HDR/Tonemapping + Transparent 재배치
- `Blit.hlsl`에 ACES Filmic Tonemap + Gamma + Exposure CB 추가.
- `ForwardRenderer::AddTransparentPasses`/`InitializeTransparent`가 `backBuffer` 대신 `hdrOut`(RTV, `R16G16B16A16_FLOAT`)에 그리도록 변경.
- `SceneRenderer::AddPasses`에서 Transparent 등록을 Blit보다 앞으로 재배치.

### 7. Phase 1-3 — Shadow 시스템 (Stage A, 단일 Shadow Map)
1. `BindingLayout_DirectX.cpp`에 비교 정적 샘플러(s2, `D3D12_COMPARISON_FUNC_LESS_EQUAL`) 추가.
2. 신규 `Engine/_Shaders/DirectX/ShadowMap.hlsl`(Depth Only VS) 작성.
3. `SceneRenderer::AddPasses`에 ShadowMapPass 추가(GBufferPass 앞), `graph.DeclareTransientDepth({2048,2048,D24_UNORM_S8_UINT})` 사용.
4. `DeferredLightingRenderer`에 shadowDepth SRV + 광원 공간 행렬 CB를 append(root7/8)로 추가, CSMain에 PCF 샘플링 구현.
5. `Common/Sampler.hlsli`의 `ShadowSampler` 주석 해제.

### 8. Phase 1-4 — PostProcessing (Bloom)
1. `Engine/Public/Render/IPostProcessPass.h` 신규 인터페이스.
2. `Bloom_BrightPass.hlsl`/`Bloom_Blur.hlsl`/`Bloom_Composite.hlsl` 신규 셰이더.
3. `SceneRenderer.cpp`에 PostProcess 체인 등록, Ping-Pong RT 관리.
4. Tonemap 로직을 `Blit.hlsl`에서 분리해 별도 `Tonemap.hlsl`로 이동, 체인 마지막 단계로 배치.

**→ 최종 빌드 권장** (Phase 1 전체 완료 후 육안 검증).

각 단계 완료 시 `Plan_RenderQuality.md`의 해당 Pre/Phase 섹션 제목을 `~~취소선~~` 처리하고, 바로 아래에 "완료(날짜) — 실제 수정 파일, 특이사항" 형식으로 기술한다.

---

## Critical Files

문서:
- `E:\Project\VaEngine\Agent\AgentLog\Plan\Plan_RenderQuality.md` — 전면 개정 + 진행에 따라 취소선 갱신
- `E:\Project\VaEngine\Agent\AgentLog\Plan\Refactoring_At260711.md` — 신규 생성
- `E:\Project\VaEngine\Agent\AgentLog\README.md` — WorldAnimatedModel Material 서술 정정 (섹션 8 부근)

코드(전체 목록은 산출물 3 각 단계 참조, 핵심만 요약):
- `Engine/_Shaders/Common/Lighting.hlsli`, `Sampler.hlsli` — Pre-0/1, Phase 1-3
- `Engine/_Shaders/DirectX/GBuffer.hlsl`, `GBufferSkinned.hlsl`, `DeferredLighting.hlsl`, `ForwardOpaque.hlsl`, `Common/Transparent.hlsli`, `Blit.hlsl` — Pre-1/2, Phase 1-1/1-2/1-3
- `Engine/Public/RHI/Common_RHI.h`, `Engine/Private/RHI/DirectX/DepthBuffer_DirectX.cpp`, `Pipeline/BindingLayout_DirectX.cpp` — Pre-3, Phase 1-3
- `Engine/Public/Mesh/MeshData.h`, `SkinnedVertex.h`, `Engine/Private/Asset/MeshLoader.cpp`, `SkmLoader.cpp`, `Engine/Public/Asset/MeshAsset.h` — Phase 1-2 에셋 포맷
- `Tools/ImportTool/Converter.cpp`, `Exporter.cpp` — Phase 1-2 임포트툴
- `Engine/Private/Mesh/CubeShape.cpp`, `IcoSphereShape.cpp`, `UVSphereShape.cpp` — Phase 1-2 절차적 메시
- `Engine/Private/Render/GBufferRenderer.cpp/.h`, `ForwardRenderer.cpp/.h`, `SceneRenderer.cpp`, `DeferredLightingRenderer.cpp/.h` — Phase 1-1/1-2/1-3/1-4 렌더러 오케스트레이션
- `Engine/Private/Object/WorldModel.cpp`, `Engine/Public/Object/WorldAnimatedModel.h/.cpp`, `Application/Source/.../WO_Kachujin.cpp` — Phase 1-2 머티리얼 로딩

## 실행 시 유의사항

- 문서(산출물 1·2) 확정 → 코드 구현(산출물 3) 순서로 진행하며, 코드 구현은 auto mode로 각 항목 완료 시 확인 없이 다음 항목으로 진행한다.
- 완료된 Pre/Phase 항목은 `Plan_RenderQuality.md`에서 `~~취소선~~` 처리 후 해결 내용을 바로 아래에 기술한다(삭제 금지).
- **빌드는 사용자가 직접 실행** — Pre-0~3 완료 시점, Phase 1 전체 완료 시점 등 "빌드 필요" 지점마다 명확히 안내하고, Claude가 임의로 `cmake --build`를 실행하지 않는다.
- **VaImportTool 실행 + 에셋 재-임포트/수동 복사는 사용자 수행** — Phase 1-2에서 정점 포맷이 바뀌는 시점에 안내한다.
- 새 파일(`ShadowMap.hlsl`, `IPostProcessPass.h`, `Bloom_*.hlsl` 등)은 Write로 생성 가능하나, 기존 파일의 이동/리네임은 하지 않는다(VS/Rider 전용).

## Verification

- 코드 변경은 `.hlsl`/`.cpp` 컴파일 가능 여부까지만 Claude가 눈으로 재확인하고, 실제 빌드/실행/육안 렌더 검증은 사용자가 수행(안내 문구 포함).
- `Plan_RenderQuality.md`/`Refactoring_At260711.md` 상호 참조 정합성 확인.
- `README.md` 수정 문장이 문서의 다른 서술과 모순되지 않는지 확인.
- Phase 1-2(Normal Mapping)는 에셋 재-임포트 전까지 빌드는 되어도 시각적 확인이 불가능하므로, 재-임포트 완료 여부를 별도로 사용자에게 확인받는다.
