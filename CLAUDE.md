# CLAUDE.md

---

## 설계 원칙

구현 요청 시 코드를 쓰기 전에 이 원칙을 먼저 확인한다.

### 구현 전 필수 확인

1. **시스템 전체 영향 먼저** — 이 변경이 RHI · 렌더러 · Material · 플랫폼 · 예정 기능(Normal Mapping / Shadow / Particle 등)에 어떤 영향을 주는지 먼저 정리하고 사용자와 공유한다.
2. **기술 부채는 구현 전에 고지** — "지금 이렇게 하면 나중에 X를 고쳐야 한다"는 사실을 코드 작성 전에 명시한다.
3. **목표 상태로 바로 구현** — 임시·테스트용 구현 금지. 처음부터 리팩토링 없이 유지할 수 있는 구조로 작성한다.
4. **인터페이스·데이터 흐름 설계 후 동의** — 코드를 쓰기 전에 인터페이스와 데이터 흐름을 텍스트로 정리하고 사용자가 동의하면 구현한다.

### 사용자 의견에 대한 태도

사용자의 말이 항상 정답은 아니다. 동의하기 전에 논리적 인과나 코드·스펙 상의 근거가 있는지 확인한다.

- 근거가 명확하면 동의하고 진행한다.
- 근거가 불충분하거나 방향이 잘못됐다고 판단되면, 이유와 함께 반론을 제시한다.
- "사용자가 원하니까"는 동의의 근거가 아니다. 무조건적인 동의는 잘못된 설계를 고착시키고 결국 더 큰 수정 비용을 만든다.
- 단, 최종 판단과 결정은 사용자의 몫이다. 반론을 제시한 뒤에도 사용자가 원래 방향을 선택하면 그 결정을 따른다.

### 멀티 플랫폼 제약

두 축은 독립적이다. 알고리즘 선택(Forward/Deferred/GPU-Driven)과 API 선택(DX12/Vulkan)을 혼동하지 않는다.

| 플랫폼 | API | GPU 아키텍처 | 렌더링 방향 |
|---|---|---|---|
| ExecWindows | DX12 | IMR (즉시 모드) | Deferred 유지, 장기 GPU-Driven 가능 |
| ExecAndroid | Vulkan | TBDR (타일 기반) | Forward 선호. ExecuteIndirect·Compute Culling은 대역폭 역효과. Deferred MRT도 비용 큼 |

- 렌더링 알고리즘은 `ISceneRenderer` 수준에서 플랫폼별로 교체 가능하도록 설계한다.
- GPU-Driven 방식은 데스크탑 전용 구현 선택이며, Material 시스템과 경쟁하는 개념이 아니다.

### 렌더링 아키텍처 원칙

- **Material = 의도** (blend mode, texture 슬롯, 조명 파라미터). **Renderer = 구현** (PSO 선택, 배리어, 패스 순서). Material이 PSO를 직접 소유하지 않는다.
- **RenderScene = CPU 데이터** ("무엇을 그릴 것인가"). **SceneRenderer = GPU 실행** ("어떻게 그릴 것인가"). 이 경계를 넘는 데이터 배치는 허용하지 않는다.
- **RenderGraph = 메커니즘** (배리어, 패스 순서, 실행). **SceneRenderer = 정책** (어떤 패스가 있고, 어떤 리소스를 소유하는가). 두 역할을 혼재하지 않는다.
- 새 추상화(인터페이스·기반 클래스)를 추가할 때 다형성이 실제로 쓰이거나 정말로 필요한지 확인한다. 불필요한 추상화는 추가하지 않는다.

---

## 빌드

모든 CMake 명령은 `VaEngine/` 디렉토리 기준으로 실행한다.

```powershell
# 구성
cmake --preset windows-directx      # DX12 메인 빌드
cmake --preset asset-importer       # VaImportTool (FBX 변환기)

# 빌드
cmake --build Build/windows-directx
cmake --build Build/asset-importer

# 실행
.\Build\windows-directx\Platforms\ExecWindows\Debug\ExecWindows.exe
.\Build\asset-importer\Tools\VaImportTool.exe
```

셰이더(`.hlsl`)를 수정하면 빌드 시 `VaShaderCompiler`가 자동으로 `.cso`를 재생성한다.  
테스트 시스템은 없다.

---

## 아키텍처

### 레이어 구조

```
Platform (ExecWindows / ExecAndroid)
    └─ Execute::OnLoop()
           ├─ management->OnUpdate()          [Application] — 게임 로직
           ├─ management->SubmitRenderState() — RenderScene 스냅샷 제출
           └─ RenderGraph::Execute()          [Engine] — GPU 커맨드 기록/실행
```

- **Engine** (`Engine/`): Static Library. RHI 추상화, 렌더러, 애니메이션, 에셋 로더.
- **Application/Core** (`Application/Core/`): FreeCamera, LightManager, Time 유틸.
- **Application/Source** (`Application/Source/`): 프로그램 고유 로직. `VaProgramName`이 `ApplicationManager` 구현.
- **Platform** (`Platforms/`): OS 진입점.

### 렌더링 파이프라인

```
Execute::OnRender()
    │
    ├─ RenderScene 구성 (SubmitRenderState)
    │       WorldObject::AddToScene → RenderCommand { mesh, material, sortKey }
    │
    ├─ SceneRenderer::AddPasses(RenderGraph, RenderScene)
    │       GBufferPass → DeferredLightingPass → BlitPass
    │       TransparentPass (ForwardRenderer, shared depth)
    │       DebugLine / DebugText
    │
    └─ RenderGraph::Execute() → GPU 커맨드 제출
```

