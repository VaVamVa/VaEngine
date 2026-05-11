# Public Interface 리팩터링 계획

본 문서는 **Desktop**, **Android**, **Xbox(GDK)**, **PlayStation(AGC)** 등 멀티 플랫폼 대응을 목표로 하는 하이엔드 RHI 추상화 및 렌더링 아키텍처 계획입니다.

---

## 1. 목표

- **다형성 극대화**: 특정 API의 고유 개념이 Public 계층으로 유출되는 것을 차단.
- **렌더링/로직 완전 분리**: Application은 렌더링 명령에 전혀 관여하지 않음.
- **콘솔 최적화**: Xbox/PS의 명시적 메모리 및 동기화 모델을 수용할 수 있는 인터페이스 설계.
- **성능 및 멀티스레딩**: 스냅샷 및 RenderGraph 기반 커맨드 정렬 구조 도입.

---

## 2. 전체 아키텍처

### 2-1. 계층 구조

```
Platform (ExecWindows / ExecAndroid)
  └─ Engine::Execute (IExecute 구현, 렌더 루프 소유)
       └─ ApplicationManager* (Application/Core가 정의, Client가 구현)
```

### 2-2. 데이터 흐름

```
Application (Client — GameApp : ApplicationManager)
  camera.Update(dt)
  SubmitRenderState(scene)
    scene.SetCamera(view, proj)
    scene.AddMesh(mesh, transform)
         │
         ▼
Engine::Execute::OnRender(scene)
  RenderGraph graph                  ← Engine이 매 프레임 빌드
  graph.AddPass<ForwardPass>(scene)
  graph.Compile()                    ← 배리어 삽입, 패스 컬링
  renderer->Execute(graph, scene)    ← GPU 커맨드 기록
```

### 2-3. 역할 분리

| 클래스 | 위치 | 역할 |
|---|---|---|
| `ApplicationManager` | Engine/Public/Manager | Application이 구현할 인터페이스 |
| `RenderScene` | Engine/Public/Scene | Application → Engine 스냅샷 |
| `BaseCamera` | Engine/Public/Scene | 위치·행렬 유틸리티 (Application이 상속) |
| `RenderGraph` | Engine/Public/Render | 렌더 패스 기술서 |
| `IRenderPass` | Engine/Public/Render | 패스 단위 인터페이스 |
| `IRenderer` | Engine/Public/Render | RenderGraph 실행 인터페이스 |
| `ForwardRenderer` | Engine/Private/Render | 기본 Forward 구현체 |
| `Execute` | Engine/Private | IExecute 구현, 루프 소유 |
| `FreeCamera` | Application/Source | BaseCamera 상속, FPS 이동 |
| `GameApp` | Application/Source | ApplicationManager 구현 |

---

## 3. Application 프로젝트 구조

Application 프로젝트는 두 레이어로 나뉜다.

```
Application/
├── Core/                      ← 자주 사용되는 공통 클래스 (앱 간 재사용)
│   ├── Public/
│   │   ├── Camera/
│   │   │   └── FreeCamera.h   ← BaseCamera 상속, FPS 카메라
│   │   ├── Character/
│   │   │   └── Character.h    ← 기본 캐릭터 (Transform + 이동)
│   │   └── GameObject/
│   │       └── GameObject.h   ← Mesh + Transform 래퍼, RenderScene 제출용
│   └── Private/
│       ├── Camera/
│       ├── Character/
│       └── GameObject/
└── {Source}/                  ← 프로그램 specific (Client 개발자 작성 영역)
    ├── Public/
    │   └── GameApp.h          ← ApplicationManager 구현체 선언
    └── Private/
        └── GameApp.cpp        ← OnUpdate / SubmitRenderState 구현
```

- **`Application/Core/`**: `FreeCamera`, `Character`, `GameObject` 등 게임/앱 개발 시 자주 사용되는 공통 클래스. Engine이 제공하지 않는 앱 레벨 유틸리티. Core의 클래스들은 Engine Public API만 참조한다.
- **`Application/{Source}/`**: 프로젝트 이름에 따라 가변 네이밍. Client 개발자가 작성하는 프로그램 고유 로직. Engine/Core와 무관하게 교체 가능.

