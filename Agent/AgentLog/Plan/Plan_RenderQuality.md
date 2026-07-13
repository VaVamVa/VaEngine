# 렌더링 품질 개선 계획서 (RenderQuality)

작성일: 2026-07-11
개정일: 2026-07-11 (Explore×3 + Plan 에이전트 조사 반영, 전체 구현 착수)
상태: **완료 — 빌드·에셋 재-임포트·육안 검증·로그 검증 전부 완료(2026-07-12)**
참조: `2026-07-11_Log.md > Phase 1`, `PhongToPBR_Migration.md > 섹션 8·9`, `Refactoring_At260711.md`, `Refactoring_RenderGraph-Buffer.md`

**검증 완료(2026-07-12)**:
1. 전체 빌드 후 실행 확인 — Normal Map, Tonemapping 육안 검증 완료(사용자 확인).
2. VaImportTool 재빌드 + 기존 FBX(Kachujin 등) 재-임포트 완료 — Phase 1-2(Normal Mapping)가 정상 표시되는 것으로 재-임포트가 정상 반영되었음을 확인.
3. `BaseRHIResource` 리팩터링(아래 "추가로 확인된 문제" 절, 상세는 `Refactoring_RenderGraph-Buffer.md` 참조) — 백버퍼 Present 전환·BonePalette 첫 프레임 UAV 쓰기·Shadow Map 프레임 간 재사용 3가지 모두 실행 로그(`_Files/Log/2026-07-12_06-17-01.log`)로 회귀 없이 정상 동작함을 확인.

---

## 개요

PBR 파이프라인(Phase 1~6) 완성 후 남은 개선 사항과 Phase 1 기능 추가를 순서대로 이행한다.
각 단계의 의존성을 고려해 **선결 수정 → Phase 1-1~4** 순으로 진행한다.

**진행 기록 규칙**: 완료된 항목은 삭제하지 않고 `~~취소선~~`으로 감싼 뒤, 해당 항목 바로 아래에 "완료(날짜) — 실제 수정 파일, 특이사항"을 기술한다. 기술 부채로 남기는 항목은 "왜 지금 하지 않는지"를 반드시 명시한다.

---

## 0. 사전 조사 발견사항 (2026-07-11 조사)

초안 작성 이후 Explore 에이전트 3개 병렬 조사(셰이더/RHI/메시-머티리얼) + Plan 에이전트 설계 과정에서 드러난, 초안에 없던 사실 4가지.

### 0-1. [중대] TransparentPass가 Blit 이후 backBuffer에 직접 그려짐

`SceneRenderer::AddPasses`의 실제 패스 순서를 확인한 결과, `ForwardRenderer::AddTransparentPasses`(TransparentPass)가 `DeferredBlitPass` **이후** `output.backBuffer`에 직접 그려지고 있다. `Blit.hlsl`/`ITextureUAV.h`의 주석은 "Transparent가 hdrOut에 그려짐"을 전제로 작성되어 있어 문서와 실제 구현이 어긋난 상태다.

현재는 Opaque/Transparent 둘 다 톤맵 없이 raw 값을 UNORM RT에 하드웨어 클램프하므로 문제가 드러나지 않지만, Phase 1-1에서 Blit에 Tonemap을 추가하는 순간 **투명 오브젝트만 톤맵을 건너뛰어 밝기/색감이 달라지는 회귀**가 발생한다. → Phase 1-1 범위에 "Transparent를 hdrOut 타겟으로 재배치" 포함.

### 0-2. Normal Map 에셋 파이프라인이 절반 완성 상태

`Tools/ImportTool/Converter.cpp::ExtractMaterials`는 이미 `aiTextureType_NORMALS`/`HEIGHT`를 추출하고, `Exporter.cpp`는 `.matl`에 `normal_tex=`를 기록한다. `_Assets/Kachujin/Kachujin.matl`에 `normal_tex=Kachujin_normal.png`가 이미 존재하고 텍스처 파일도 있다. 그러나 런타임 로더(`WorldModel.cpp::ParseDiffuseTex`)는 `diffuse_tex=`만 읽고, `WorldAnimatedModel::Initialize`는 애초에 `matlPath` 파라미터가 없어 `.matl`을 거치지 않는다(`WO_Kachujin.cpp`가 텍스처 경로를 직접 하드코딩). → Normal Mapping(Phase 1-2)의 텍스처 로딩 배선은 상대적으로 가벼운 작업이고, Tangent(버텍스 포맷+임포트툴+셰이더)가 실제 무게중심이다.

### 0-3. Shadow Map은 `RenderGraph::DeclareTransientDepth`로 충분

`DeclareTransientDepth(desc)`는 동일 `{width,height,format}`이면 프레임 간 리소스를 재사용(캐싱)하고 매 `Compile()`마다 `DepthWrite` 상태로 리셋한다. Shadow Map처럼 한 프레임 안에서 쓰고 읽고 끝나는 리소스는 GBuffer/hdrOut처럼 `SceneRenderer`가 멤버 변수로 상태를 들고 다니는 영속 리소스 패턴(`UpdateResourceStates`)이 필요 없다.

**주의**: `DeclareTransientDepth`는 `{width,height,format}`만으로 캐시를 매칭하므로, Shadow Map 해상도를 화면 해상도(1280×720)·포맷과 동일하게 잡으면 다른 트랜지언트 depth와 의도치 않게 물리 리소스를 공유(alias)한다. Shadow Map은 반드시 화면과 다른 해상도(예: 2048×2048)를 사용한다.

### 0-4. PCF에 필요한 비교 샘플러 인프라 부재

`Sampler.hlsli`에 `// TODO: Depth Buffer 구현 후 활성화` 주석과 함께 `SamplerComparisonState ShadowSampler : register(s2)`가 비활성 상태로 자리만 잡혀 있다. 그런데 `BindingLayout_DirectX.cpp::Create`는 `EBindingType::Sampler` 케이스가 switch문에 없어 `default: throw`로 떨어지고(사실상 미구현), 텍스처 바인딩이 있으면 s0 linear-wrap 샘플러 1개만 항상 추가하는 구조라 s2 비교 샘플러를 넣을 방법이 없다. → Phase 1-3에서 s2 비교 샘플러를 추가하는 최소 구현이 선결 필요(범용 `EBindingType::Sampler` 일반화는 `Refactoring_At260711.md`로 분리).

---

## 선결 수정 (Phase 1 이행 전 처리)

### ~~Pre-0. roughness 최솟값 클램프~~

**완료(2026-07-11)** — `Lighting.hlsli::D_GGX`의 `a = roughness*roughness`를 `a = max(roughness*roughness, 0.0001f)`로 수정.

| | |
|---|---|
| **파일** | `Engine/_Shaders/Common/Lighting.hlsli` |
| **수정** | `float a = roughness * roughness;` → `float a = max(roughness * roughness, 0.0001f);` |
| **이유** | roughness=0 설정 시 a=a2=0, D_GGX=0 → specular 완전 소멸. 거울면 재질 표현 불가. |
| **영향** | Lighting.hlsli 1줄. 셰이더 재컴파일만. |

### ~~Pre-1. Point/Spot Light 물리 감쇠 교체 + CB 필드 정리~~

