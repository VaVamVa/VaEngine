# 2026-05-08 Log

## Start Log

- Phase 1 리팩터링(Refactoring_Public.md 기준) 완료, 실행 확인됨
- Phase 2 리팩터링 진행 중 (IShader, Render 백엔드 분리)
- Input System 개선 작업 병행

---

## Phase 1 완료 요약

> 전체 설계 근거: `Agent/AgentLog/Plan/Refactoring_Public.md`

Refactoring_Public §13 Phase 1 항목 기준 완료 상태:

| 항목 | 상태 |
|---|---|
| `ApplicationManager` 인터페이스 (Engine/Public/Manager) | ✅ |
| `Execute` Engine/Private 이동, 렌더 루프 소유 | ✅ |
| `Application/Core/` + `Application/Source/` 구조 재편 | ✅ |
| `RenderScene` 스냅샷 구조 | ✅ |
| `BaseCamera` 도입, `ICamera` 제거 | ✅ |
| `FreeCamera` → `Application/Core/Camera/` | ✅ |
| `RenderGraph` (순서 기반 패스 목록) | ✅ |
| `IRenderer` / `ForwardRenderer` | ✅ |
| `CubeRenderer` 제거 (Agent glob 제거로 컴파일 제외) | ✅ |

Phase 1 미완: `MeshData` / `PrimitiveShape` / `BoxShape` 도입 (`Cube` 교체) 는 Phase 2 이후

---

## Phase 2 작업 내역

### IBuffer 통합 (§10 IBuffer 통합)

- `IConstantBuffer` + `IResourceBuffer` → `IBuffer : IRHIResource` 단일 인터페이스
- `Map() / Unmap() / Upload()` 통합
- `Buffer_DirectX`: CBV 256바이트 정렬 자동 처리 (`EBufferUsage::ConstantBuffer` 시)
- `IRenderDevice::CreateBuffer(const BufferDesc&)` 단일 팩토리

### IShader 도입 (§10 IShader)

현재 구현은 Refactoring_Public §10의 바이너리 기반 설계와 방향만 동일하며, 구체 구현은 HLSL 런타임 컴파일 형태임 (향후 바이너리 전환 예정).

**변경 파일**:
- `Engine/Public/RHI/Shader/IShader.h` — `ShaderDesc` + `IShader` 인터페이스
- `Engine/Private/RHI/DirectX/Shader/Shader_DirectX.h/.cpp` — D3DCompileFromFile, VS/PS blob 보관
- `Engine/Public/RHI/Pipeline/PipelineDesc.h` — `vsPath`/`psPath` 제거 → `IShader* shader`
- `Engine/Private/RHI/DirectX/Pipeline/PipelineState_DirectX.cpp` — 인라인 컴파일 제거, `Shader_DirectX` 캐스트로 blob 사용
- `Engine/Public/RHI/IRenderDevice.h` — `CreateShader(const ShaderDesc&)` 추가
- `Engine/Private/RHI/DirectX/RenderDevice_DirectX.h/.cpp` — `CreateShader` 구현

### Render 소스 백엔드 분리

`ForwardRenderer`, `RenderGraph`, `Execute` 가 백엔드 무관 코드임에도 `if(USE_DIRECTX)` 블록에 있었던 문제 해결.

**핵심 변경**:
- `IRenderer::Initialize(IRenderDevice*, const ShaderDesc&)` — 셰이더 경로를 외부(Execute)에서 주입
- `ForwardRenderer.cpp` — `DX_SHADER_DIR` 완전 제거, `ShaderDesc` 파라미터로 수신
- `Execute.cpp` — `SHADER_DIR` 매크로 + `#ifdef USE_DIRECTX / USE_VULKAN` 로 백엔드별 경로 분기
- `CMakeLists.txt` — `Execute.cpp`, `RenderGraph.cpp`, `ForwardRenderer.cpp` 공용 Private으로 이동 / `DX_SHADER_DIR` → `SHADER_DIR`