> ExecWindows는 `{Source}` 안의 `GameApp`을 생성해 Engine의 `Execute`에 주입한다.
> 타겟이 바뀌면 `{Source}` 디렉토리만 교체하면 된다.

---

## 4. ApplicationManager (Engine/Public)

Application은 렌더링 명령을 직접 내리지 않고, 씬 상태를 갱신하고 스냅샷을 엔진에 제출합니다.

```cpp
// Engine/Public/Manager/ApplicationManager.h
class ApplicationManager
{
public:
    virtual ~ApplicationManager() = default;

    virtual void OnUpdate(float deltaTime) = 0;
    virtual void SubmitRenderState(class RenderScene* scene) = 0;
};
```

### Execute 호출 흐름

```
Engine::Execute::OnLoop()
  ├─ time->Update()
  ├─ input->Update()
  ├─ management->OnUpdate(delta)           ← Application: 게임 로직, Transform 갱신
  │
  ├─ RenderScene scene
  ├─ management->SubmitRenderState(&scene) ← Application: 현재 상태 스냅샷 제출
  │
  ├─ RenderGraph graph
  ├─ graph.AddPass<ForwardPass>(scene)     ← Engine: 렌더 파이프라인 빌드
  ├─ graph.Compile()
  ├─ renderer->Execute(graph, scene)       ← Engine: GPU 커맨드 기록
  └─ Present / Signal
```

---

## 5. RenderScene (Engine/Public/Scene)

Application이 `SubmitRenderState`에서 채우는 **"Snapshot Bucket"**. 로직 스레드와 렌더 스레드의 데이터를 완전히 분리(Isolation)하는 역할을 합니다.

```cpp
using RenderSortKey = uint64_t; // [ Layer(4) | Translucency(1) | Pass(3) | MaterialID(16) | Depth(40) ]

struct RenderCommand
{
    RenderSortKey sortKey;       // Phase 2에서 정렬에 사용
    SceneObjectID objectID;      // 엔진 관리 ID (안전한 수명 관리)
    IMesh*        mesh;          // 렌더링 리소스 참조
    
    // [Memcpy Snapshot Data]
    // 렌더링에 필요한 데이터를 이곳에 복사하여 App 스레드와 절연
    Matrix4x4     worldMatrix;
    MaterialData  materialState; // 재질 파라미터, 투명도 등
};

struct CameraData
{
    Matrix4x4 view;
    Matrix4x4 proj;
};

class RenderScene
{
public:
    void SetCamera(const Matrix4x4& view, const Matrix4x4& proj);
    
    // Phase 1: 단일 스레드 제출 (vector::reserve 사용)
    // Phase 2: 멀티스레드 병렬 제출, 64비트 소팅(SortCommands), Linear Allocator 도입
    void AddCommand(const RenderCommand& cmd);
    void Clear();

    const CameraData&              GetCamera()   const { return camera; }
    const vector<RenderCommand>&   GetCommands() const { return commands; }

private:
    CameraData             camera;
    vector<RenderCommand>  commands;
};
```

> **성능 핵심**: `RenderCommand`에 데이터를 직접 복사(Memcpy)함으로써 안전성을 확보하고, 64비트 소팅 키를 통해 GPU 상태 변경을 최소화하는 최적의 드로우 순서를 결정합니다.

---

## 6. BaseCamera (Engine/Public/Scene)

Engine이 Application에 공급하는 카메라 유틸리티. 순수 가상 `Update()`만 파생에서 구현.

```
BaseCamera                        ← position, yaw, pitch, view, proj 공통 보유. GetView() / GetProjection() 구현. RebuildView() 공통 구현
     ├─ FreeCamera (Application)  ← WASD + 마우스 회전
     ├─ OrbitCamera               ← 타겟 주위 공전 (future)
     └─ FixedCamera               ← 고정 시점 (future)

`ICamera`는 제거. `BaseCamera` 단일 계층이 역할을 충분히 담당.
```

### Phase2: 플랫폼별 NDC(Z축 0~1 vs -1~1) 및 Row/Column Major 컨벤션 차이는 `BaseCamera` 내부 또는 RHI별 `ProjectionMatrix` 생성 유틸리티에서 분기 처리 필수.
---

## 7. RenderGraph (Engine/Public/Render)

렌더 패스의 실행 순서와 리소스 의존성을 기술하는 구조.

### Phase 1 — 순서 기반 패스 목록