- **SceneRenderer**: GPU 리소스 소유 + 서브렌더러 조율. 영속 리소스 상태를 프레임 간 보존(`UpdateResourceStates`).
- **RenderGraph**: 매 프레임 Reset. 배리어 자동 삽입, 패스 순서 실행.
- **FrameOutput**: `RenderGraph.h`에 정의. 스왑체인 백버퍼·해상도 정보.

### RHI 계층

모든 GPU 리소스는 `Engine/Public/RHI/` 인터페이스를 통해 접근한다.  
DX12 구현체는 `Engine/Private/RHI/DirectX/`에 위치하며 `USE_DIRECTX` 매크로로 활성화된다.

- DX12는 shader-visible CBV/SRV/UAV heap을 한 번에 하나만 바인딩 가능. `RenderDevice_DirectX`의 **전역 SRV Heap**(1024 slots)에서 모든 텍스처가 슬롯을 할당받는다. 텍스처별 private heap 생성 금지.
- 리소스 상태 전환(Barrier)은 항상 명시적으로 기록해야 한다.

### WorldObject 계층

```
WorldObject                 (Engine/Public/Object/)  — Transform 소유
├─ WorldModel               (Engine/Public/Object/)  — 정적 메시 + IMaterial 소유
│      ├─ WO_Cube / WO_Tower  (Application/Source/)
└─ WorldAnimatedModel       (Engine)                 — 스켈레탈 메시
       └─ WO_Kachujin       (Application/Source/)
```

`WorldObject` 서브클래스는 `Initialize(IRenderDevice*)` + `Update(float)` + `AddToScene(RenderScene&)` 패턴으로 구성한다.

### Material 시스템

- `IMaterial`: blend mode + Phong 파라미터 인터페이스. `EBlendMode`로 투명 여부 결정.
- `WorldModel::EnsureMaterial()`: 파생 클래스가 `WorldModel::Initialize`를 우회할 때 호출해 기본 Material을 생성한다.
- `RenderCommand.material`에서 sortKey 투명 판별. `ForwardRenderer`가 blend mode에 따라 PSO 선택.
- `WorldAnimatedModel`은 `IMaterial`을 소유하며(`.matl`의 `diffuse_tex=`/`normal_tex=` 파싱, `WorldModel`과 동일 패턴) Albedo·Normal Map을 GBufferSkinned에 바인딩한다.

### 스켈레탈 애니메이션 파이프라인

```
FBX → VaImportTool → .smesh + .clip × N → 수동 복사 → _Assets/{name}/

런타임:
SkmLoader → Skeleton + SkinnedMesh
ClipLoader → AnimClipData (boneNames 포함, version 2)
BakeTransformsMap → Texture2DArray [clipIdx][frameIdx][boneIdx×4]
AnimController → TweenFrameDesc → HLSL lerp
```

- `.smesh`와 `.clip`의 본 수/순서가 달라도 된다. `BakeTransformsMap`은 이름 기반 매핑.
- 클립 전환: `WorldAnimatedModel::PlayTween(nextClip, blendTime)`.

### 에셋 포맷

| 확장자 | 내용 | 로더 |
|---|---|---|
| `.mesh` | 정적 메시 (version 1) | `MeshLoader` |
| `.smesh` | 스켈레톤 + 스키닝 정점 (version 1) | `SkmLoader` |
| `.clip` | 애니메이션 키프레임 + 본 이름 (version 2) | `ClipLoader` |

`.clip` version 2부터 bone 이름 배열이 필수. 하위 호환 없음.

---

## 네이밍 규칙

| 대상 | 규칙 | 예시 |
|---|---|---|
| 변수 | camelCase | `runningTime`, `clipCount` |
| 함수 / 클래스 / 구조체 | PascalCase | `BakeTransformsMap`, `AnimController` |
| 순수 가상 클래스 | `I` 접두사 | `IRenderDevice`, `ITexture` |
| 공통 구현 포함 가상 클래스 | `Base` 접두사 | `BaseCamera` |
| NVI 훅 / Bridge 구현 함수 | `Impl_` 접두사 | `Impl_Draw()` |

---

## 코드 규칙

- COM 객체는 반드시 `ComPtr<T>` 사용 (RAII).
- `HRESULT` 반환값은 항상 검사. 실패 시 예외(`std::runtime_error`) 처리.
- 커맨드 큐/리스트는 용도별로 분리 (Direct / Copy / Compute).
- 새 텍스처/SRV 생성 시 `RenderDevice_DirectX::AllocateSRVDescriptor()`로 전역 힙에서 슬롯 할당.
- 소스코드는 사용자가 직접 작성한다. 요청 없이 구현 코드를 먼저 제안하지 않는다.
- 파일 이동 / 이름 변경은 IDE(VS / Rider)에서만 수행한다. 파일시스템 직접 조작 금지.

---

## 문서 위치

- 프로젝트 현황판: `Agent/AgentLog/README.md`
- Q&A / Log: `Agent/AgentLog/{YYYY-MM-DD}_Q&A.md` / `_Log.md`
- 구현 계획: `Agent/AgentLog/Plan/`
- 메모리: `C:\Users\User\.claude\projects\E--Project-VaEngine\memory\`