**결과**: `Private/Render/` 및 `Private/Execute.cpp`는 이제 백엔드 무관. DX12 코드는 `Private/RHI/DirectX/` 안에만 존재.

---

## Input System 개선

### AxisProcessor → AxisInputCallback (void 형 콜백)

- 기존 `float(float)` 변환 프로세서 → `void(float)` 이벤트 콜백으로 교체
- `GetAxis()`는 여전히 `raw * scale` 반환 (폴링 방식 유지)
- 콜백은 그 외에 추가로 호출 (push 방식 병행)

### EInputTrigger + KeyBoolBinding

```cpp
enum class EInputTrigger : uint8_t { Down, Press, Up };
```

- `MapKeyBool(action, key, EInputTrigger, ActionCallback)` 추가
- Down(첫 프레임) / Press(유지 중 매 프레임) / Up(뗀 첫 프레임) 개별 등록 가능
- 같은 키에 Down + Up 동시 등록 지원

### PointerButtonBinding

- `MapPointerButton(action, EPointerButton, EInputTrigger, ActionCallback)` 추가
- 마우스 버튼도 키와 동일한 Down/Press/Up 콜백 체계

### TransparentStringHash — unordered_map 조회 최적화

```cpp
struct TransparentStringHash {
    using is_transparent = void;
    size_t operator()(std::string_view sv) const noexcept { ... }
};
```

- `GetAxis(const char*)` / `IsTriggered(const char*)` 호출 시 임시 `std::string` 생성 제거
- C++20 이형 조회(heterogeneous lookup) 활용 (`is_transparent` + `std::equal_to<>`)
- 적용 대상: `InputContext::bindings`, `InputSystem::actionValues`

---

## FreeCamera SRP 수정

**문제**: `FreeCamera`가 `IPointerInput`을 직접 참조 (커서 모드 제어, 우클릭 감지) → SRP 위반

**해결**:
- `FreeCamera.cpp` — `IPointerInput` include 완전 제거, `isActive` 플래그로만 동작 제어
- `FreeCamera.h` — `SetActive(bool)` / `IsActive()` 추가
- `VaProgramName.cpp` — `MapPointerButton`으로 우클릭 Down/Up 시 커서 모드 전환 + 카메라 활성화
- 결과: 우클릭을 누르는 동안에만 WASD + 마우스 Look 활성화

**Pitch 부호 수정** (Y축 반전 문제):
- Windows 화면 좌표 `deltaY`는 아래로 내릴 때 양수
- 기존 `pitch += GetAxis("LookY")` → 마우스 아래 = 시점 위 (Y반전)
- 수정: `pitch -= GetAxis("LookY") * lookSensitivity`

---

## Step 5 — ITexture + Diffuse 텍스처

✅ 완료 (이전 세션)

- `ITexture` 인터페이스 + `Texture_DirectX` 구현 (stb_image, DEFAULT/UPLOAD heap 업로드)
- `BindingLayout_DirectX`: `EBindingType::Texture` → Descriptor Table + Static Sampler
- `CubeShader.hlsl`: UV 입력, `gDiffuse.Sample` 적용

---

## Step 5.5 — 입력 시스템

✅ 완료 (이전 세션, 이번 세션에서 개선)

- `IKeyInput` / `IPointerInput` Physical Layer
- `InputContext` / `InputSystem` Logical Layer
- `ITime`: `std::chrono` 기반 단일 구현

---

### RenderPassDesc + BeginRenderPass / EndRenderPass (§10)

✅ 완료 — 실행 확인

| 파일 | 변경 내용 |
|---|---|
| `Engine/Public/RHI/Common_RHI.h` | `ELoadAction`, `EStoreAction`, `RenderPassAttachment`, `RenderPassDesc` 추가 |
| `Engine/Public/RHI/ICommandList.h` | `BeginRenderPass(const RenderPassDesc&)` / `EndRenderPass()` 순수 가상 추가 |
| `Engine/Private/RHI/DirectX/CommandList_DirectX.h/.cpp` | override 구현 — `OMSetRenderTargets` + `loadAction == Clear` 시 `ClearRenderTargetView` |
| `Engine/Private/Execute.cpp` | `SetRenderTarget + ClearRenderTargetView` → `BeginRenderPass / EndRenderPass` 교체 |

