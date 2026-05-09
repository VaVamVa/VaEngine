# VaEngine — 프로젝트 현황판

> 마지막 업데이트: 2026-05-08
>
> **[공지] 이 문서는 현황판입니다. ToDo 항목은 각 날짜의 Log 파일에만 기록합니다.**

---

## 프로젝트 개요

DirectX12 + Vulkan 크로스 플랫폼 3D 렌더링 엔진.
개발 방식은 개념/가이드 위주 학습. 코드는 컴파일 가능한 수준으로 완성도 있게 작성.

---

## 핵심 아키텍처 (2026-05-08 기준)

### Execute (Engine 소유) + ApplicationManager (Application 소유)

`Execute`는 Engine 레이어로 이동하여 GPU 초기화, 프레임 루프, RHI 관리를 전담한다.
Application은 `ApplicationManager` 인터페이스를 구현하여 "무엇을 그릴지"에 대한 데이터만 제공한다.

**레이어 책임:**

| 레이어 | 역할 |
|---|---|
| Platform | OS 진입점(WinMain/Android), IExecute 인스턴스화 및 메인 루프 실행 |
| Engine | RHI 추상화, Execute(오케스트레이터), RenderGraph(패스 관리), Renderer(구현체) |
| Application/Core | 공통 유틸리티 (FreeCamera, GameObject 등) |
| Application/Source | 프로그램 고유 로직 (GameApp), ApplicationManager 구현 |

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
├── CMakeLists.txt              ← 최상위. option(USE_DIRECTX/USE_VULKAN), Tools/ShaderCompiler 포함
├── CMakePresets.json           ← windows-directx / windows-vulkan / android / engine 프리셋
├── Engine/
│   ├── _Shaders/               ← HLSL 소스 (Common/DirectX/Vulkan)
│   ├── Public/
│   │   ├── Manager/            ← ApplicationManager.h
│   │   ├── Scene/              ← RenderScene.h, BaseCamera.h
│   │   ├── Render/             ← RenderGraph.h, IRenderer.h, IRenderPass.h
│   │   ├── RHI/                ← RHI 추상 인터페이스 (IBuffer, IShader, RenderPassDesc 등)
│   │   ├── Mesh/               ← MeshData.h, PrimitiveShape.h, IMesh.h
│   │   └── Math/               ← Container.h (Row-Major, LH/RH 지원)
│   ├── Private/
│   │   ├── Core/               ← Execute.cpp (Engine 소유 루프)
│   │   ├── Render/             ← ForwardRenderer.cpp, RenderGraph.cpp
│   │   └── RHI/
│   │       ├── DirectX/        ← DX12 백엔드 구현 (Shader_DirectX, PipelineState_DirectX 등)
│   │       └── Vulkan/         ← Vulkan 백엔드 구현 (예정)
├── Application/
│   ├── Core/                   ← 공통 앱 유틸리티 (FreeCamera, GameObject, Input 전용 로직)
│   └── Source/                 ← Client 전용 로직 (VaProgramName.cpp 등)
├── Platforms/
│   ├── ExecWindows/            ← WinMain + Execute 연결
│   └── ExecAndroid/            ← Android 진입점 (ndk)
└── Tools/
    └── ShaderCompiler/         ← VaShaderCompiler (HLSL -> CSO 오프라인 컴파일러)
```

---

## 프로젝트 상태

| 프로젝트 | 타입 | 빌드 | 코드 작성 |
|---|---|---|---|
| Engine | Static Library | ✅ 성공 | ✅ RHI/RenderGraph/Renderer/Execute 아키텍처 완료 |
| Application | Static Library | ✅ 성공 | ✅ ApplicationManager 기반 구조 전환 완료 |
| ExecWindows | Executable | ✅ 성공 | ✅ WinMain 연동 완료 |
| VaShaderCompiler | Tool | ✅ 성공 | ✅ 오프라인 셰이더 컴파일 시스템 구축 완료 |

---

## 핵심 설계 원칙

### 1. 스냅샷 기반 데이터 절연 (Snapshot Isolation)
`RenderScene`은 로직 스레드의 데이터를 렌더 스레드용 바구니(Bucket)로 복사(Memcpy)하여, 멀티스레드 환경에서 데이터 경합(Race Condition)을 원천 차단한다.

### 2. 64비트 렌더 커맨드 정렬 (Sorting)
`RenderSortKey` [ Layer(4) | Translucency(1) | Pass(3) | MaterialID(16) | Depth(40) ] 를 통해 GPU 상태 변경을 최소화하고 뎁스 최적화(Front-to-Back)를 수행한다.

### 3. RenderGraph (DAG & Auto-Barrier)
패스별 리소스 의존성을 분석하여 GPU 배리어(Barrier)를 자동으로 삽입하고, 리소스 재사용(Aliasing) 및 패스 컬링을 지원한다.

### 4. 오프라인 셰이더 컴파일 (Binary-based RHI)
`VaShaderCompiler`를 통해 빌드 시점에 `.cso` 바이너리를 생성하며, RHI는 런타임 컴파일 없이 바이너리를 로드하여 시작 시간을 단축하고 안정성을 높인다.

### 5. 멀티 플랫폼 다형성
- **Mesh**: `MeshData` 중간 포맷을 통해 Primitive와 FBX(Assimp) 업로드 경로 통일.
- **Math**: `Matrix4x4` 정적 팩토리를 통해 LH/RH 및 NDC 공간 차이(0~1 vs -1~1) 대응.
- **RHI**: `BeginRenderPass` 인터페이스로 Mobile(Android)의 타일 기반 아키텍처 최적화 지원.

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
| [2026-05-08_Log.md](2026-05-08_Log.md) | Depth Buffer 구현, Phong 조명(Material/Light) 및 Normal 데이터 추가 |
| [2026-05-08_Refactoring_Log.md](2026-05-08_Refactoring_Log.md) | 대규모 리팩터링: Execute Engine 이동, ApplicationManager 도입, RenderGraph/RenderScene 스냅샷, IBuffer/IShader 통합, 오프라인 셰이더 컴파일러 구축 |

---

## 환경

- **언어**: C++20, HLSL (Model 5.0/6.0)
- **IDE**: Visual Studio 2026 / Rider
- **빌드**: CMake + CMakePresets.json
- **SDK**: Windows SDK, Android NDK, Vulkan SDK