**완료(2026-07-11)** — `Lighting.hlsli`에 `CalcDistanceAttenuation(dist,range)` 추가, `DeferredLighting.hlsl`/`ForwardOpaque.hlsl`/`Transparent.hlsli`/`AnimationDemo.hlsl`(초안에 없던 4번째 사용처, 추가 발견) 모두 교체. `PointLightData`/`SpotLightData::attenuation` 필드를 `ILight.h`/`Lighting.hlsli`에서 완전히 제거하고 `SetAttenuation` 인터페이스·구현(`Light.h`)·호출부(`VaProgramName.cpp`)까지 삭제.

| | |
|---|---|
| **파일** | `Engine/_Shaders/Common/Lighting.hlsli`, `Engine/_Shaders/DirectX/DeferredLighting.hlsl`, `Engine/_Shaders/DirectX/ForwardOpaque.hlsl`, `Engine/_Shaders/Common/Transparent.hlsli` |
| **현재** | 감쇠 계산(`1.0f/dot(attenuation, float3(1,dist,dist*dist))`)이 `Lighting.hlsli`가 아니라 **3개 셰이더 파일에 Point+Spot 각각 인라인 중복**되어 있음. 전용 함수 없음. |
| **변경** | `Lighting.hlsli`에 `float CalcDistanceAttenuation(float dist, float range)` 신설(역제곱+windowing, `1/max(dist*dist,eps) * saturate(1-(dist/range)^4)^2`). 3개 파일의 Point+Spot 인라인 코드를 함수 호출로 교체. |
| **[미루지 않고 포함] CB 필드 정리** | `PointLightData`/`SpotLightData::attenuation` 필드를 C++(`ILight.h` 등 데이터 구조체 정의부)와 HLSL(`Lighting.hlsli`) 양쪽에서 완전히 제거. `IPointLight::SetAttenuation`/`ISpotLight::SetAttenuation` 인터페이스 삭제. `VaProgramName.cpp`의 `SetAttenuation(...)` 호출 삭제(이미 `SetRange()`로 충분). CB struct offset/padding 재확인. |
| **영향** | 셰이더 4개 파일 + C++ 광원 인터페이스/데이터 구조체 + 호출부 1곳. |

### ~~Pre-2. Deferred Emissive 복원~~

**완료(2026-07-11)** — `GBuffer.hlsl`/`GBufferSkinned.hlsl` PSMain의 RT2 출력을 `float4(gMetallic, gEmissive)`로 변경. `DeferredLighting.hlsl` CSMain에서 `materialBuf.gba`를 emissive로 읽어 `outHDR = Lo + ambient + emissive`로 수정.

| | |
|---|---|
| **파일** | `Engine/_Shaders/DirectX/GBuffer.hlsl`, `Engine/_Shaders/DirectX/GBufferSkinned.hlsl`, `Engine/_Shaders/DirectX/DeferredLighting.hlsl` |
| **현재** | RT2 = `Metallic.R (GBA 예약)`. **정적 GBuffer뿐 아니라 스키닝 GBuffer(`GBufferSkinned.hlsl`)도 동일하게 emissive 누락**(초안 누락분). `DeferredLighting`이 emissive를 읽지 않음. |
| **변경** | RT2 = `Metallic.R + Emissive.GBA`. 두 GBuffer 셰이더 PSMain에서 `o.rt2 = float4(gMetallic, gEmissive.rgb)` 출력. `DeferredLighting` CSMain에서 `emissive = rt2.gba` 읽어 `Lo + ambient + emissive`로 출력. |
| **이유** | Forward 경로(ForwardOpaque, Transparent)는 emissive 정상 적용 중. Deferred 경로(정적+스키닝 둘 다)만 누락. 버그. |
| **한계** | R8G8B8A8_UNORM 8bit Emissive는 HDR 범위 표현 불가. 강한 emissive의 Bloom 반영은 Phase 1-4에서 별도 검토. |

### ~~Pre-3. DepthBuffer 포맷 파라미터화~~

**완료(2026-07-11)** — `Common_RHI.h`에 `EPixelFormat::D32_FLOAT = 40` 추가. `DepthBuffer_DirectX.cpp`에 `GetDepthFormatTriplet(DXGI_FORMAT)` 헬퍼를 추가해 `Create`가 요청 포맷에 따라 resource/DSV/SRV triplet을 결정하도록 수정(기존 D24 경로는 default case로 그대로 유지되어 회귀 없음).

| | |
|---|---|
| **파일** | `Engine/Public/RHI/Common_RHI.h`, `Engine/Private/RHI/DirectX/DepthBuffer_DirectX.cpp` |
| **현재** | `EPixelFormat`에 `D24_UNORM_S8_UINT`만 있고 `D32_FLOAT` 없음. `DepthBuffer_DirectX::Create`는 `format` 파라미터를 받지만 함수 본문에서 전혀 사용하지 않는 dead parameter — `R24G8_TYPELESS`/`D24_UNORM_S8_UINT`/`R24_UNORM_X8_TYPELESS`로 하드코딩. |
| **변경** | `EPixelFormat::D32_FLOAT = 40` 추가. `Create` 내부에 포맷 매핑 헬퍼(`GetDepthFormatTriplet`)를 추가해 `format` 인자에 따라 resource/DSV/SRV triplet을 결정. |
| **경량 Shadow 전용 경로 신설 여부 — 신설하지 않음** | `IDepthBuffer`가 `GetView`/`GetReadOnlyView`/`BindSRV` 3종을 계약으로 요구하므로 별도 클래스를 만들어도 DSV+ReadOnlyDSV+SRV 생성(~90줄)이 중복될 뿐이다. ReadOnlyDSV를 안 쓰더라도 생성 비용은 초기화 1회·디스크립터 1개로 무시할 수준. `BindSRV`가 이미 전역 SRV Heap을 통해 검증된 패턴이므로 Shadow Map도 동일 경로를 타는 것이 안전하다. |
| **주의** | Vulkan은 `Common_Vulkan.h` 스텁 상태 — `EPixelFormat`→`VkFormat` 매핑은 `Refactoring_At260711.md` 항목 1로 분리(지금 처리 안 함, 이유 명시). |

---

## ~~Phase 1-1. HDR / Tonemapping (+ Transparent 패스 재배치)~~

**완료(2026-07-11)** — `Blit.hlsl`에 ACES Filmic(Narkowicz 근사) + Exposure CB(b0) + 수동 감마(`pow(1/2.2)`) 추가. `ForwardRenderer::TransparentPass`가 `output.backBuffer` 대신 `hdrOut`(RTV)에 그리도록 재배치(§0-1 버그 수정), `InitializeTransparent`의 `rtvFormats`를 `R16G16B16A16_FLOAT`으로 변경. `SceneRenderer::AddPasses`에서 Transparent 등록을 Blit보다 앞으로 이동. `tonemapBuffer`(SceneRenderer 소유)를 append 전략으로 root 1에 배선.

## Phase 1-1. HDR / Tonemapping (+ Transparent 패스 재배치) (원안)

### 목표

`hdrOut`(R16G16B16A16_FLOAT)의 HDR 값을 LDR 백버퍼로 톤맵+감마 보정하여 변환하고, §0-1에서 발견한 Transparent/Blit 순서 문제를 함께 해결한다.

### 현재 상태

`Blit.hlsl`(27줄)은 `gHDR.Sample(...)`을 그대로 반환 — 톤맵/감마 없음. 백버퍼 포맷은 `Execute.cpp`에서 `EPixelFormat::R8G8B8A8_UNORM`(non-sRGB, GPU 자동 감마 변환 없음). `TransparentPass`는 `DeferredBlitPass` 이후 `backBuffer`에 직접 그려짐(§0-1).