---

### RenderGraph DAG + 자동 배리어 (§7 Phase 2)

✅ 완료 — 실행 확인

**설계 요점**:
- `IRenderPass::DeclareResources(reads, writes)` — 패스가 사용할 리소스/상태 선언
- `RenderGraph::ImportResource(resource, state)` — 외부 리소스(스왑체인 등) 초기 상태 등록
- `RenderGraph::Compile()` — 패스 순서대로 선언된 상태와 현재 추적 상태를 비교, 차이 있으면 `preBarriers` 생성
- `RenderGraph::Execute()` — 패스마다 `preBarriers` 자동 삽입 후 `Execute` 호출
- `ForwardPass` (파일 로컬 struct) — `DeclareResources`에서 `writes(backBuffer, RenderTarget)` 선언; `Execute`에서 `BeginRenderPass / Viewport / Scissor / Render / EndRenderPass` 담당

| 파일 | 변경 내용 |
|---|---|
| `Engine/Public/Render/IRenderPass.h` | `PassResourceDecl` 구조체 + `DeclareResources()` default virtual 추가 |
| `Engine/Public/Render/IRenderer.h` | `FrameOutput` 구조체 도입; `Execute` → `AddPasses(graph, output)` |
| `Engine/Public/Render/RenderGraph.h` | `PassEntry(pass + preBarriers)`, `resourceStates` 맵, `ImportResource / Compile / Execute / GetCurrentState` |
| `Engine/Private/Render/RenderGraph.cpp` | `Compile()` 배리어 계산, `Execute()` 배리어 삽입 + 패스 실행, `GetCurrentState()` 구현 |
| `Engine/Private/Render/ForwardRenderer.h/.cpp` | `ForwardPass` (파일 로컬), `AddPasses()`, `Render()` 분리 |
| `Engine/Private/Execute.cpp` | 수동 배리어 제거; `ImportResource → AddPasses → Compile → Execute → GetCurrentState`로 Present 배리어 |

---

### SortCommands — 64비트 키 정렬 (§5)

✅ 완료 — 실행 확인

**Key 레이아웃**: `[ Layer(4) | Translucency(1) | Pass(3) | MaterialID(16) | Depth(40) ]`

| 파일 | 변경 내용 |
|---|---|
| `Engine/Public/Scene/RenderScene.h` | `SortKeyDesc` 구조체 + `MakeSortKey()` 인라인 함수 추가 |
| `Engine/Public/Scene/RenderScene.h` | `RenderScene::SortCommands()` — `std::sort` 오름차순 (Layer → Translucency → Pass → MaterialID → Depth 우선순위) |
| `Engine/Private/Execute.cpp` | `SubmitRenderState` 직후 `scene.SortCommands()` 호출 |

정렬 규칙:
- 불투명(translucent=0): depth 작을수록 먼저 (front-to-back, 오버드로 최소화)
- 반투명(translucent=1): depth 클수록 먼저 (back-to-front, 알파 블렌딩 정확도)

---

### IShader 바이너리 기반 전환

✅ 완료 — 실행 확인

**변경 이유**:

| 문제 (D3DCompileFromFile, 런타임 컴파일) | 해결 (D3DReadFileToBlob, 오프라인 컴파일) |
|---|---|
| 셰이더 오류가 앱 실행 시점에야 발견됨 | 빌드 시점에 FXC가 즉시 오류 보고 → CI/CD 자동 검증 가능 |
| 앱 시작마다 HLSL 파싱 + 최적화 수행 | `.cso` 파일 읽기만 수행 → 시작 시간 단축 |
| 배포 패키지에 HLSL 소스 포함 필요 | 바이너리만 배포, 소스 비노출 |
| `D3DCompiler_47.dll` 의존성이 배포 빌드에도 노출 | 로드 경로가 단순 파일 I/O로 분리되어 향후 DLL 없는 로더로 대체 가능 |
| DX12 전용 컴파일러 API 사용 | `D3DReadFileToBlob` → 향후 `fread` 기반 로더로 추상화하면 백엔드 무관 구조 가능 |