```cpp
class RenderGraph
{
public:
    template<typename TPass, typename... Args>
    void AddPass(Args&&... args);

    void Compile();               // Phase 2: 자동 배리어 삽입, 패스 컬링
    void Execute(ICommandList*);

private:
    vector<unique_ptr<IRenderPass>> passes;
};

class IRenderPass
{
public:
    virtual ~IRenderPass() = default;
    virtual void Execute(ICommandList*, const RenderScene&) = 0;
};
```

**Phase 1 패스 구성:**
```
ForwardPass   ← 현재 구현 목표 (메시 드로우)
```

**Phase 2 이후 확장:**
```
GBufferPass   ← Deferred shading
LightingPass
ShadowPass
PostProcessPass
UIPass
```

### Phase 2 — DAG + 자동 배리어

- 패스별 Read/Write 리소스 선언
- `Compile()`에서 토폴로지 정렬 + 배리어 자동 삽입
- Transient resource aliasing (메모리 재사용)
- `RenderPassDesc`의 `LoadAction / StoreAction` (Mobile bandwidth 절감)

---

## 8. IRenderer (Engine/Public/Render)

```cpp
class IRenderer
{
public:
    virtual ~IRenderer() = default;
    virtual void Execute(RenderGraph& graph, const RenderScene& scene, ICommandList* cmd) = 0;
};
```

구현체: `ForwardRenderer` (Engine/Private/Render)

---

## 9. Mesh 시스템

### 9-1. MeshData — CPU 사이드 공통 포맷

`PrimitiveShape`와 FBX 임포터가 동일한 GPU 업로드 경로를 공유하기 위한 중간 포맷.

```cpp
// Engine/Public/Mesh/MeshData.h
struct MeshData
{
    vector<float>    vertices;   // position / normal / uv 등 인터리브
    vector<uint32_t> indices;
    VertexLayout     layout;     // 버텍스 구성 기술자
};
```

소스가 Primitive든 FBX든 `MeshData`를 거쳐 `IRenderDevice::CreateBuffer()`로 GPU에 업로드된다.

```
BoxShape      → GenerateMeshData() → MeshData ─┐
FBX Importer  → Parse()           → MeshData ─┴─→ IRenderDevice::CreateBuffer() → IMesh
```

### 9-2. PrimitiveShape 계층

```
Engine/Public/Mesh/Primitive/
  PrimitiveShape.h     ← virtual MeshData GenerateMeshData() = 0
  BoxShape.h
  SphereShape.h
  CylinderShape.h
  PlaneShape.h
```

`PrimitiveShape`는 GPU 리소스를 직접 보유하지 않는다. 순수하게 버텍스/인덱스 수치 계산만 담당. GPU 업로드는 호출자 책임.

현재 `Engine/Private/Mesh/Cube`는 Phase 1 완료 후 `BoxShape`로 교체된다.

### 9-3. FBX / 에셋 임포터 (future)

```cpp
// Engine/Public/Asset/IModelImporter.h
class IModelImporter
{
public:
    virtual vector<MeshData> Import(const char* path) = 0;
};
```

구현체는 **Assimp** 라이브러리를 사용. `MeshData` 포맷이 동일하므로 렌더링 경로 변경 없음.

#### 레퍼런스

`Agent/ExternalSources/DX11/DirectX11_3D_19/ModelEditor/` 에 Assimp 기반 FBX 변환 파이프라인 레퍼런스가 존재.
렌더링 코드는 DX11 종속이라 재사용 불가. **`Converter.cpp`** + **`Types.h`** 만 참고.

**참고 가능한 항목:**

| 항목 | 내용 |
|---|---|
| Assimp 로딩 플래그 | `ConvertToLeftHanded`, `Triangulate`, `CalcTangentSpace` 등 LH 좌표계 적합 설정 |
| 뼈대 구조 (`asBone`) | Index, Parent, Transform 계층 구축 로직 |
| 스킨 가중치 (`asBoneWeights`) | 정점당 최대 4개 뼈대로 제한 + 정규화 — `MeshData`의 BlendIndices/Weights에 대응 |
| 키프레임 구조 (`asKeyframeData`) | SRT 분해 방식 (Scale / Rotation / Translation 분리) — 보간에 유리 |
| 바이너리 사전 변환 포맷 | `.mesh` / `.clip` 오프라인 변환 — 런타임 로딩 최적화 참고 |