### 구현 방향

**(A) Tonemap + Gamma를 Blit.hlsl에 추가**: ACES Filmic 권장, Exposure 파라미터는 CB로 노출. 최종 출력 후 `pow(color, 1/2.2)` 감마 보정(백버퍼가 non-sRGB이므로 수동 필요).

**(B) 패스 순서 재배치**:
```
변경 후: DeferredSkyPass → BonePalette → GBufferPass → DeferredLightingPass(hdrOut)
          → TransparentPass(hdrOut, RTV)   ← Blit보다 앞으로 이동
          → DeferredBlitPass(hdrOut→backBuffer, Tonemap+Gamma)  ← 마지막 1회
          → DebugLinePass → DebugTextPass (backBuffer, 변경 없음)
```
- `ForwardRenderer::AddTransparentPasses`/`TransparentPass`가 `output.backBuffer` 대신 `hdrOut`(`ITextureUAV*`)의 RTV에 그리도록 변경. `DeclareResources`도 `hdrOut`을 RenderTarget으로 선언.
- `ForwardRenderer::InitializeTransparent`의 `rtvFormats[0]`을 `R8G8B8A8_UNORM` → `R16G16B16A16_FLOAT`으로 변경(hdrOut과 포맷 일치, DX12 validation 요구사항).
- `SceneRenderer::AddPasses`에서 Transparent 등록을 Blit 등록보다 앞으로 이동.
- Depth 공유(read-only DSV)는 변경 없음.

### 영향 범위

| 파일 | 변경 |
|---|---|
| `Engine/_Shaders/DirectX/Blit.hlsl` | Tonemap + Gamma 로직, Exposure CB 추가 |
| `Engine/Private/Render/ForwardRenderer.h/.cpp` | Transparent 타겟을 hdrOut으로, rtvFormats 변경 |
| `Engine/Private/Render/SceneRenderer.cpp` | 패스 등록 순서 변경(Transparent → Blit) |

### 선결 조건

Pre-2 완료 후(emissive 포함 결과로 톤맵 튜닝). Pre-0/1/3과 독립. Phase 1-2와도 독립(파일은 겹치나 로직 의존 없음).

### 기술 부채

Phase 1-4(PostProcessing) 구현 시 Tonemap을 Blit에서 분리해 PostProcess 체인 마지막 단계로 이동. Transparent를 hdrOut으로 옮기면 Bloom이 투명 오브젝트의 밝은 픽셀도 포착하는 부수 효과 있음(긍정적).

---

## ~~Phase 1-2. Normal Mapping~~

**완료(2026-07-11)** — 계획대로 전체 구현:
- 정점 포맷: `MeshData.h::PrimitiveVertex`(48→64B), `SkinnedVertex.h::SkinnedVertex`(80→96B)에 `tangent[4]` 추가. `MESH_VERSION`/`SKM_VERSION` 2로 bump, `MeshLoader`/`SkmLoader` 버전 불일치 시 로드 실패로 강화.
- 절차적 메시: `PrimitiveShape::ComputeTangents`(Lengyel 알고리즘, 신규 `PrimitiveShape.cpp`) 공유 헬퍼로 Cube/IcoSphere/UVSphere 전부 적용.
- VaImportTool: `aiProcess_CalcTangentSpace` 추가, `Converter.cpp::ExtractTangent`로 handedness 계산, `Exporter.cpp`/`SkmExporter.cpp`의 하드코딩 stride를 `sizeof(...)`로 교체.
- 셰이더: `GBuffer.hlsl`/`GBufferSkinned.hlsl`/`ForwardOpaque.hlsl`/`Transparent.hlsli` 전부 TANGENT 입력 + TBN + Normal Map 샘플링 반영(AnimationDemo.hlsl은 실제 Draw가 호출되지 않는 죽은 경로로 확인되어 제외).
- RHI: `GBufferRenderer`/`ForwardRenderer` 바인딩에 append-only로 Normal Map 슬롯 추가(정적 root3, 스키닝 root4·BonePalette root3 불변, Forward root4), `defaultNormalTexture`(1×1 tangent-up) 폴백 추가.
- 머티리얼 로딩: `WorldModel.cpp`/`WorldAnimatedModel.cpp`에 `normal_tex=` 파싱 추가(`ParseTexKey` 최소 구현), `WorldAnimatedModel::Initialize` 파라미터를 `texturePath`→`matlPath`로 변경, `WO_Kachujin.cpp`가 `Kachujin.matl` 경유하도록 갱신.
- `CLAUDE.md`의 WorldAnimatedModel Material 서술 정정(원래 오류 출처가 README.md가 아니라 CLAUDE.md였음, 위 정정 사항 참조).

**⚠ 사용자 확인 필요**: 정점 스트라이드가 바뀌었으므로 VaImportTool을 재빌드하고 기존 FBX(Kachujin 등)를 재-임포트해 `_Assets/`에 재복사해야 시각적으로 검증 가능합니다. 이 작업은 자동화되지 않았습니다.

## Phase 1-2. Normal Mapping (원안)

### 목표

메시의 Tangent로 Normal Map에서 per-pixel 법선을 읽어 GBuffer RT1에 기록. **정적 + 스키닝 + Forward(Opaque/Transparent) 전부** 적용(Deferred만 반쪽 구현하지 않음).

### 현재 상태 (범위가 초안보다 훨씬 넓음)

- `MeshData.h::PrimitiveVertex`(48B), `SkinnedVertex.h::SkinnedVertex`(80B) 모두 **Tangent 필드 없음**.
- VaImportTool(`Converter.cpp`)은 `aiProcess_CalcTangentSpace` 미사용, `mTangents` 미접근.
- **절차적 메시도 대상**: `CubeShape.cpp`/`IcoSphereShape.cpp`/`UVSphereShape.cpp`가 정점을 직접 채우므로 Tangent를 도형별로 새로 계산해야 함.
- `Exporter.cpp::ExportMesh`에 `vertexStride = 48` 하드코딩 — Tangent 추가 시 `sizeof(PrimitiveVertex)`로 교체 필요.
- `MESH_VERSION`(`MeshAsset.h`) 불일치 시 `MeshLoader.cpp`가 **경고만 출력하고 계속 진행** — 스트라이드가 바뀌므로 위험. **[미루지 않고 포함]** 버전 불일치 시 로드 실패로 강화.
- `IMaterial`/`Material`에는 이미 `GetNormalTexture`/`SetNormalTexture`가 존재(§0-2)하나 실사용 코드 없음.
- `GBuffer.hlsl` PS는 `gDiffuse`(t0)만 샘플링, Normal Map 샘플링 없음. RT1은 정점 보간 노멀 그대로 출력.

### 정정 사항 — WorldAnimatedModel Material 보유 확인

**정정(2026-07-11 재확인)**: 이 "오래된 정보"는 `Agent/AgentLog/README.md`가 아니라 **`CLAUDE.md`**(프로젝트 루트, Material 시스템 섹션 128~131행)에 있었음 — 최초 조사 시 출처를 README로 잘못 특정했던 것을 정정. 실제로 `WorldAnimatedModel`은 이미 `unique_ptr<IMaterial> material` 멤버를 갖고(`WorldAnimatedModel.h`), `GBufferRenderer::RenderGBuffer`가 스키닝 메시의 Albedo를 정상 바인딩한다. 남은 것은 **Normal Map 미연결**뿐. → `CLAUDE.md` 정정(완료), 기술 부채 표에서 해당 항목 취소선 처리.