| 파일 | 변경 내용 |
|---|---|
| `Engine/Private/RHI/DirectX/Shader/Shader_DirectX.h/.cpp` | `Compile()` → `Load()` — `D3DCompileFromFile` 제거, `D3DReadFileToBlob`으로 `.cso` 바이너리 로드 |
| `Engine/Private/RHI/DirectX/RenderDevice_DirectX.cpp` | `CreateShader` 내부에서 `Compile` → `Load` 호출 |
| `Engine/Private/Execute.cpp` | `SHADER_DIR/CubeShader.hlsl` → `SHADER_DIR/CubeShader_VS.cso` / `CubeShader_PS.cso` |
| `Engine/CMakeLists.txt` | FXC 탐색 (`C:/Program Files (x86)/Windows Kits/10/bin/*/x64/fxc.exe`), `add_custom_command`으로 `.hlsl` → `.cso` 오프라인 컴파일, `SHADER_DIR` 매크로를 `.cso` 출력 디렉토리로 변경 |

---

### 백엔드 무관 셰이더 로딩 + 공용 HLSLI + 프로젝트 로컬 컴파일러

**변경 이유**:

| 문제 | 해결 |
|---|---|
| FXC가 Windows SDK 고정 경로(`C:/Program Files (x86)/...`)에 의존 → 환경마다 경로 다름 | `VaShaderCompiler` 빌드 타깃으로 FXC 대체 → CMake가 경로 자동 결정 |
| `D3DReadFileToBlob`은 DX API 호출 → Shader 로드가 백엔드 종속 | `BinaryLoader.h` (`std::ifstream` 기반) + `D3DCreateBlob`으로 분리 → 파일 I/O는 순수 C++ |
| HLSL 공용 헬퍼(변환행렬, 조명, 샘플러)가 없어 셰이더마다 중복 선언 | `_Shaders/Common/` 공용 라이브러리 계층 신설 |
| HLSL 소스가 `Private/RHI/DirectX/` 안에 있어 백엔드 의존성 암시 | `Engine/_Shaders/DirectX/` (백엔드별) + `Engine/_Shaders/Common/` (공통) 로 분리 |

**파일 구조**:
```
Engine/
├── _Shaders/
│   ├── Common/
│   │   ├── Global.hlsli       # CB_PerFrame(b0), CB_World(b1), 버텍스 구조체, NormalSampleToWorldSpace
│   │   ├── Sampler.hlsli      # LinearSampler(s0), PointSampler(s1)
│   │   └── Lighting.hlsli     # DirectionalLight/PointLight/SpotLight, CB_Lights(b2), Compute* 함수
│   └── DirectX/
│       └── CubeShader.hlsl    # Sampler.hlsli include, LinearSampler 사용
└── Private/
    └── Utilities/
        └── BinaryLoader.h     # LoadBinaryFile(wchar_t*) → vector<uint8_t>, 순수 C++
Tools/
└── ShaderCompiler/
    ├── CMakeLists.txt
    └── Main.cpp               # --in/--stage/--entry/--out/--debug CLI, D3DCompileFromFile + D3D_COMPILE_STANDARD_FILE_INCLUDE
```