**레퍼런스 → VaEngine 매핑:**
```
asMesh (레퍼런스)  →  MeshData
asClip            →  AnimationClip (future)
asBone            →  BoneData (future)
asBlendWeight     →  MeshData 내 BlendIndices / BlendWeights 필드
```

**주의 — 레퍼런스 버그:**
`Converter.cpp ReadSkinData` 내 루프에서 `boneWeights[i].Normalize()` 의 `i`는
외부 루프(메시 인덱스)가 잘못 사용된 것. `w`(내부 루프 인덱스)로 수정 후 참고.

---

## 10. 리소스 인터페이스 통합 (IBuffer / IShader)

### 9-1. IBuffer 통합

`IConstantBuffer`, `IResourceBuffer`를 통합하여 콘솔의 유연한 메모리 접근을 수용.

```cpp
class IBuffer : public IRHIResource
{
public:
    virtual void* Map()   = 0;
    virtual void  Unmap() = 0;
};
```

### 9-2. IShader 도입

경로 기반 로딩에서 바이너리 기반 추상화로 전환.

```cpp
struct ShaderLoadDesc
{
    const void*  data;      // DXIL(Xbox), SPIR-V(Android), AGC Binary(PS)
    size_t       dataSize;
    EShaderStage stage;
};

class IShader
{
public:
    virtual ~IShader() = default;
};
```

---

## 10. RenderPass 개념 (ICommandList 확장)

Mobile/Console 최적화를 위한 Load/Store Action 명시 구조.

```cpp
struct RenderPassAttachment
{
    IResourceView* view;
    ELoadAction    loadAction;   // Clear, Load, DontCare
    EStoreAction   storeAction;  // Store, DontCare
    float          clearColor[4];
};

struct RenderPassDesc
{
    uint32_t             renderTargetCount;
    RenderPassAttachment renderTargets[8];
    RenderPassAttachment depthStencil;
};

// ICommandList 추가 메서드
virtual void BeginRenderPass(const RenderPassDesc& desc) = 0;
virtual void EndRenderPass()                             = 0;
```

---

## 11. IRenderDevice 팩토리 확장

```cpp
virtual unique_ptr<IShader>        CreateShader       (const ShaderLoadDesc&    desc) = 0;
virtual unique_ptr<IBuffer>        CreateBuffer       (const BufferDesc&     desc)    = 0;
virtual unique_ptr<ISampler>       CreateSampler      (const SamplerDesc&       desc) = 0;
virtual unique_ptr<IPipelineState> CreatePipelineState(const PipelineStateDesc& desc) = 0;
```

---

## 12. 플랫폼별 고려사항

- **Xbox**: DX12 유사, 전용 메모리 할당 정책 필요 (Buffer 생성 시 플래그로 대응).
- **PlayStation**: 저수준 메모리 관리. `IBuffer::Map/Unmap` + Alignment 필수.
- **Android**: `BeginRenderPass`의 `DontCare` 액션이 대역폭 절감 핵심.

---

## 13. 실행 우선순위

### Phase 1 — 구조 확립 (현재 진행)

1. `ApplicationManager` 인터페이스 생성 (Engine/Public/Manager)
2. `Execute` Engine/Private으로 이동, 렌더 루프 소유
3. Application 구조 재편
   - `Application/Core/` — 공통 유틸리티
   - `Application/{Source}/` — Client 고유 로직 (`GameApp`)
4. `RenderScene` 스냅샷 구조 생성
5. `BaseCamera` 도입, `ICamera` 제거
6. `FreeCamera` → `Application/Core/Camera/`
7. `RenderGraph` (순서 기반 패스 목록)
8. `IRenderer` / `ForwardRenderer`
9. `MeshData` 공통 포맷 + `PrimitiveShape` / `BoxShape` 도입, `Cube` 교체
10. `CubeRenderer` 제거 (test 역할 종료)

### Phase 2 — 성능 최적화 (이후)

- RenderGraph DAG + 자동 배리어
- 64비트 소팅 키 + `SortCommands()`
- Transient resource aliasing
- `IBuffer` / `IShader` 통합
- `RenderPassDesc` Load/Store Action (멀티 스레드 타겟)
- `IFence` OS 종속성 제거
