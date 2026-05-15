# VaEngine — 프로젝트 현황판

> 마지막 업데이트: 2026-05-11
>
> **[공지] 이 문서는 현황판입니다. ToDo 항목은 각 날짜의 Log 파일에만 기록합니다.**

---

## 프로젝트 개요

DirectX12 + Vulkan 크로스 플랫폼 3D 렌더링 엔진.
개발 방식은 개념/가이드 위주 학습. 코드는 컴파일 가능한 수준으로 완성도 있게 작성.

---

## 핵심 아키텍처 (2026-05-11 기준)

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
| Engine | Static Library | ✅ 성공 | ✅ RHI/RenderGraph/Renderer/Execute + 스켈레탈 애니메이션 완료 |
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

### 8. Forward 멀티패스 렌더링
- **패스 순서**: `SkyPass`(ELoadAction::Clear) → `ForwardPass`(Opaque, ELoadAction::Load) → `TransparentPass`(Transparent, ELoadAction::Load)
- **Opaque/Transparent 분기**: `RenderCommand::sortKey` bit[59](`translucent` 플래그)로 판별. `ForwardRenderer::Render(isTransparentPass)`가 해당 패스에 속한 명령만 그린다.
- **SkinnedMesh 제외**: `cmd.skinnedMesh != nullptr` 이면 ForwardRenderer에서 스킵 → `AnimationRenderer`가 전담.
- **Depth 재사용**: `TransparentPass`는 `ForwardPass`가 기록한 깊이 버퍼를 `DepthRead`로 읽어 Z-Test만 수행 (DepthWrite 없음).

### 9. Meshless HDRi Sky Rendering
- **Full-screen Triangle**: SV_VertexID (0,1,2) → NDC 삼각형 생성. VB/IB 없이 `DrawInstanced(3, 1)` 한 번으로 화면 전체 커버
- **LatLong UV 매핑**: InvProj + InvViewRot(View 회전 역행렬)를 CB로 전달, PS에서 월드 방향 → θ/φ → UV 변환
- **TextureFloat_DirectX**: `ITexture` 구현체, `DXGI_FORMAT_R32G32B32A32_FLOAT`, `stbi_loadf()` 사용 (`.hdr` 파일 지원)
- **SkyPass 상시 등록**: SkyPass(ELoadAction::Clear) → ForwardPass(ELoadAction::Load) 순서로 고정
- **에셋 경로**: `ASSETS_DIR "HDR/filename.hdr"` — `ASSETS_DIR`은 `"${CMAKE_SOURCE_DIR}/_Assets/"` (후행 슬래시 포함)

---

## 문서 구조 (Log 및 Q&A 기록)

| 파일 | 내용 |
|---|---|
| [2026-04-22_Q&A.md](2026-04-22_Q&A.md) | Public/Private 분리, RHI 구조, Platform 위치, 라이프사이클, 빌드 워크플로우 |
| [2026-04-22_Log.md](2026-04-22_Log.md) | Q10 아키텍처 재구성, 프로젝트 4개 생성, Android 빌드 오류 해결 |
| [2026-04-30_Q&A.md](2026-04-30_Q&A.md) | ExecAndroid 참조 경고, IApplication 혼용 문제, 플랫폼별 구현 구조 확정 |
| [2026-04-30_Log.md](2026-04-30_Log.md) | 참조 설정, IApplication 아키텍처 재설계, RHI 백엔드 아키텍처 확정, FNativeWindowInfo 구조체 설계 |
| [2026-05-01_Q&A.md](2026-05-01_Q&A.md) | CMake 파일 수집, RHI 백엔드 CMake 제어, FNativeWindowInfo, Vulkan/DX12 Surface, NDK, Android Vulkan, 레이어 순정 확정 |
| [2026-05-02_Q&A.md](2026-05-02_Q&A.md) | IExecute 구현체 명명, WinMain 설계, RHI 인터페이스 설계, DX12 개요, Factory 패턴, FILE_SET, USE_DX12 매크로, DirectX-Headers |
| [2026-05-02_Log.md](2026-05-02_Log.md) | WinMain 구현 완료, RHI 인터페이스 전체 선언, RHILoader 팩토리, DX12 환경 구성 |
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
| [Plan/Plan_Animation.md](Plan/Plan_Animation.md) | 스켈레탈 애니메이션 구현 계획 (Steps 1~11 완료) |
| [Plan/AssetImporter.md](Plan/AssetImporter.md) | VaImportTool + 텍스처 파이프라인 구현 계획 |
| [Plan/Meshless_Sky_Rendering.md](Plan/Meshless_Sky_Rendering.md) | Meshless HDRi Sky Rendering 구현 계획 (Steps 1~9 완료) |
| [Plan/DebugTextRendering.md](Plan/DebugTextRendering.md) | Debug Text Rendering 구현 계획 (stb_truetype + Glyph Atlas, Steps 1~9) |

---

## 환경

- **언어**: C++20, HLSL (Model 5.0/6.0)
- **IDE**: Visual Studio 2026 / JetBrains Rider
- **빌드**: CMake + CMakePresets.json
- **SDK**: Windows SDK, Android NDK, Vulkan SDK
- **서드파티**: DirectX-Headers, stb_image, Assimp (ImportTool 전용)