| 파일 | 변경 내용 |
|---|---|
| `Tools/ShaderCompiler/Main.cpp` | 신규 — `D3DCompileFromFile` CLI 래퍼. `--in/--stage/--entry/--out/[--debug]` |
| `Tools/ShaderCompiler/CMakeLists.txt` | 신규 — `add_executable(VaShaderCompiler)`, `d3dcompiler` 링크 |
| `Engine/_Shaders/Common/Global.hlsli` | 신규 — CB_PerFrame/CB_World, 버텍스 입출력 구조체, NormalSampleToWorldSpace |
| `Engine/_Shaders/Common/Sampler.hlsli` | 신규 — LinearSampler(s0), PointSampler(s1) |
| `Engine/_Shaders/Common/Lighting.hlsli` | 신규 — 3종 라이트 구조체 + CB_Lights(b2) + Compute 함수 (그림자 섹션 TODO 주석) |
| `Engine/_Shaders/DirectX/CubeShader.hlsl` | 신규 위치 — `../Common/Sampler.hlsli` include, `LinearSampler` 사용 |
| `Engine/Private/Utilities/BinaryLoader.h` | 신규 — `std::ifstream` 기반 바이너리 로더, DX API 없음 |
| `Engine/Private/RHI/DirectX/Shader/Shader_DirectX.cpp` | `D3DReadFileToBlob` → `LoadBinaryFile` + `D3DCreateBlob` |
| `Engine/CMakeLists.txt` | FXC 고정경로 탐색 제거 → `$<TARGET_FILE:VaShaderCompiler>`, 셰이더 소스 경로 `_Shaders/DirectX/`로 변경 |
| `CMakeLists.txt` (루트) | `USE_DIRECTX` 시 `add_subdirectory(Tools/ShaderCompiler)` 추가 |

**주의**: `Engine/Private/RHI/DirectX/_Shaders/CubeShader.hlsl` (구 위치)는 더 이상 참조되지 않음. IDE에서 삭제 권장.

---

### VaShaderCompiler 멀티 백엔드 확장 구조

✅ 완료 — 실행 확인

**변경 이유**:

| 문제 | 해결 |
|---|---|
| `Main.cpp`에 DX 전용 코드가 조건 없이 컴파일됨 | `#ifdef USE_DIRECTX` / `#ifdef USE_VULKAN` 분기로 백엔드별 코드 격리 |
| Vulkan 추가 시 전체 재설계 필요 | `--target dx\|vk` 인자 + 백엔드별 함수(`CompileHLSL`, `CompileGLSL`) 구조로 확장 가능 |
| FetchContent로 못 가져오는 FXC 대신 shaderc는 GitHub 오픈소스 | `USE_VULKAN` 시 `FetchContent(shaderc)` 연동 경로 확보 |

| 파일 | 변경 내용 |
|---|---|
| `Tools/ShaderCompiler/Main.cpp` | `--target dx\|vk` 인자 추가, `#ifdef USE_DIRECTX`로 DX 코드 격리, `CompileGLSL` 스텁(`USE_VULKAN`) |
| `Tools/ShaderCompiler/CMakeLists.txt` | `if(USE_DIRECTX)` → `d3dcompiler` 링크 + 정의 주입 / `if(USE_VULKAN)` → `shaderc` FetchContent 연동 |
| `CMakeLists.txt` (루트) | `if(USE_DIRECTX)` → `if(USE_DIRECTX OR USE_VULKAN)` |
| `Engine/CMakeLists.txt` | 커스텀 커맨드에 `--target dx` 명시 |

---

### IFence OS 종속성 제거

**변경 이유**:

| 문제 | 해결 |
|---|---|
| `IFence::SetEventOnComplete(uint64_t, void*)` — `void*`가 Windows `HANDLE`을 공개 인터페이스로 노출 | `Wait(uint64_t)` 단일 메서드로 교체 — OS 핸들은 `Fence_DirectX` 내부로 완전 이동 |
| `Execute.cpp`가 `while(GetCompletedValue() < value)` 스핀 루프 사용 → CPU 100% 점유 | `frameFence->Wait(value)` 호출 → 내부에서 `WaitForSingleObject(INFINITE)` 사용, CPU 양보 |
| `HANDLE` 생성/해제 책임이 호출자(Execute)에게 있음 | `Fence_DirectX`가 `Register()`에서 생성, 소멸자에서 `CloseHandle()` → RAII |

