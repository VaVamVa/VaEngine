# VaEngine — 프로젝트 현황판

> 마지막 업데이트: 2026-07-16
>
> **[공지] 이 문서는 현황판입니다. ToDo 항목은 각 날짜의 Log 파일에만 기록합니다.**
>
> **[공지] (2026-07-16~) 계획/설계/근거는 계획서(`Plan/`)에, 실제 구현 내용은 날짜별 Log에 기록합니다. 자세한 규칙은 [문서화 규칙](#문서화-규칙) 참조.**

---

## 프로젝트 개요

DirectX12 + Vulkan 크로스 플랫폼 3D 렌더링 엔진.
개발 방식은 개념/가이드 위주 학습. 코드는 컴파일 가능한 수준으로 완성도 있게 작성.

빠르게 상품화하는 엔진이 아니라, **어떤 최적화가 가능하고 그 최적화가 언제 필요한지를 비교·수치로 보여주는 포트폴리오**로 삼는다. CSM/IBL처럼 단계적으로 문제를 진단하고 근거를 남기며 구현하는 방식, Phase 2(멀티플랫폼 프러스텀 컬링)처럼 하나의 정답으로 좁히지 않고 여러 방식(CPU 브루트포스/Loose-Octree, GPU-Driven)을 모두 구현해 비교하는 방식이 이 성격을 따른다.

---

## 핵심 아키텍처 (2026-07-13 기준)

### Execute (Engine 소유) + ApplicationManager (Application 소유)

`Execute`는 Engine 레이어로 이동하여 GPU 초기화, 프레임 루프, RHI 관리를 전담한다.
Application은 `ApplicationManager` 인터페이스를 구현하여 "무엇을 그릴지"에 대한 데이터만 제공한다.

**레이어 책임:**

| 레이어 | 역할 |
|---|---|
| Platform | OS 진입점(WinMain/Android), IExecute 인스턴스화 및 메인 루프 실행 |
| Engine | RHI 추상화, Execute(오케스트레이터), RenderGraph(패스 관리), Renderer(구현체) |
| Application/Core | 공통 유틸리티 (FreeCamera, LightManager, Time 등) |
| Application/Source | 프로그램 고유 로직 (VaProgramName), WorldObject 서브클래스 |

**Execute 호출 흐름:**

```
Execute::OnLoop()
  ├─ OnPreUpdate()                             (Engine 내부 — 입력/시간 처리)
  ├─ management->OnUpdate(delta)               (Application 구현 — 게임 로직, Transform 갱신)
  ├─ OnPreRender()                             (Engine 내부 — 커맨드 기록 준비)
  │
  ├─ RenderScene scene;                        (스냅샷 버켓 생성)
  ├─ management->SubmitRenderState(&scene)     (Application 구현 — 현재 상태 복사/제출)
  │
  ├─ scene.SortCommands()                      (Engine 내부 — 64비트 키 기반 병렬 정렬)
  ├─ RenderGraph graph;                        (엔진 내부 빌드)
  ├─ renderer->AddPasses(graph, output)        (Renderer: 패스 구성 - ForwardPass 등)
  ├─ graph.Compile()                           (Engine: DAG 분석, 자동 배리어 삽입)
  ├─ graph.Execute(cmdList, scene)             (Engine: 정렬된 명령 기록 및 실행)
  │
  └─ Present / Signal                          (Engine 내부)
```

---

## 현재 디렉토리 구조

```
VaEngine/
├── CMakeLists.txt
├── CMakePresets.json               ← windows-directx / asset-importer / engine 프리셋
├── Engine/
│   ├── _Shaders/
│   │   ├── Common/                 ← Lighting.hlsli, Sampler.hlsli 등 공유 헤더
│   │   └── DirectX/               ← CubeShader.hlsl, AnimationDemo.hlsl, Sky.hlsl
│   ├── Public/
│   │   ├── Animation/              ← AnimClip.h, AnimController.h
│   │   ├── Asset/                  ← MeshAsset.h, SkmAsset.h, ClipAsset.h, *Loader.h
│   │   ├── Interfaces/             ← IActivate.h
│   │   ├── Manager/                ← ApplicationManager.h
│   │   ├── Math/                   ← Container.h (Matrix4x4, Quaternion 등)
│   │   ├── Mesh/                   ← IMesh.h, MeshData.h, SkinnedVertex.h, 각종 Shape
│   │   ├── Object/                 ← WorldObject.h, Skeleton.h, WorldAnimatedModel.h
│   │   ├── RHI/                    ← IBuffer.h, IDepthBuffer.h, ITexture2DArray.h, IRenderDevice.h 등
│   │   ├── Render/                 ← IRenderer.h, IMaterial.h, ILight.h, AnimationRenderer.h
│   │   ├── Scene/                  ← RenderScene.h, BaseCamera.h, Transform.h
│   │   ├── System/                 ← ITime.h, IKeyInput.h, InputContext.h 등
│   │   └── Utilities/              ← Locator.h, Singleton.h, Helpers.h
│   └── Private/
│       ├── Animation/              ← AnimController.cpp
│       ├── Asset/                  ← MeshLoader.cpp, SkmLoader.cpp, ClipLoader.cpp
│       ├── Execute.cpp / .h        ← Engine 루프 오케스트레이터
│       ├── Math/
│       ├── Mesh/                   ← MeshPrimitive, CubeShape, SkinnedMesh 등
│       ├── Object/                 ← Skeleton.cpp, WorldAnimatedModel.cpp
│       ├── RHI/
│       │   └── DirectX/            ← DX12 백엔드 (CommandList, RenderDevice, SwapChain 등)
│       │       ├── Buffer/         ← Buffer_DirectX, DepthBuffer_DirectX
│       │       ├── Pipeline/       ← PipelineState_DirectX
│       │       ├── Shader/
│       │       └── Texture/        ← Texture_DirectX, TextureFloat_DirectX, Texture2DArray_DX
│       ├── Render/                 ← ForwardRenderer, AnimationRenderer, RenderGraph
│       ├── Scene/
│       ├── System/
│       └── Utilities/
├── Application/
│   ├── Core/                       ← FreeCamera, LightManager, Time 유틸
│   └── Source/
│       ├── Public/WorldObjects/    ← WO_Cube.h, WO_Tower.h, WO_Kachujin.h
│       └── Private/WorldObjects/  ← 각 WorldObject 구현
├── Platforms/
│   ├── ExecWindows/                ← WinMain + Execute 연결
│   └── ExecAndroid/                ← Android 진입점 (ndk)
└── Tools/
    ├── ShaderCompiler/             ← VaShaderCompiler (HLSL → CSO 오프라인 컴파일)
    └── ImportTool/                 ← VaImportTool (FBX → .mesh / .smesh / .clip)
        └── Assets/Output/          ← 변환 결과물 (→ _Assets/ 로 수동 복사)
```

---

## 프로젝트 상태

| 프로젝트 | 타입 | 빌드 | 코드 작성 |
|---|---|---|---|
| Engine | Static Library | ✅ 성공 | ✅ RHI/RenderGraph/Renderer/Execute + 스켈레탈 애니메이션 + Deferred Renderer + Material 시스템(Phase 1~6) + PBR(Cook-Torrance GGX) + HDR/Tonemap·Normal Mapping·SSAO·Bloom·CSM Shadow·OIT 완료 |
| Application | Static Library | ✅ 성공 | ✅ WorldObject 계층, LightManager, FreeCamera 완료 |
| ExecWindows | Executable | ✅ 성공 | ✅ WinMain 연동 완료 |
| VaShaderCompiler | Tool | ✅ 성공 | ✅ 오프라인 셰이더 컴파일 시스템 구축 완료 |
| VaImportTool | Tool | ✅ 성공 | ✅ FBX → .mesh / .smesh / .clip 변환 완료 |

---

## 핵심 설계 원칙

### 1. 스냅샷 기반 데이터 절연 (Snapshot Isolation)
`RenderScene`은 로직 스레드의 데이터를 렌더 스레드용 버킷(Bucket)으로 복사하여 멀티스레드 환경의 데이터 경합을 차단한다.

### 2. 통합 커맨드 스트림 (Unified Command Stream)
정적 메시(`IMesh`)와 스키닝 메시(`SkinnedMesh`)를 단일 `RenderCommand` 리스트로 통합 관리한다. `skinnedMesh` 필드 유무로 두 종류를 구분하며, `GetSkinnedCommands()` 별도 인터페이스는 제거되었다. 모든 명령이 하나의 리스트에서 정렬·분배되므로 Opaque/Transparent 분기가 자연스럽게 가능하다.

### 3. 64비트 렌더 커맨드 정렬 (Sorting)
`RenderSortKey` [ Layer(4) | Translucency(1) | Pass(3) | MaterialID(16) | Depth(40) ] 를 통해 GPU 상태 변경을 최소화하고 Front-to-Back 깊이 최적화를 수행한다. `RenderScene::CalculateSortKey()`가 오브젝트 제출 시점에 카메라 거리(유클리드) 및 `ITexture::HasAlpha()` 결과를 자동으로 소트키에 반영한다.

### 4. RenderGraph (DAG & Auto-Barrier)
패스별 리소스 의존성을 분석하여 GPU 배리어(Barrier)를 자동으로 삽입하고, 리소스 재사용(Aliasing) 및 패스 컬링을 지원한다.

### 5. 오프라인 셰이더 컴파일 (Binary-based RHI)
`VaShaderCompiler`를 통해 빌드 시점에 `.cso` 바이너리를 생성하며, RHI는 런타임 컴파일 없이 바이너리를 로드한다.

### 6. 멀티 플랫폼 다형성
- **Mesh**: `MeshData` 중간 포맷을 통해 Primitive와 FBX(Assimp) 업로드 경로 통일.
- **Math**: `Matrix4x4` 정적 팩토리를 통해 LH/RH 및 NDC 공간 차이 대응.
- **RHI**: `BeginRenderPass` 인터페이스로 Mobile(Android) 타일 기반 아키텍처 지원.

### 7. 스켈레탈 애니메이션 시스템
- **오프라인 변환**: VaImportTool (Assimp) → `.smesh` (스켈레톤 + 스키닝 정점) + `.clip` (본별 SRT, version 2)
- **GPU 업로드**: `BakeTransformsMap` — 이름 기반 본 매핑, `Texture2DArray` [clipIdx][frameIdx][boneIdx × 4]
- **Global SRV Heap**: `RenderDevice_DirectX` 단일 SRV Heap (1024 slots) — 다중 텍스처 바인딩 안정화
- **Tween Transition**: `AnimController::PlayTween()` — inter-clip lerp, HLSL `lerp(currentSkinMat, nextSkinMat, TweenTime)`
- **Bone Palette GPU 오프로드**: `BonePaletteCompute.hlsl` — Dispatch(1, instanceCount, 1), `[numthreads(250, 1, 1)]`. `SkinnedMesh`가 buffer/UAV/SRV 소유 (다종 mesh 자동 분리)
- **3-clip Blend**: `EAnimBlendMode::Blend` — `PlayBlend(clip0, clip1, clip2)` + `SetBlendAlpha(alpha [0,2])`. CSMain에서 alpha≤1→clip0↔1, alpha>1→clip1↔2 가중 합성

### 8. Deferred + Forward 하이브리드 렌더링

#### Windows (DX12) 기준 패스 순서 (`SceneRenderer::AddPasses`)
```
DeferredSkyPass (hdrOut 클리어) →
BonePaletteCompute (스키닝 GPU 오프로드) →
ShadowMapPass (CSM, 화면과 다른 해상도의 트랜지언트 Texture2DArray Depth) →
GBufferPass (RT0 Albedo+AO / RT1 Normal+Rough / RT2 Metallic) →
SSAOPass (Raw → Blur, RT1+Depth만으로 계산) →
DeferredLightingPass (CS: GBuffer+Shadow+IBL+SSAO → hdrOut) →
TransparentPass (ForwardRenderer, hdrOut에 그림 · shared depth DepthRead) →
BloomPass (BrightPass → BlurH → BlurV → Composite, hdrOut에 가산) →
BlitPass (hdrOut → 백버퍼, Tonemap+Gamma) →
DebugLinePass →
DebugTextPass
```

- **Opaque/Transparent 분기**: `RenderSortKey` bit[59](`translucent` 플래그). `GBufferRenderer`는 translucent 스킵, `ForwardRenderer::TransparentPass`가 담당.
- **2-Pass 투명 렌더링**: `transparentBackFacePSO`(CullMode::Front) → `transparentFrontFacePSO`(CullMode::Back) 순서로 투명 오브젝트 자신의 뒷면 및 앞면을 모두 렌더링. Weighted Blended OIT 구현 완료(런타임 토글 없음, 상시 적용).
- **Material 중심 렌더링 (Phase 1~6 완료)**: `IMaterial`이 GPU 버퍼 소유 (dirty flag). DrawGroup 기준 `(mesh, IMaterial*)`. `GetCullMode()` → DoubleSided PSO 선택. `GetBlendMode()` → ForwardRenderer PSO 선택. `RenderCommand.texture` 제거 (Phase 5). HLSL Phong → PBR 전면 교체 (Phase 6).
- **Transparent는 Blit보다 먼저 hdrOut에 그린다**: Tonemap(Blit)이 Opaque+Transparent를 모두 포함한 결과에 한 번만 적용되도록 순서 고정(Plan_RenderQuality.md Phase 1-1).
- **Read-Only DSV**: `TransparentPass` / `DebugLinePass`는 GBuffer depth를 `D3D12_DSV_FLAG_READ_ONLY_DEPTH`로 바인딩 (depth test O, write X).
- **SkinnedMesh**: GBufferSkinned PSO로 GBuffer에 직접 쓰기. ForwardRenderer 경유 없음. Shadow Map의 스키닝 caster도 동일 패턴(BonePaletteCompute가 ShadowMapPass보다 항상 먼저 실행).
- **CSM (Cascaded Shadow Mapping)**: `ShadowMapRenderer`가 Directional Light 기준 최대 8캐스케이드(`kMaxCascadeCount`)를 항상 할당하고, 실제 사용 개수(`activeCascadeCount`, 1~8, 런타임 Num4/Num5 조정)만큼만 매 프레임 그린다(Mesh LOD와 동일 패턴 — 캐스케이드 수를 바꿔도 RenderGraph 리소스 재할당 없음). PSSM(Practical Split Scheme) 분할 + 카메라 서브 프러스텀 bounding-sphere 피팅, 캐스케이드 경계 smooth blend. 상세 설계·버그 수정 이력은 `2026-07-13_Q&A.md`/`2026-07-13_Log.md` 참조.

### 9. Meshless HDRi Sky Rendering
- **Full-screen Triangle**: SV_VertexID (0,1,2) → NDC 삼각형 생성. VB/IB 없이 `DrawInstanced(3, 1)` 한 번으로 화면 전체 커버
- **LatLong UV 매핑**: InvProj + InvViewRot(View 회전 역행렬)를 CB로 전달, PS에서 월드 방향 → θ/φ → UV 변환
- **TextureFloat_DirectX**: `ITexture` 구현체, `DXGI_FORMAT_R32G32B32A32_FLOAT`, `stbi_loadf()` 사용 (`.hdr` 파일 지원)
- **SkyPass 상시 등록**: SkyPass(ELoadAction::Clear) → ForwardPass(ELoadAction::Load) 순서로 고정
- **에셋 경로**: `ASSETS_DIR "HDR/filename.hdr"` — `ASSETS_DIR`은 `"${CMAKE_SOURCE_DIR}/_Assets/"` (후행 슬래시 포함)

---

## 문서화 규칙

> 2026-07-16 신설. 이 날짜 이후 새로 시작하는 작업부터 적용한다 — 기존 `Plan_RenderQuality.md`/`Plan_Material_Driven.md` 등 과거 계획서를 이 규칙에 맞춰 소급 재작성하지는 않는다.

**계획서(Plan)와 Log는 역할이 다르다.** 하나의 문서에 계획과 구현 결과를 함께 쓰면, 구현이 끝난 뒤 계획서를 계속 고쳐야 하고 같은 내용이 Log와 계획서 양쪽에 중복 서술되기 쉽다. 이를 막기 위해 아래처럼 역할을 분리한다.

| 문서 | 역할 | 위치/이름 규칙 | 수정 시점 |
|---|---|---|---|
| **계획서(Plan)** | 계획·설계·근거(왜 이렇게 하기로 했는가) | `Plan/Plan_{주제}.md` (예: `Plan/Plan_Phase2.md`) | 계획/설계/근거 자체가 바뀔 때만 수정 |
| **Log** | 실제 구현 내용(무엇을 어떻게 고쳤는가), Compact Log 단위 | `{YYYY-MM-DD}_Log.md` | 구현이 진행되는 그날그날 추가 |

**연결 규칙:**

1. **계획서에는 "무엇을 왜 하기로 했는가"만 남긴다.** 실제로 무엇을 구현했는지에 대한 상세 서술(수정 파일, 코드 변경 내역, 버그·검증 이력)은 전부 Log 쪽 책임이며, 계획서에 중복 기술하지 않는다.
2. **구현이 완료되면 계획서의 해당 항목을 `~~취소선~~` 처리**하고(기존 관례 유지), 그 아래에 완료 날짜 Log 파일을 상대경로 마크다운 링크로 태그한다. 계획서는 `Plan/` 하위, Log는 `AgentLog/` 바로 아래에 있으므로 계획서에서 Log를 가리키려면 한 단계 위로 올라가야 한다 — 예: `Plan/Plan_Phase2.md` 안에서는 `` [2026-07-30_Log.md](../2026-07-30_Log.md) `` 형태.
3. **반대 방향으로 Log의 `## Start Log`도 그날 대응하는 계획서를 태그한다** — `### [ToDo](../TODO.md#phase-N)`와 함께 `### [계획서](./Plan/Plan_{주제}.md)`를 나란히 걸어, 어느 문서에서 시작해도 서로 찾아갈 수 있게 한다(2026-07-30_Log.md 참조).
4. Q&A 파일(`{YYYY-MM-DD}_Q&A.md`)은 이 분리와 별개로 기존 관례를 그대로 유지한다 — 계획서 작성 전 논의된 질문/답변 기록.

---

## 문서 구조 (Log 및 Q&A 기록)

| 파일 | 내용 |
|---|---|
| [2025-12-10.md](2025-12-10.md) | DirectX11 vs DirectX12 비교 (멀티스레딩·메모리·동기화 방식 차이), DXGI 역할 및 헤더 버전 구조 (1.0~1.6 점진적 COM 상속) |
| [2026-01-17.md](2026-01-17.md) | WndProc 최적화 — GetMessage(블로킹) → PeekMessage(논블로킹) 전환, 메시지 핸들러 테이블, 입력 버퍼링, Raw Input API |
| [2026-04-22_Q&A.md](2026-04-22_Q&A.md) | Public/Private 분리, RHI 구조, Platform 위치, 라이프사이클, 빌드 워크플로우 |
| [2026-04-22_Log.md](2026-04-22_Log.md) | Q10 아키텍처 재구성, 프로젝트 4개 생성, Android 빌드 오류 해결 |
| [2026-04-30_Q&A.md](2026-04-30_Q&A.md) | ExecAndroid 참조 경고, IApplication 혼용 문제, 플랫폼별 구현 구조 확정 |
| [2026-04-30_Log.md](2026-04-30_Log.md) | 참조 설정, IApplication 아키텍처 재설계, RHI 백엔드 아키텍처 확정, FNativeWindowInfo 구조체 설계 |
| [2026-05-01_Q&A.md](2026-05-01_Q&A.md) | CMake 파일 수집, RHI 백엔드 CMake 제어, FNativeWindowInfo, Vulkan/DX12 Surface, NDK, Android Vulkan, 레이어 순정 확정 |
| [2026-05-02_Q&A.md](2026-05-02_Q&A.md) | IExecute 구현체 명명, WinMain 설계, RHI 인터페이스 설계, DX12 개요, Factory 패턴, FILE_SET, USE_DX12 매크로, DirectX-Headers |
| [2026-05-02_Log.md](2026-05-02_Log.md) | WinMain 구현 완료, RHI 인터페이스 전체 선언, RHILoader 팩토리, DX12 환경 구성 |
| [2026-05-03_Q&A.md](2026-05-03_Q&A.md) | DX12 초기 구현 빌드·설계 Q&A 23항목 — FILE_SET 스코프 충돌, FetchContent vs Submodule, DirectX-Headers include 경로, IRenderDevice Factory 설계, virtual/override 관례, DXGI FetchContent 불가 이유 |
| [2026-05-03_Log.md](2026-05-03_Log.md) | DX12 RHI 핵심 객체 구현 완료 (CommandQueue, Fence, SwapChain), ThirdParty FetchContent 전환 |
| [2026-05-04_Q&A.md](2026-05-04_Q&A.md) | IExecute 필요성, Execute 위치 결정, 엔진 배포 전략 |
| [2026-05-04_Log.md](2026-05-04_Log.md) | Execute → Application 이동, RHI 인터페이스 재구조화, CommandList/CommandAlloc 구현, engine 프리셋 추가 |
| [2026-05-05_Q&A.md](2026-05-05_Q&A.md) | if constexpr vs #if, USE_DEBUGGING 설계, engine 프리셋 구조, CMakePresets 검증 |
| [2026-05-05_Log.md](2026-05-05_Log.md) | 첫 렌더링 명령(Clear) 및 WaitForPreviousFrame 구현 |
| [2026-05-06_Log.md](2026-05-06_Log.md) | Fence 동기화 안정화 및 TRHIResource 커스텀 삭제자 적용 |
| [2026-05-07_Log.md](2026-05-07_Log.md) | IGPUBuffer 인터페이스 설계 및 Vertex/Index Buffer 구현 |
| [2026-05-08_Refactoring_Log.md](2026-05-08_Refactoring_Log.md) | 대규모 리팩터링: Execute Engine 이동, ApplicationManager 도입, RenderGraph/RenderScene 스냅샷, IBuffer/IShader 통합, 오프라인 셰이더 컴파일러 구축 |
| [2026-05-08_Log.md](2026-05-08_Log.md) | Depth Buffer, Phong 조명(IMaterial+ILight), Draw Instanced, Transform 컴포넌트, LightManager, WorldObject, IActivate, VaImportTool, 텍스처 파이프라인, WO_Tower |
| [2026-05-09_Log.md](2026-05-09_Log.md) | 스켈레탈 애니메이션 전체 구현, ImportTool 버그 수정 3종, .clip v2 bone 이름 매핑, Global SRV Heap, Tween Transition |
| [2026-05-10_Q&A.md](2026-05-10_Q&A.md) | 외부 HDR Asset 적용 방법, LoadFromFile 인터페이스 개선 |
| [2026-05-10_Log.md](2026-05-10_Log.md) | Meshless Sky 런타임 검증, LoadFromFile narrow 전환, ASSETS_DIR 정비, Debug Text Panel 시스템, Pick Ray, Compute 인프라 + BonePalette GPU 오프로드, 3-clip BlendBones |
| [2026-05-11_Log.md](2026-05-11_Log.md) | RenderCommand 통합(Unified Command Stream), CalculateSortKey 자동화, TransparentPass 2-패스 구성, AnimationRenderer API 동기화, RenderGraph 디버그 패널, CameraManager 접근자 확장 |
| [2026-07-06_Log.md](2026-07-06_Log.md) | Phase 4~6 완료: 2-Pass 투명 렌더링, RenderCommand.texture 제거(Phase 5), HLSL Phong→PBR 전면 교체(Phase 6), 0x87A 크래시 2건 수정, PBR ambient 조정 |
| [2026-07-11_Q&A.md](2026-07-11_Q&A.md) | IBL Stage B 프리필터 밉 해상도/레벨 수, BRDF LUT 크기 근거 |
| [2026-07-11_Log.md](2026-07-11_Log.md) | Plan_RenderQuality.md 전체 구현(Pre-0~3+Phase 1-1~4: HDR/Tonemap·Normal Mapping·Shadow Stage A·Bloom), RenderGraph BaseRHIResource 리팩터링, IBL Stage B + Forward Transparent IBL 적용 |
| [2026-07-12_Q&A.md](2026-07-12_Q&A.md) | OIT 도입 장단점·2-Pass CullMode 오류 형태, 굴절/투과색 병목 여부, 런타임 토글 가능성, D3D12 경고 정리 |
| [2026-07-12_Log.md](2026-07-12_Log.md) | Scene Scale 단위 통일(cm→m 변환), WorldObject Local/World Transform Hierarchy, SSAO 구현, 런타임 IBL/SSAO 비교 토글, Weighted Blended OIT |
| [2026-07-13_Q&A.md](2026-07-13_Q&A.md) | CSM 설계 Q&A — Atlas vs Texture2DArray Depth, 사이드 이펙트, 드로우콜, blend/dither, GS/HS, mip level 비교 |
| [2026-07-13_Log.md](2026-07-13_Log.md) | CSM(Cascaded Shadow Mapping) 구현 — Texture2DArray Depth 8캐스케이드, 활성 개수 런타임 조정(Num4/5), smooth blend, 캐스케이드 색상 오버레이(Num3). 버그 3종(버퍼 재사용 타이밍·행렬 row/column 의미론·HLSL cbuffer 스칼라 배열 패킹) 발견·수정, Math 유틸 정리(World/View 축 추출 함수 분리, `InvertRigidTransform`) |
| [2026-07-16_Q&A.md](2026-07-16_Q&A.md) | 프러스텀 컬링 설계 Q&A — CPU 사전 필터링 vs GPU 클리핑 오해 정정, 멀티스레드 브루트포스 vs Loose-Octree 비교 |
| [2026-07-30_Log.md](2026-07-30_Log.md) | Phase 2(멀티플랫폼 프러스텀 컬링) 착수 및 설계 — TODO.md Phase 2 신설, 계획서/Log 문서화 규칙 신설, Plan_Phase2.md 인터페이스·데이터 흐름 설계 및 최종 검증 완료 |
| [Plan/Plan_Material_Driven.md](Plan/Plan_Material_Driven.md) | Material 중심 렌더링 계획서 (Phase 1~6 완료, Phase 7 Normal Mapping 대기) |
| [Plan/Plan_Animation.md](Plan/Plan_Animation.md) | 스켈레탈 애니메이션 구현 계획 (Steps 1~11 완료) |
| [Plan/AssetImporter.md](Plan/AssetImporter.md) | VaImportTool + 텍스처 파이프라인 구현 계획 |
| [Plan/Meshless_Sky_Rendering.md](Plan/Meshless_Sky_Rendering.md) | Meshless HDRi Sky Rendering 구현 계획 (Steps 1~9 완료) |
| [Plan/DebugTextRendering.md](Plan/DebugTextRendering.md) | Debug Text Rendering 구현 계획 (stb_truetype + Glyph Atlas, Steps 1~9) |
| [Analysis/PhongToPBR_Migration.md](Analysis/PhongToPBR_Migration.md) | Phong→PBR 마이그레이션 커밋 리뷰 (2901a7c·7a59ab2·660d924): G-Buffer 설계, GGX 구현 정확성, 버퍼 레이아웃 버그 분석, 기술 부채 |

---

## 환경

- **언어**: C++20, HLSL (Model 5.0/6.0)
- **IDE**: Visual Studio 2026 / JetBrains Rider
- **빌드**: CMake + CMakePresets.json
- **SDK**: Windows SDK, Android NDK, Vulkan SDK
- **서드파티**: DirectX-Headers, stb_image, Assimp (ImportTool 전용)