### 구현 방향

**TBN 구성**:
```
VSMain: Tangent(float4, xyz+handedness) 입력 추가 → PSMain으로 전달
PSMain: T = normalize(mul(float4(tangent.xyz,0), world).xyz)
        B = normalize(cross(N, T) * tangent.w)
        TBN = float3x3(T, B, N)
        normalWorld = normalize(mul(TBN, normalMap*2-1))
```
Normal Map 없는 머티리얼 fallback: 1×1 `(128,128,255)` 기본 텍스처(`defaultNormalTexture`)로 분기 없이 처리 — `GBufferRenderer`의 기존 `defaultTexture`(1×1 white albedo)와 동일 패턴.

**Root Signature 슬롯 전략 — "append-only" 컨벤션**: root parameter index는 `BindingEntry[]` 배열의 위치와 1:1이며, 레지스터 번호는 배열 위치와 독립적이다. 새 슬롯은 항상 각 배열의 **끝에** 추가한다.

| 파이프라인 | 기존 순서 | 추가 후 |
|---|---|---|
| 정적 GBuffer | b0,b1,t0(albedo) → root 0,1,2 | +t1(Normal) → root **3**(신규) |
| 스키닝 GBuffer | b0,b1,t0(albedo),t1(BonePalette) → root 0,1,2,**3** | +t2(Normal) → root **4**(신규, BonePalette의 root 3 불변) |
| Forward(Opaque/Transparent) | b0,b1,b2,t0(albedo) → root 0,1,2,**3** | +t1(Normal) → root **4**(신규, albedo의 root 3 불변) |

이 전략으로 `GBufferRenderer.cpp`의 `SetGraphicsSRV(..., 3)`, `ForwardRenderer.cpp`의 `Bind(cmdList, 3)` 등 기존 하드코딩된 root index 호출부를 **전혀 수정할 필요가 없다**.

### 영향 범위

| 구분 | 파일 |
|---|---|
| 정점 포맷 | `Engine/Public/Mesh/MeshData.h`, `SkinnedVertex.h` |
| 절차적 메시 | `Engine/Private/Mesh/CubeShape.cpp`, `IcoSphereShape.cpp`, `UVSphereShape.cpp` |
| 임포트툴 | `Tools/ImportTool/Converter.cpp`(`aiProcess_CalcTangentSpace`, `mTangents`/`mBitangents`), `Exporter.cpp`(stride 수정) |
| 에셋 버전 | `Engine/Public/Asset/MeshAsset.h`(`MESH_VERSION` bump), `.smesh` 버전 상수, `MeshLoader.cpp`/`SkmLoader.cpp`(불일치 시 실패 처리) |
| GBuffer 셰이더 | `Engine/_Shaders/DirectX/GBuffer.hlsl`, `GBufferSkinned.hlsl` |
| Forward 셰이더 | `Engine/_Shaders/DirectX/ForwardOpaque.hlsl`, `Engine/_Shaders/Common/Transparent.hlsli` |
| 바인딩/PSO | `Engine/Private/Render/GBufferRenderer.cpp/.h`, `ForwardRenderer.cpp/.h`(TANGENT VertexInputDesc, Normal Map append 슬롯, defaultNormalTexture) |
| 머티리얼 로딩 | `Engine/Private/Object/WorldModel.cpp`(`normal_tex=` 파싱 + `SetNormalTexture`), `Engine/Public/Object/WorldAnimatedModel.h/.cpp`(`matlPath` 파라미터) |
| 애플리케이션 | `Application/Source/Private/WorldObjects/WO_Kachujin.cpp`(matlPath 전달) |
| 문서 | `CLAUDE.md`(WorldAnimatedModel Material 서술 정정 — README.md 아님, 위 정정 사항 참조) |

### 구현 순서

1. 정점 포맷에 Tangent 추가 → 버전 bump
2. 임포트툴 Tangent 추출 → **사용자가 VaImportTool 재빌드 후 기존 FBX 재-임포트 + `_Assets/`로 수동 복사** (Claude 대행 불가 지점)
3. 절차적 메시 Tangent 계산
4. GBuffer 셰이더(정적→스키닝) → Forward 셰이더
5. RHI 바인딩(append 전략)
6. 머티리얼 로딩 배선(WorldModel → WorldAnimatedModel → WO_Kachujin)
7. CLAUDE.md 정정
8. 셰이더 재컴파일 후 육안 검증(재-임포트 완료 후에만 가능)

### 선결 조건

Pre-2 완료 후(GBuffer RT 레이아웃 충돌 방지). Phase 1-1과 독립. **범위가 Phase 1 중 가장 넓어 가장 먼저 착수, 가장 늦게 완료되는 항목으로 배치**(다른 Phase 착수를 막지는 않음).

### 기술 부채

- 법선 XY 인코딩(hemisphere encoding)으로 RT1 채널 절약 — 별도 최적화 과제.
- `.matl` 파서 일반화(key-value) — `Refactoring_At260711.md` 항목 3으로 분리.

---

## ~~Phase 1-3. Shadow 시스템~~

**완료(2026-07-11, Stage A)** — 신규 `ShadowMapRenderer.h/.cpp`(GBufferRenderer 등과 동일한 Initialize/AddPasses 패턴)가 `RenderGraph::DeclareTransientDepth(2048×2048)`로 화면과 다른 해상도의 트랜지언트 depth를 확보(§0-3 alias 방지). Directional Light 방향으로 `Matrix4x4::LookAtLH` + 신규 `Matrix4x4::OrthographicLH`(Container.h/cpp에 추가) 조합해 광원 공간 view*proj 계산(고정 박스 근사, 씬 바운드 추적 없음). 신규 `ShadowMap.hlsl`(Depth Only VS + 빈 PS — RHI가 VS+PS 쌍을 요구해 PS는 트리비얼 빈 함수로 충족)과 CMakeLists.txt 컴파일 규칙 추가. `BindingLayout_DirectX.cpp`에 s2 비교 정적 샘플러(`D3D12_COMPARISON_FUNC_LESS_EQUAL`) 추가, `Sampler.hlsli`의 `ShadowSampler` 주석 해제. `DeferredLightingRenderer`에 shadowDepth SRV(root7)+광원행렬 CB(root8) append, `DeferredLighting.hlsl`에 3×3 PCF(`SampleCmpLevelZero`) 추가해 Directional Light 항에만 적용.

**범위 제한(고지)**: 정적 불투명 메시만 shadow caster로 사용 — 스키닝 메시(WorldAnimatedModel) shadow casting은 미구현. 아래 기술 부채 표에 추가.