| 파일 | 변경 내용 |
|---|---|
| `Engine/Public/RHI/IFence.h` | `SetEventOnComplete` 제거 → `Wait(uint64_t)` 추가 |
| `Engine/Private/RHI/DirectX/Fence_DirectX.h` | `~Fence_DirectX()` 추가, `HANDLE fenceEvent` 멤버 추가 |
| `Engine/Private/RHI/DirectX/Fence_DirectX.cpp` | `Register()`에서 `CreateEventW` → `Wait()`에서 `SetEventOnCompletion + WaitForSingleObject` → 소멸자에서 `CloseHandle` |
| `Engine/Private/Execute.cpp` | `OnLoop` + `OnRelease` 스핀 루프 → `frameFence->Wait(value)` |

---

---

### MeshData + PrimitiveShape 도입

✅ 완료

**변경 이유**:

| 문제 | 해결 |
|---|---|
| `Cube`가 `BuildGeometry()` 오버라이드로 정점 데이터 생성 — GPU 업로드와 CPU 데이터 분리 불가 | `MeshData` (순수 CPU 구조체) + `PrimitiveShape` (빌더) 분리 |
| `IMesh::Initialize(IRenderDevice*)` — 인터페이스가 특정 초기화 전략을 강제 | `IMesh`에서 `Initialize` 제거, `MeshPrimitive::Initialize(IRenderDevice*, const MeshData&)`로 이동 |
| `Cube` 8정점 공유 → 면 경계에서 UV 불연속 심(seam) 발생 | `CubeShape` 24정점(면당 4개) — 면별 UV 독립 |
| `MeshPrimitive`가 protected `vertexData/indexData` 공개 → 서브클래스가 버퍼 직접 조작 | `vertexByteSize/indexCount/vertexStride` private, 외부에서 `MeshData`만 주입 |

**신규 파일**:

| 파일 | 내용 |
|---|---|
| `Engine/Public/Mesh/MeshData.h` | `PrimitiveVertex {pos[3], color[4], uv[2]}` + `MeshData {vertices, indices, vertexStride}` |
| `Engine/Public/Mesh/PrimitiveShape.h` | `virtual MeshData Build() const = 0` 추상 기반 |
| `Engine/Public/Mesh/CubeShape.h/.cpp` | 24정점 큐브, DX12 CW 와인딩, 면별 UV |
| `Engine/Public/Mesh/UVSphereShape.h/.cpp` | (stacks+1)×(slices+1) 정점, 구면 UV, DX12 CW 쿼드 분할 |
| `Engine/Public/Mesh/IcoSphereShape.h/.cpp` | 정20면체 + N회 세분, 구면 투영, phi=atan2 UV |

**변경 파일**:

| 파일 | 변경 내용 |
|---|---|
| `Engine/Public/Mesh/IMesh.h` | `Initialize(IRenderDevice*)` 제거, `Draw(ICommandList*)` 만 보유 |
| `Engine/Public/Mesh/MeshPrimitive.h` | `BuildGeometry()` 제거, `Initialize(IRenderDevice*, const MeshData&)` 추가, protected 데이터 멤버 → private 크기 필드 |
| `Engine/Private/Mesh/MeshPrimitive.cpp` | 신 `Initialize` 구현 — `MeshData`에서 버퍼 생성 |
| `Engine/Public/Mesh/Cube.h` | deprecated redirect(`#include "CubeShape.h"`) |
| `Engine/Private/Mesh/Cube.cpp` | 내용 비움 (IDE에서 파일 삭제 권장) |
| `Application/Source/Public/VaProgramName.h` | `IMesh` → `MeshPrimitive` |
| `Application/Source/Private/VaProgramName.cpp` | `Cube` → `CubeShape{}.Build()` 패턴 |