**런타임 크래시 수정(2026-07-11, 빌드 후 실행 중 발견)**: `RenderGraph::Compile()`이 트랜지언트 depth(`DeclareTransientDepth`)를 매 프레임 무조건 `EResourceState::DepthWrite`로 재설정하던 버그. `DeferredLightingPass`가 프레임 끝에 Shadow Map을 `NonPixelShaderResource`(SRV)로 읽고 끝나면, 다음 프레임 `ShadowMapPass`가 실제로는 SRV 상태인 리소스에 `ClearDepthStencilView`를 호출해 D3D12 디바이스 행(exception 0x87A, `INVALID_SUBRESOURCE_STATE` #538)을 일으켰다. 이 트랜지언트 depth 메커니즘은 이전엔 죽은 코드 경로(`ForwardRenderer::AddOpaquePasses`)에서만 선언되어 한 번도 실행 검증된 적 없던 기존 설계 결함 — Shadow Map이 실제 실행 경로에서 처음 노출시킴. `RenderGraph.h`의 `TransientDepthEntry`에 `lastKnownState` 필드를 추가하고, `RenderGraph.cpp::Compile()`이 프레임 시작 시 이 값으로 상태를 시작하고 프레임 끝에 갱신하도록 수정(`Engine/Public/Render/RenderGraph.h`, `Engine/Private/Render/RenderGraph.cpp`).

## Phase 1-3. Shadow 시스템 (원안)

### 목표

Directional Light 기반 Shadow Map(Stage A: 단일) → 추후 CSM(Stage B).

### depth-only 리소스 생성 경로 — Pre-3 파라미터화로 해결, 별도 경로 신설 안 함

근거는 Pre-3 항목 참조. Stage A는 `D24_UNORM_S8_UINT` 재사용(코드 변경 0)으로 시작하고, 정밀도 이슈 발생 시 `D32_FLOAT`으로 전환(Pre-3에서 이미 인프라 마련).

### 구현 방향

```
Stage A: 단일 Shadow Map
  ShadowMapPass (GBufferPass 앞, Depth Only, Directional Light 기준 Orthographic)
    - RenderGraph::DeclareTransientDepth({2048, 2048, D24_UNORM_S8_UINT}) — §0-3
    - 화면 해상도(1280×720)와 다르게 잡아 트랜지언트 캐시 alias 방지
  DeferredLightingPass (Compute)
    - shadowDepth SRV를 append 전략으로 root 7에 추가
    - 광원 공간 행렬 CB를 root 8에 추가
    - PCF 샘플링(2×2 또는 3×3)
```

**[미루지 않고 포함] PCF용 비교 샘플러 최소 구현(§0-4)**: `BindingLayout_DirectX.cpp`에서 `hasTexture`일 때 s0(linear-wrap) + s2(comparison, `D3D12_COMPARISON_FUNC_LESS_EQUAL`) 정적 샘플러를 함께 추가하는 실용적 구현. `Sampler.hlsli`의 `ShadowSampler` 주석 해제. (`EBindingType::Sampler`의 범용 커스터마이징은 `Refactoring_At260711.md` 항목 2로 분리 — Shadow가 실제로 필요로 하는 s2 하나만 배선)

### 영향 범위

| 구분 | 파일 |
|---|---|
| 신규 셰이더 | `Engine/_Shaders/DirectX/ShadowMap.hlsl`(Depth Only VS) |
| Lighting 통합 | `Engine/_Shaders/DirectX/DeferredLighting.hlsl`(shadowDepth SRV + PCF), `Engine/_Shaders/Common/Sampler.hlsli` |
| RHI | `Engine/Private/RHI/DirectX/Pipeline/BindingLayout_DirectX.cpp`(s2 비교 정적 샘플러) |
| 렌더러 | `Engine/Private/Render/DeferredLightingRenderer.cpp/.h`(바인딩 append, 광원 행렬 CB) |
| 오케스트레이션 | `Engine/Private/Render/SceneRenderer.cpp`(ShadowMapPass를 GBufferPass 앞에 추가) |
| 광원 정보 | `Engine/Public/Scene/RenderScene.h` — `scene.GetLighting().dirLight.direction`로 이미 접근 가능, 변경 불필요 |

### 구현 순서

1. Pre-3 완료 확인 → 2. BindingLayout s2 샘플러 추가 → 3. `ShadowMap.hlsl` 작성 → 4. `SceneRenderer`에 ShadowMapPass 등록 → 5. `DeferredLightingRenderer`에 shadowDepth SRV + 광원 행렬 CB append, PCF 구현 → 6. Directional Light 방향 기준 Orthographic 투영(씬 바운드 근사치로 시작) → 7. 그림자 테스트 씬 육안 검증

### 선결 조건

Pre-3 이후 필수. Phase 1-1/1-2와 완전 독립, 병렬 진행 가능. 단 `DeferredLighting.hlsl`을 Pre-2(emissive)와 동시에 건드리므로 병합 충돌 주의.

### 기술 부채

- Transparent 패스는 shadow 미적용(Forward) — 추후 별도 shadow lookup 추가 가능.
- Point Light / Spot Light Shadow(큐브맵/스팟 ShadowMap) — 별도 과제.
- ~~CSM(Stage B) — `Create()` 시그니처 확장 또는 별도 array 경로 필요, 지금 만들지 않음.~~ → 완료(2026-07-13, `2026-07-13_Log.md > Compact Log #1~9` — 구현은 #1, 버그 3종 발견·수정은 #3/#7/#9).

---

## ~~Phase 1-4. PostProcessing 파이프라인~~

**완료(2026-07-11, Bloom만)** — 신규 `BloomRenderer.h/.cpp`: `Bloom_BrightPass.hlsl`(soft threshold) → `Bloom_BlurH.hlsl`/`Bloom_BlurV.hlsl`(9-tap 가우시안, half-res 핑퐁 `bloomA`/`bloomB`) → `Bloom_Composite.hlsl`(`EBlendMode::Additive`로 hdrOut에 직접 가산). `SceneRenderer::AddPasses`에서 Transparent 다음·Blit 이전에 배치. bloomA/bloomB는 GBuffer RT와 동일한 영속 리소스 패턴(`ImportResource`+`UpdateResourceStates`)으로 프레임 간 상태 추적.

**원안 대비 2가지 의도적 축소(근거 명시)**:
1. **`IPostProcessPass` 인터페이스 도입 안 함** — 현재 PostProcess 효과가 Bloom 하나뿐이라 다형성이 실제로 쓰이지 않음(CLAUDE.md: "다형성이 실제로 쓰이는지 확인, 사용되지 않는 추상화는 추가하지 않는다"). `BloomRenderer`를 GBufferRenderer 등과 동일한 Initialize/AddPasses 패턴의 구체 클래스로 직접 구현. 두 번째 PostProcess 효과(예: SSAO를 이 체인에 넣기로 결정하는 시점, 또는 MotionBlur 등)가 생기면 그때 인터페이스로 일반화.
2. **Tonemap을 별도 `Tonemap.hlsl`로 분리하지 않음** — Phase 1-1에서 이미 `Blit.hlsl`에 통합 구현했고 소비자가 Blit 하나뿐이라 지금 분리해도 얻는 이득이 없음(파일만 나뉘는 순수 이동). Bloom Composite가 hdrOut에 직접 가산하므로 Blit이 최종적으로 Tonemap하는 시점엔 이미 Bloom이 반영된 값을 읽어 결과적으로 "Bloom이 Tonemap보다 먼저" 순서는 원안대로 보장됨.

**SSAO는 구현하지 않음(설계만 유지)** — 원안의 "구현 방향"에도 SSAO 관련 실제 구현 대상 파일이 명시되어 있지 않았고("정정" 섹션은 설계 방향 재확인일 뿐), Pre-Lighting 패스로 넣으려면 `DeferredLightingRenderer`에 AO SRV를 또 append하고 half/quarter-res SSAO 텍스처 생성 인프라(노이즈 텍스처, 커널 샘플, 뷰공간 재구성)가 추가로 필요해 Bloom과 별개의 작업량. 아래 기술 부채 표에 등록.

## Phase 1-4. PostProcessing 파이프라인 (원안)

### 목표

Tonemap 이전에 HDR RT를 입력받는 PostProcess 패스 체인 구성. 첫 구현: Bloom.

### 구현 방향

```
DeferredLightingPass(hdrOut) → TransparentPass(hdrOut, Phase 1-1에서 재배치됨)
  → [PostProcess Chain] → BloomPass (bright-pass + Gaussian blur, hdrOut 기준)
  → TonemapPass (Phase 1-1에서 Blit에 임시 통합한 것을 여기로 이동)
  → BlitPass (backBuffer 출력)
```

### SSAO 정정 — Normal Mapping 선행 불필요, 단 위치는 별도

초안의 "SSAO는 Phase 1-2 이후"는 부정확. SSAO에 필요한 Normal+Depth는 이미 GBuffer RT1+Depth로 존재(Phase 1-2는 RT1의 normal *값을 정밀화*할 뿐 RT1 자체를 새로 만들지 않음). SSAO는 Phase 1-2와 무관하게 이미 가능하다.

다만 **위치는 Bloom과 다르다**: SSAO는 간접광(ambient) 감쇠 값이므로 `DeferredLightingPass`가 `Lo+ambient`를 계산하기 **이전**에 필요하다. → `SSAOPass`(GBuffer normal+depth 읽어 AO 텍스처 생성)를 `GBufferPass`와 `DeferredLightingPass` **사이**에 삽입하고, `DeferredLightingRenderer`가 AO SRV를 append(root 9 등)로 추가해 `ambient *= ssaoFactor`로 반영하는 것이 정확한 설계. PostProcess 체인(hdrOut 이후)에 넣는 것은 이미 합성된 직접광까지 어둡게 만드는 근사이므로 권장하지 않음.

### IPostProcessPass 인터페이스

```
IPostProcessPass:
  virtual void AddToGraph(RenderGraph&, ITextureUAV* hdrIn, ITextureUAV* hdrOut) = 0;
```

`SceneRenderer`가 패스 목록을 갖고 순서대로 `AddToGraph` 호출. Ping-Pong RT 2장 필요.

### 영향 범위

- 신규: `Engine/Public/Render/IPostProcessPass.h`, `Engine/_Shaders/DirectX/Bloom_BrightPass.hlsl`, `Bloom_Blur.hlsl`, `Bloom_Composite.hlsl`
- `Engine/Private/Render/SceneRenderer.cpp/.h` — PostProcess 체인 등록, Ping-Pong RT 관리
- `Engine/_Shaders/DirectX/Blit.hlsl` — Tonemap 분리, 별도 `Tonemap.hlsl`로 이동

### 선결 조건

Phase 1-1의 Tonemap 통합 + Transparent 재배치 완료 후(hdrOut이 Opaque+Transparent를 모두 포함해야 Bloom이 올바르게 동작). SSAO는 Phase 1-2와 무관하게 가능하나 GBuffer를 동시에 건드리는 협업 충돌 방지 차원에서 Phase 1-2 이후 착수 권장.

### 기술 부채

- PostProcess 패스마다 Ping-Pong RT 필요 — RT 할당/재사용 전략 확정 필요.
- SSAO를 Pre-Lighting 패스로 분리 시 `DeferredLightingRenderer` 바인딩에 AO SRV append 필요(Shadow Map root 7~8과 마찬가지 원칙).

---

## ~~추가로 확인된 문제 (런타임 디버깅 중 발견, 2026-07-11)~~

**모두 해결됨(2026-07-12) → 최종 설계로 대체 완료(2026-07-12)** — 1차로 `RegisterPersistentResource`/`UnregisterPersistentResource` 도입으로 문제 1(설계 결함)과 문제 2(기존 버그 2건)를 해결했으나, 구현 착수 직전 재검토 과정에서 실제 Unreal Engine RDG 소스(`FRHIViewableResource::TrackedAccess`)를 검증한 결과 더 근본적인 설계(상태를 리소스 객체 자신이 멤버로 소유)가 있음을 확인해 **`RegisterPersistentResource` 자체를 다시 제거하고 `BaseRHIResource`로 완전히 대체 구현**했다. 상세 설계·grep 검증·구현 목록은 `Plan/Refactoring_RenderGraph-Buffer.md` 참조. 아래는 1차 원인 조사·설계 검토 기록을 그대로 보존하고, 맨 아래에 최종 결과를 별도 기술한다.

Phase 1-3 빌드 후 실행 중 GPU 디바이스 행(exception 0x87A)이 발생해 원인을 추적하는 과정에서, 개별 버그 하나를 고치는 것을 넘어 **`RenderGraph`의 리소스 상태 추적 설계 자체**에 구조적 결함이 있음이 드러났다. 사용자가 "Depth Buffer가 추가될 때마다 하드코딩을 반복해야 하는 구조 아니냐"고 지적했고, 조사 결과 실제로 그랬다.

### ~~문제 1. 리소스 상태 추적이 이원화되어 있고, 매 리소스마다 수동 반복이 강제됨~~

**해결됨(2026-07-12)** — 아래 "제안하는 해결 설계" 절의 `RegisterPersistentResource`/`UnregisterPersistentResource` 도입으로 통합. 문제 서술은 원인 기록으로 아래에 그대로 남긴다.

현재 두 가지 서로 다른 메커니즘이 병존한다:
1. `RenderGraph`의 트랜지언트 depth 캐시(`DeclareTransientDepth`/`GetTransientDepth`) — 그래프가 직접 상태를 추적.
2. 리소스를 소유한 클래스(`SceneRenderer`, `BloomRenderer`)가 `EResourceState` 멤버 변수를 직접 들고 다니며 `ImportResource`(프레임 시작)+`GetCurrentState`(프레임 끝)를 매번 수동으로 짝지어 호출하는 방식. `SceneRenderer`에 `rt0State`/`rt1State`/`rt2State`/`hdrState`/`depthState` 5개, `BloomRenderer`에 `bloomAState`/`bloomBState` 2개 — 새 영속 리소스가 추가될 때마다 이 보일러플레이트(멤버 선언 + Import 호출 + UpdateResourceStates 호출 + 상위로의 체인 연결)가 그대로 반복된다.

CLAUDE.md 원칙("RenderGraph = 메커니즘, SceneRenderer = 정책")에 비춰보면, 리소스가 지금 어떤 상태인지 아는 것은 배리어 계산의 일부이므로 메커니즘(RenderGraph) 책임인데, 현재는 정책 계층이 직접 상태를 들고 다니는 구조라 원칙이 깨져 있다.

### ~~문제 2. 위 패턴을 빠뜨린 실제 버그가 이미 2건 확인됨~~

**해결됨(2026-07-12)** — 아래 표 두 건 모두 해결 완료.

| # | 위치 | 증상 | 상태 |
|---|---|---|---|
| 1 | `ShadowMapRenderer`(신규) → `DeferredLightingPass`가 shadow depth를 `NonPixelShaderResource`로 읽고 프레임을 끝냄 | `RenderGraph::Compile()`이 트랜지언트 depth를 매 프레임 무조건 `DepthWrite`로 재설정 → 다음 프레임 `ClearDepthStencilView`가 실제로는 SRV 상태인 리소스에 호출됨 → GPU 디바이스 행(0x87A) | ~~해결됨~~ — `TransientDepthEntry`에 `lastKnownState` 필드 추가, 프레임 종료 시 실제 상태를 저장해 다음 프레임에 이어받도록 수정 완료(위 Phase 1-3 절 참조) |
| 2 | `Engine/Private/Render/AnimationRenderer.cpp:230` — `graph.ImportResource(m->GetBonePaletteBuffer(), EResourceState::UnorderedAccess)` | 매 프레임 무조건 `UnorderedAccess`로 강제 리셋하지만, `GBufferRenderer`가 같은 프레임에 이 버퍼를 `NonPixelShaderResource`로 읽고 끝남 — 문제 1과 완전히 동일한 패턴. 이번 세션 변경분이 아니라 **기존에 이미 있던 버그**(백그라운드 조사 에이전트가 발견) | ~~미해결~~ — `graph.RegisterPersistentResource(m->GetBonePaletteBuffer(), EResourceState::UnorderedAccess)`로 교체해 해결(아래 참조) |

### ~~제안하는 해결 설계 (사용자 승인 대기)~~

**완료(2026-07-12) → 이후 아래 "최종 해결 설계"로 대체됨** — 구현 전 사용자가 "이 방법이 최선인지, Register 비용이 비싸지 않은지"를 먼저 판단하라고 요구해 재검토한 뒤 아래 설계로 1차 확정·구현했으나, 이 설계 자체가 최종 형태는 아니었다(아래 참조). 조사·판단 기록으로 원문 그대로 보존한다.

**비용 판단**: `persistentResourceStates.try_emplace(...)`는 포인터 키 해시맵 조회 O(1)로, 리소스 8~10개 기준 프레임당 나노초 단위 — 실질적으로 무시 가능. 다만 "왜 매 프레임 같은 정보를 반복 등록하는가"라는 지적은 타당해 아래처럼 반영했다.

**최종 설계**:
```cpp
// 최초 등록 시에만 defaultInitialState 사용 — 이후 호출은 idempotent(무해). 그래프가 직전 프레임
// 종료 시점의 실제 상태를 스스로 기억해 사용한다.
void RegisterPersistentResource(IRHIResource* resource, EResourceState defaultInitialState);

// 리소스가 파괴되고 재생성될 때(예: 화면 크기 변경) 이전 포인터의 추적을 제거.
// 파괴 직전 호출 → 새 리소스 생성 → 다음 프레임 RegisterPersistentResource가 새 포인터를
// 최초 등록으로 인식해 defaultInitialState부터 다시 추적한다. 화면 크기 동적 대응(Phase 2 ToDo) 대비.
void UnregisterPersistentResource(IRHIResource* resource);
```

- **등록 호출 위치**: 처음엔 "생성 시점(Initialize)에 1회만 호출"을 검토했으나, 이렇게 하려면 `RenderGraph&`를 `GBufferRenderer`/`BloomRenderer`뿐 아니라 Application 계층의 `WorldAnimatedModel`/`SkinnedMesh`(BonePalette 생성 지점)까지 관통시켜야 해 Engine/Application 경계를 건드리는 과한 변경이 된다. `RegisterPersistentResource`가 `try_emplace` 기반이라 매 프레임 호출해도 두 번째 호출부터는 완전히 무해(상태를 덮어쓰지 않음)하므로, **기존에 이미 `RenderGraph&`를 매 프레임 받는 위치**(`SceneRenderer::AddPasses`, `BloomRenderer::AddPasses`, `AnimationRenderer::AddComputePasses`)에서 `ImportResource` 호출을 그대로 `RegisterPersistentResource`로 바꾸는 것으로 충분 — 시그니처 변경 없이 동일한 "사실상 1회 등록" 효과를 얻는다.
- **안전장치(신규)**: `Compile()`의 `transition()`에서 `DeclareResources`가 참조하는 리소스가 `resourceStates`에 없으면(= 등록 누락) `assert`로 즉시 실패하도록 추가. 등록을 빠뜨리면 예전처럼 조용히 배리어가 생략되고 몇 달 뒤 우연히 발견되는 대신, 첫 프레임에 바로 크래시해 원인이 명확해진다. Release 빌드에서는 `NDEBUG`로 자동 제거되어 비용 없음.
- 기존 `ImportResource(resource, state)`는 그대로 유지 — 스왑체인 백버퍼처럼 `Execute.cpp`가 그래프 밖에서 수동 배리어(Present 전환)로 상태를 관리하는 예외 케이스와 분리하기 위함. `TransientDepthEntry::lastKnownState`(Shadow Map 수정 때 추가)도 별도 필드로 유지 — 그래프가 직접 생성/캐시하는 리소스(desc 기반)와 외부 소유 리소스는 생명주기가 달라 하나로 억지 통합하지 않음.

**수정 파일**: `Engine/Public/Render/RenderGraph.h`(`RegisterPersistentResource`/`UnregisterPersistentResource`/`persistentResourceStates` 추가), `Engine/Private/Render/RenderGraph.cpp`(`Compile()` 시드/저장 로직 + assert), `Engine/Private/Render/SceneRenderer.h/.cpp`(RT0/RT1/RT2/hdrOut/depth — 5개 멤버+`UpdateResourceStates()` 삭제), `Engine/Private/Render/BloomRenderer.h/.cpp`(bloomA/bloomB — 2개 멤버+메서드 삭제), `Engine/Private/Render/AnimationRenderer.cpp`(BonePalette 버퍼 — 문제 2 버그 동시 해결), `Engine/Private/Execute.cpp`(`sceneRenderer.UpdateResourceStates()` 호출 제거).

### 최종 해결 설계 — `BaseRHIResource` 리소스 상태 캡슐화 (2026-07-12)

**완료(2026-07-12)** — 위 `RegisterPersistentResource` 설계를 사용자가 재검토를 요구("이 리소스가 그래프 관리 대상인지 판단해야 하는 게 자동 배리어의 의미를 퇴색시킨다")하는 과정에서, 실제 Unreal Engine RDG 소스(`FRHIViewableResource::TrackedAccess`)를 검증해 "상태를 리소스 객체 자신이 멤버로 소유"하는 더 근본적인 설계로 전면 대체했다. 전체 설계 확정 과정·A/B/C 대안 비교·grep 기반 API 삭제 가능성 검증은 `Plan/Refactoring_RenderGraph-Buffer.md`에 별도 문서로 기록했다(계획서 → 구현 순서로 진행하는 이번 문서 체계의 규칙상, 여기서는 결과만 요약).

- **`IRHIResource` → `BaseRHIResource`로 개명**(`I`=순수 인터페이스, `Base`=공통 구현 포함 규칙에 따라 — 상태 필드+구현을 갖게 되며 더 이상 순수 인터페이스가 아니게 됨). 죽은 코드였던 `TRHIResource<T>` 템플릿 삭제.
- `EResourceState GetTrackedState() const`(public) / `void SetTrackedState(EResourceState)`(`protected` + `friend class RenderGraph`) 추가 — 리소스가 자기 상태를 직접 소유. `ForceTrackedState()` 같은 public 이스케이프 해치는 만들지 않음(아래 참조).
- `RenderGraph`의 `resourceStates`/`persistentResourceStates` 맵, `RegisterPersistentResource`/`UnregisterPersistentResource`/`ImportResource`/`GetCurrentState` **4개 API 전부 삭제**(grep으로 살아있는 호출부 0개 확인 후 삭제) — `Compile()`이 리소스 포인터에서 직접 상태를 읽고 갱신.
- **백버퍼 Present 전환도 그래프 Pass로 편입**(`PresentTransitionPass`, 신규) — `Execute.cpp`가 그래프 밖에서 수동 배리어를 걸던 방식을 없애고, 빈 `Execute()` + `{backBuffer, Present}` 요구만 갖는 트리비얼 Pass를 그래프 마지막에 추가. `Compile()`이 다른 모든 리소스와 동일한 경로로 자동 계산해 "이 리소스가 그래프 관리 대상인지" 호출부가 판단할 필요가 완전히 사라짐.
- **기존 버그 수정**: `Buffer_DirectX`가 만드는 UAV 버퍼(BonePalette 등)는 D3D12가 버퍼에 한해 InitialState를 무시하고 항상 `COMMON`으로 생성하는데, 지금까지 `UnorderedAccess`로 잘못 가정하고 있었다 — 5개 구체 구현체(`ColorBuffer_DirectX`→RenderTarget, `DepthBuffer_DirectX`→DepthWrite, `TextureUAV_DirectX`→UnorderedAccess, `Buffer_DirectX`→Common, `SwapChain_DirectX::BackBufferResource`→Common) 모두 `Create()` 직후 실제 상태를 정확히 설정하도록 수정하며 함께 바로잡음.
- **후속 개선(같은 날)**: `IDepthBuffer`가 `IColorBuffer`/`ITextureUAV`/`IBuffer`와 달리 `BaseRHIResource`를 합성(`GetResource()` 접근자)으로만 참조하던 비대칭을 해소 — `IDepthBuffer : public BaseRHIResource`로 직접 상속하도록 변경, `DepthBuffer_DirectX`의 중첩 `DepthResource` 합성 구조체 제거, 9개 호출부의 `depthBuffer->GetResource()`를 `depthBuffer`로 단순화. `SwapChain_DirectX`/`BackBufferResource`는 1:N(스왑체인 1개-백버퍼 N개) 관계라 의미상 맞지 않아 합성 유지.
- **수정 파일**(요약, 전체 목록은 `Refactoring_RenderGraph-Buffer.md`): `BaseRHIResource.h`(신규, `IRHIResource.h` 대체) · `RenderGraph.h/.cpp`(단순화) · `PresentTransitionPass.h`(신규) · `Execute.cpp`/`SceneRenderer.cpp`/`BloomRenderer.cpp`/`AnimationRenderer.cpp`(호출부 정리) · `IBuffer.h`/`IColorBuffer.h`/`ITextureUAV.h`/`IDepthBuffer.h`/`IResourceView.h`/`ICommandList.h`/`ISwapChain.h`/`IRenderPass.h` 및 DirectX 구현체 다수(타입 치환).

---

## 기술 부채 트래킹 (Phase 1 범위 외)

| 항목 | 우선순위 | 시점 |
|---|---|---|
| ~~G_Smith IBL k 분리 (`k_direct` / `k_ibl` 파라미터화)~~ | — | 완료(2026-07-12) — `Lighting.hlsli`의 `G_Smith(NdotV,NdotL,k)`가 k를 직접 받도록 변경, 기존 direct 공식은 `G_Smith_Direct(NdotV,NdotL,roughness)` 래퍼로 분리해 `EvalBRDF`가 사용. `G_Smith_IBL`은 호출부가 생기는 IBL 구현 시점에 함께 추가 예정(현재는 미사용 코드라 추가 안 함). 순수 리팩터링이라 direct 조명 결과값 변화 없음 |
| IBL — ~~Diffuse Irradiance~~ 완료(2026-07-12) / Specular Prefiltered + BRDF LUT 미구현 | 높음 | Diffuse: `IBLRenderer` 신규(컬렉션 관리형, Light Probe로 재사용 가능하도록 소스 파라미터화) + `IrradianceConvolve.hlsl`(1회성 `ImmediateSubmit` 프리컴퓨트) + `DeferredLighting.hlsl` root9(t5) append. 상세: `2026-07-11_Log.md > Compact Log #2`. Specular는 `ITextureUAV`에 실제 mip 체인을 추가하는 작업과 함께 별도 착수(배열 슬라이스 우회 안 함, 결정 근거 상동 로그 참조) |
| OIT (Order-Independent Transparency) | 낮음 | 볼록 오브젝트 이상의 투명 필요 시 |
| ~~WorldAnimatedModel Material 지원~~ | — | 이미 구현되어 있음이 확인됨(2026-07-11 조사) — `unique_ptr<IMaterial>` 보유, Albedo 바인딩 정상. Normal Map도 Phase 1-2 구현으로 연결 완료 |
| ~~CSM(Cascaded Shadow Maps)~~ | — | 완료(2026-07-13) — Texture2DArray Depth(최대 8캐스케이드 항상 할당, 활성 개수 1~8 런타임 조정 Num4/5, 기본 4) + smooth blend. 상세: `2026-07-13_Log.md > Compact Log #1~9`(구현 #1, 버퍼 재사용 타이밍 버그 #3, 행렬 row/column 의미론 버그 #7, HLSL cbuffer 스칼라 배열 패킹 버그 #9), 설계 근거: `2026-07-13_Q&A.md`. 드로우콜 캐스케이드 수배·캐스케이드별 컬링 없음·텍셀 스냅핑 미적용은 후속 기술 부채로 별도 유지 |
| Point/Spot Light Shadow | 낮음 | 필요 시 |
| ~~스키닝 메시(WorldAnimatedModel) shadow caster 미구현~~ | — | 완료(2026-07-12) — `ShadowMapRenderer::InitializeSkinned` + 신규 `ShadowMapSkinned.hlsl`로 해결. Kachujin 그림자 육안 검증 완료. 상세: `2026-07-11_Log.md > Compact Log #2` |
| SSAO (Pre-Lighting 패스) | 중간 | 필요 시 — `DeferredLightingRenderer`에 AO SRV append(root9는 IBL Irradiance가 선점, 2026-07-12 — SSAO는 root10부터), 노이즈 텍스처+커널 샘플+뷰공간 재구성 인프라 필요 |
| `IPostProcessPass` 인터페이스 일반화 | 낮음 | 두 번째 PostProcess 효과(SSAO를 체인에 넣기로 결정/MotionBlur 등) 추가 시 |
| Transparent shadow lookup | 낮음 | Forward 경로 shadow 필요 시 |

---

## 진행 순서 요약

```
Pre-0, Pre-1, Pre-3               ← 병렬/독립, 즉시                    [완료]
Pre-2                             ← Phase 1-1·1-2 착수 전 필수 공통 선결 [완료]

Phase 1-2 (범위 최대)              ← Pre-2 이후, 가장 먼저 착수·가장 늦게 완료 [완료 — 재-임포트 대기]
Phase 1-3                         ← Pre-3 이후, 1-1/1-2와 독립적으로 병행 가능 [완료 — Stage A]
Phase 1-1                         ← Pre-2 이후, 1-2와 독립               [완료]
Phase 1-4                         ← Phase 1-1(Tonemap+Transparent 재배치) 완료 후 [완료 — Bloom만]
```

모든 항목의 코드 구현이 끝났다. 남은 것은 문서 상단 "남은 수동 작업"의 빌드·재-임포트·육안 검증뿐이다.