**와인딩 분석 요약**:
- DX12 `FrontCounterClockwise=FALSE` + screen Y-down → 2D 부호면적 양수 = 전면(front-face)
- CubeShape: 면별 v0→v1→v2 CW 확인 ✓
- UVSphere: 쿼드 `(v00,v10,v01)`, `(v10,v11,v01)` CW ✓
- IcoSphere: 표준 CCW 면 목록 좌우 반전(`{a,b,c}` → `{a,c,b}`) 후 세분 시 CW 유지 ✓

---

---

## Transient Resource Aliasing (Structural)

✅ 완료

**변경 이유**:

| 문제 | 해결 |
|---|---|
| `ForwardRenderer`가 `depthBuffer`를 소유 → 렌더러가 리소스 수명까지 관리하는 SRP 위반 | `depthBuffer` 소유권을 `RenderGraph.transientDepths`로 이전 |
| `RenderGraph`가 매 프레임 stack에서 생성/소멸 → 트랜지언트 GPU 리소스도 매 프레임 재생성 | `RenderGraph`를 `Execute` 멤버로 영속화, `Reset()`으로 패스만 초기화 |
| `Compile()`이 IRenderDevice 없이 호출 → 트랜지언트 리소스를 그래프 내부에서 생성 불가 | `Compile(IRenderDevice*)` 로 변경, 미생성 트랜지언트만 생성 후 재사용 |
| 패스가 트랜지언트 포인터를 얻을 시점 없음 → `DeclareResources()` 호출 전에 포인터 필요 | `IRenderPass::OnCompile(RenderGraph&)` 추가 — 트랜지언트 생성 직후, 배리어 계산 직전 호출 |
| 실제 GPU 메모리 aliasing 미구현 | `CreatePlacedResource` + 공유 heap 기반 bin-packing은 향후 작업 (TODO) |

**변경 파일**:

| 파일 | 변경 내용 |
|---|---|
| `Engine/Public/Render/IRenderPass.h` | `OnCompile(RenderGraph&)` default virtual 추가 |
| `Engine/Public/Render/RenderGraph.h` | `TransientDepthDesc`, `DeclareTransientDepth()`, `GetTransientDepth()`, `Reset()`, `Compile(IRenderDevice*)` 추가 |
| `Engine/Private/Render/RenderGraph.cpp` | 트랜지언트 생성·재사용 로직, `OnCompile` 호출, `Reset()` 구현 |
| `Engine/Public/Render/IRenderer.h` | `Initialize` 시그니처에서 `width, height` 제거 (depth 크기는 `FrameOutput`에서 결정) |
| `Engine/Private/Render/ForwardRenderer.h` | `depthBuffer` unique_ptr + `GetDepthBuffer()` 제거 |
| `Engine/Private/Render/ForwardRenderer.cpp` | `Initialize`에서 depth 생성 제거, `AddPasses`에서 `DeclareTransientDepth` 호출, `ForwardPass::OnCompile`으로 포인터 확보 |
| `Engine/Private/Execute.h` | `RenderGraph renderGraph` 멤버 추가, `#include "Render/RenderGraph.h"` 이동 |
| `Engine/Private/Execute.cpp` | 로컬 `graph` → 멤버 `renderGraph`, `Reset()` + `Compile(device)` 호출로 변경 |

**프레임 루프 흐름**:
```
renderGraph.Reset()                         // 패스·상태 초기화, 트랜지언트 GPU 리소스 유지
renderGraph.ImportResource(backBuffer, Present)
renderer.AddPasses(renderGraph, output)
  └─ DeclareTransientDepth(w, h, fmt)       // 동일 desc면 기존 리소스 핸들 반환
     AddPass<ForwardPass>(..., depthHandle)

renderGraph.Compile(device)
  ├─ 미생성 트랜지언트 CreateDepthBuffer()  // 첫 프레임만 실제 생성
  ├─ OnCompile() → ForwardPass.depthBuffer = GetTransientDepth(handle)
  └─ DeclareResources() → 배리어 계산

renderGraph.Execute(cmdList, scene)
```

---

## ToDo

### Phase 3 (Refactoring 범주)

- [x] Transient resource aliasing (structural — GPU heap aliasing은 향후 작업)
