# VaEngine 멀티 API 다형성 아키텍처 계획

- 작성일: 2026-05-08
- 분석 기반: DirectX11_3D_19 레퍼런스 + 현재 VaEngine RHI 구조

---

## 1. DX11 레퍼런스 시스템 분류

레퍼런스를 분석하면 각 시스템의 "API 의존성 위치"가 명확하게 분리된다.

| 시스템 | API 독립 부분 | API 종속 부분 |
|---|---|---|
| **Vertex Layout** | 정점 구조체 (POD) | InputLayout 생성 (DX12: PSO, Vulkan: VkVertexInputState) |
| **Mesh** | 정점/인덱스 데이터 배열, `Create()` 가상 구조 | GPU 버퍼 생성 및 바인딩 |
| **Transform** | Position, Rotation, Scale → Matrix4x4 계산 | GPU에 업로드하는 방식 (CBV / UBO) |
| **Camera/Projection** | View, Projection 행렬 계산 | PerFrame 상수 버퍼에 업로드하는 방식 |
| **Material** | Ambient, Diffuse, Specular, Emissive 데이터 | 셰이더 바인딩 방식 |
| **Lighting** | Direction, Color, Attenuation 데이터 | 셰이더 바인딩 방식 |
| **Shader** | 셰이더 로직 (HLSL/GLSL) | PSO 생성, Root Signature, Pipeline Layout |
| **Texture** | 이미지 데이터 로딩 | GPU 리소스 생성, SRV/VkImageView |
| **RenderTarget** | 해상도, 포맷 정의 | RTV/DSV Heap (DX12), Framebuffer+Attachment (Vulkan) |
| **ConstantBuffer** | 업로드할 데이터 구조체 | CBV 바인딩 (DX12) vs UBO Descriptor (Vulkan) |

---

## 2. 현재 VaEngine RHI와의 매핑

```
DX11 개념                   VaEngine 현재 상태
─────────────────────────────────────────────────────
ID3D11Buffer (VB/IB)      → IGPUBuffer ✅
ID3D11RenderTargetView    → IResourceView ✅
cbuffer / CB_World        → IGPUBuffer (Upload) ⚠️  인터페이스 미분리
Technique / Pass          → (없음) ❌  IPipelineState 필요
ID3D11InputLayout         → PSO에 포함됨 (DX12 방식) ✅
Camera (View/Proj Matrix) → (없음) ❌  ICamera 필요
Mesh abstract             → (없음) ❌  IMesh 필요
Material                  → (없음) ❌  IMaterial 필요
```

---

## 3. 핵심 추상화 난이도 분류

### 🟢 낮음 — 거의 동일한 개념, 래핑만 필요

- **IGPUBuffer** (Vertex/Index) — DX12 `CreateCommittedResource` ↔ Vulkan `vkCreateBuffer`
- **IFence / Sync** — DX12 `ID3D12Fence` ↔ Vulkan `VkFence/VkSemaphore`
- **ISwapChain** — DX12 `IDXGISwapChain` ↔ Vulkan `VkSwapchainKHR`

### 🟡 중간 — 개념은 같으나 API 구조 차이 있음

- **IConstantBuffer** — DX12 CBV(Root Parameter) ↔ Vulkan UBO(Descriptor Set)
- **ITexture** — DX12 Resource+SRV ↔ Vulkan Image+ImageView+Sampler
- **IResourceView (RTV/DSV)** — DX12 Descriptor Heap ↔ Vulkan `VkRenderPass` Attachment

### 🔴 높음 — 철학적으로 다름, 가장 신중하게 설계 필요

- **IPipelineState (PSO)** — DX12: 모든 상태를 하나의 PSO로 묶음. Vulkan: VkPipeline 동일하지만 `VkRenderPass` 종속성이 추가로 필요
- **IBindingLayout (Root Signature / Pipeline Layout)** — DX12: Root Parameter(CBV/SRV/UAV). Vulkan: Descriptor Set Layout + VkPipelineLayout. 바인딩 모델이 근본적으로 다름
- **RenderPass 개념** — DX12: ResourceBarrier + OMSetRenderTargets. Vulkan: vkBeginRenderPass + Framebuffer + Attachment (명시적 서브패스)

---

## 4. 제안 계층 구조

```
┌─────────────────────────────────────────────────────────┐
│  Engine / Public  (API 완전 독립)                        │
│                                                          │
│  Core/Math         Vector3, Matrix4x4, Quaternion        │
│  Core/RHI          공통 Desc 구조체, Enum                 │
│                                                          │
│  RHI 인터페이스     IRenderDevice, ICommandList           │
│                    IGPUBuffer, IResourceView              │
│  신규 필요           IConstantBuffer, IPipelineState      │
│                    IBindingLayout, ITexture               │
│                                                          │
│  Scene 인터페이스   IMesh, ICamera, IMaterial, ILight     │
└─────────────────────────────────────────────────────────┘
         │ implements
┌─────────────────────────────────────────────────────────┐
│  Engine / Private / RHI / DirectX  (DX12 구현)           │
│                                                          │
│  RenderDevice_DirectX, CommandList_DirectX               │
│  GPUBuffer_DX12, ConstantBuffer_DX12                     │
│  PipelineState_DX12 (PSO + Root Signature)               │
│  Texture_DX12                                            │
└─────────────────────────────────────────────────────────┘
         │ (미래)
┌─────────────────────────────────────────────────────────┐
│  Engine / Private / RHI / Vulkan  (Vulkan 구현)           │
│                                                          │
│  RenderDevice_Vulkan, CommandList_Vulkan                  │
│  GPUBuffer_Vulkan, ConstantBuffer_Vulkan                  │
│  PipelineState_Vulkan (VkPipeline + PipelineLayout)       │
└─────────────────────────────────────────────────────────┘
```

---

## 5. 신규 인터페이스 정의 방향

### 5-1. IConstantBuffer (IGPUBuffer 분리)

현재 `IGPUBuffer`는 Vertex/Index Buffer와 Constant Buffer를 동일하게 취급하지만,
Constant Buffer는 **바인딩 방식**이 완전히 다르다.

```cpp
// Engine/Public/RHI/IConstantBuffer.h
class IConstantBuffer
{
public:
    virtual ~IConstantBuffer() = default;
    virtual void SetData(const void* data, size_t size) = 0;
    // DX12: SetGraphicsRootConstantBufferView(slot, gpuAddress)
    // Vulkan: vkUpdateDescriptorSets + descriptor set bind
    virtual void Bind(ICommandList* cmdList, uint32_t slot) = 0;
};
```

`IRenderDevice::CreateConstantBuffer(size_t size)` 추가 필요.

---

### 5-2. IPipelineState (PSO / VkPipeline 추상화)

DX12와 Vulkan 모두 "파이프라인 상태"를 미리 컴파일한다.
차이점: DX12는 `RootSignature`를 별도 생성, Vulkan은 `PipelineLayout`을 PipelineState 내에 포함.

```cpp
struct PipelineStateDesc
{
    // 셰이더 소스 경로 또는 바이트코드 (API가 컴파일)
    const wchar_t* vsPath;
    const wchar_t* psPath;
    
    // 정점 레이아웃
    const VertexInputDesc* vertexInputs;
    uint32_t               vertexInputCount;
    
    // 렌더 타겟 포맷
    EPixelFormat rtvFormat;
    EPixelFormat dsvFormat;   // EPixelFormat::Unknown = 없음
    
    // 파이프라인 상태
    ECullMode    cullMode;
    EBlendMode   blendMode;
    bool         depthEnable;
    
    // 바인딩 레이아웃
    IBindingLayout* bindingLayout;
};

class IPipelineState
{
public:
    virtual ~IPipelineState() = default;
    // DX12: ID3D12PipelineState + SetPipelineState
    // Vulkan: VkPipeline + vkCmdBindPipeline
    virtual void Bind(ICommandList* cmdList) = 0;
};
```

---

### 5-3. IBindingLayout (Root Signature / PipelineLayout)

리소스 바인딩 구조를 선언하는 인터페이스.

```cpp
struct BindingEntry
{
    EBindingType type;     // ConstantBuffer, Texture, Sampler, UAV
    uint32_t     slot;     // 레지스터 번호
    EShaderStage stage;    // Vertex, Pixel, Compute, All
};

class IBindingLayout
{
public:
    virtual ~IBindingLayout() = default;
    // DX12: ID3D12RootSignature
    // Vulkan: VkDescriptorSetLayout + VkPipelineLayout
};
```

---

### 5-4. IMesh (Mesh 추상화)

DX11의 `Mesh` 추상 클래스와 동일한 역할. GPU 버퍼 소유 + 드로우 커맨드 책임.

```cpp
// Engine/Public/RHI/IMesh.h
class IMesh
{
public:
    virtual ~IMesh() = default;
    virtual void Initialize(IRenderDevice* device) = 0;
    // DX12: IASetVertexBuffers + IASetIndexBuffer + DrawIndexedInstanced
    // Vulkan: vkCmdBindVertexBuffers + vkCmdBindIndexBuffer + vkCmdDrawIndexed
    virtual void Draw(ICommandList* cmdList) = 0;
};
```

구체 구현: `MeshPrimitive`(공통 데이터 보유) → `Cube`, `Sphere`, `Grid`, `Quad`

---

### 5-5. ICamera (View/Projection 행렬 공급)

```cpp
class ICamera
{
public:
    virtual ~ICamera() = default;
    virtual Matrix4x4 GetViewMatrix() const = 0;
    virtual Matrix4x4 GetProjectionMatrix() const = 0;
    virtual void Update(float deltaTime) = 0;
};
```

DX11의 `Camera`, `Freedom`, `Fixity`에 해당. 행렬 계산 자체는 API 독립.
`IConstantBuffer`를 통해 GPU에 업로드하는 방식만 API 종속.

---

## 6. CubeRenderer의 현재 위치와 리팩토링 방향

현재 `CubeRenderer_DX12`는 아래를 모두 소유:
- Root Signature (→ `IBindingLayout`으로 분리 예정)
- PSO (→ `IPipelineState`으로 분리 예정)
- Vertex/Index Buffer (→ `IMesh`로 분리 예정)
- Draw 로직

**단기 (MVP 단계):** 현재 구조 유지. Root Signature에 CBV 파라미터 1개 추가.
**중기 (리팩토링):** PSO와 Mesh를 각자의 인터페이스로 분리.

---

## 7. MVP 상수 버퍼 구현 계획 (즉시 작업)

현재 큐브가 "평면"으로 보이는 문제를 해결하기 위한 다음 단계.

### 구현 순서

```
1. HLSL 수정
   - cbuffer CB_Transform { float4x4 gMVP; };
   - VS: output.pos = mul(float4(input.pos, 1.0), gMVP);

2. Root Signature 수정 (CubeRenderer_DX12)
   - Descriptor Table or Root CBV (register b0) 1개 추가

3. Constant Buffer 리소스 생성
   - Upload Heap, 256 바이트 정렬 (DX12 CBV 요구사항)
   - 현재 IGPUBuffer::Create 로 생성 가능 (Upload 접근, 64 bytes)

4. MVP 행렬 계산 및 업로드
   - Model: 회전 행렬 (매 프레임 업데이트로 회전하는 큐브)
   - View: 카메라 위치 (0, 1, -3) → LookAt (0, 0, 0)
   - Proj: PerspectiveFovLH(45도, 16/9, 0.1, 100)
   - 주의: DX12는 Row-Major vs Column-Major 주의

5. CubeRenderer::Draw에서 CBV 바인딩
   - SetGraphicsRootConstantBufferView(0, cbvGpuAddress)
```

### 수학 라이브러리 선택

DX11은 DirectXMath (`DirectX::XMMatrix*`)를 사용. VaEngine도 동일하게 사용 가능:
- `DirectX::XMFLOAT4X4` — CPU 저장용
- `DirectX::XMMatrixLookAtLH()`, `XMMatrixPerspectiveFovLH()`
- `DirectX::XMMatrixMultiply()`

DirectXMath는 DirectX-Headers에 포함되어 있어 별도 추가 불필요.

---

## 8. 단계별 구현 로드맵

| 단계 | 작업 | 목표 | 상태 |
|---|---|---|---|
| **Step 1** | MVP 상수 버퍼 + 회전 큐브 | 3D 큐브 렌더링 확인 | ✅ |
| **Step 2** | `IConstantBuffer` 인터페이스 분리 | ConstantBuffer 추상화 | ✅ |
| **Step 3** | `IMesh` 인터페이스 + MeshPrimitive 기반 | Cube/Sphere 다형성 | ✅ |
| **Step 4** | `IPipelineState` + `IBindingLayout` | PSO/RootSig 추상화 | ✅ |
| **Step 5** | `ITexture` + Diffuse 텍스처 적용 | 텍스처 렌더링 | ✅ |
| **Step 5.5** | `ITime` + `IKeyInput` + `IMouseInput` | 플랫폼 독립 입력/시간 시스템 | ✅ |
| **Step 6** | `ICamera` (Freedom 카메라) | 키보드+마우스 시점 이동 | 🔜 |
| **Step 7** | `IMaterial` + `ILight` | Phong 조명 | - |
| **Step 8** | Depth Buffer (`IResourceView` 확장) | Z-sorting 정확도 | - |

---

## 9. 다형성 설계 원칙 요약

```
1. 데이터는 API 독립          (Matrix, Vertex struct, Light params)
2. 생성/바인딩은 API 종속     (Create* 메서드가 구현체를 반환)
3. 인터페이스는 커맨드로 표현  (Draw, Bind, Upload — HOW는 숨김)
4. 캐스팅은 구현체 내부에서만  (DX12 클래스 안에서만 static_cast<DX12*>)
5. Public 인터페이스에 DX12/Vulkan 타입 노출 금지
```

---

## 10. Input / Time 시스템 설계 (2026-05-08, v2 업데이트)

### 10-1. 2계층 아키텍처

```
Layer 2 — 논리 입력 (플랫폼 완전 독립)
─────────────────────────────────────────────────
  InputContext   : 물리 키/포인터 → 논리 액션 매핑 정의
  IInputSystem   : 컨텍스트 관리 + 액션 값 쿼리

Layer 1 — 물리 입력 (플랫폼별 구현)
─────────────────────────────────────────────────
  IKeyInput      : EKeyCode, IsDown/IsUp/IsPress
  IPointerInput  : EPointerButton, PointerState, GetState()
  ITime          : Delta(), FPS(), Running()
```

Camera/게임 로직은 **Layer 2 (IInputSystem)만** 사용.
Layer 1은 IInputSystem이 내부적으로 소비.

### 10-2. 파일 구조

```
Engine/Public/System/
    ITime.h            ← unchanged
    IKeyInput.h        ← unchanged (Layer 1, IInputSystem이 소비)
    IPointerInput.h    ← IMouseInput 대체 (EPointerButton::Primary/Secondary)
    InputContext.h     ← NEW: 액션 바인딩 데이터 클래스 (vtable 없음)
    IInputSystem.h     ← NEW: Layer 2 인터페이스

Engine/Private/System/
    Time.cpp                      ← std::chrono, 단일 구현 (모든 플랫폼)
    InputSystem.cpp               ← Layer 2 구현, 플랫폼 독립

Engine/Private/System/Windows/
    KeyInput_Windows.cpp/.h       ← GetKeyboardState
    MouseInput_Windows.cpp/.h     ← GetAsyncKeyState + GetCursorPos
                                     (Windows에서 Pointer = Mouse이므로 이름 유지)

(미래 — 멀티플랫폼 빌드 시스템 구축 단계에서 CMake 조건부 컴파일 설정 예정)
Engine/Private/System/Android/
    KeyInput_Android              ← AInputEvent
    FingerInput_Android           ← AMotionEvent (첫 손가락)

Engine/Private/System/Console/
    KeyInput_Console              ← XInput / PlayStation SDK
    JoystickInput_Xbox            ← 우 스틱 X/Y → delta 변환
```
구현체 네이밍 컨벤션: `{DescriptiveName}Input_{Platform}`
- 모두 `IPointerInput` 상속, 플랫폼에 맞는 서술적 이름 사용
- `InputSystem`은 단일 구현 (플랫폼 변형 없음 — `IKeyInput*`/`IPointerInput*`만 참조)

### 10-3. IPointerInput 설계 원칙

`IMouseInput` → `IPointerInput`: Mouse는 Desktop 전용 단어.

```
플랫폼         Primary        Secondary      DeltaX/Y
────────────────────────────────────────────────────
Desktop        마우스 좌클릭   마우스 우클릭  GetCursorPos delta
Android        첫 손가락 탭    두 손가락 탭   첫 손가락 이동량
Console        A버튼           B버튼          우 스틱 X/Y (정규화)
```

`ProcessNativeEvent(uintptr_t msg, wParam, lParam)` 는 인터페이스에 기본 no-op 제공.
Windows에서만 WM_MOUSEWHEEL 처리에 사용.

### 10-4. EKeyCode 설계 원칙

`EKeyCode` 값 = Windows VK_ 코드 (0x00~0xFF).
- Windows 구현: 배열 직접 인덱싱, 변환 테이블 불필요
- 다른 플랫폼: 플랫폼 코드 → EKeyCode 매핑을 구현체 내부에서만 처리
- 사용자 코드는 항상 심볼(`EKeyCode::W`)만 사용, 숫자값 노출 없음

### 10-5. InputContext + IInputSystem 인터페이스

```cpp
// InputContext — 바인딩 데이터 (vtable 없음, 앱에서 정의)
enum class EPointerSource { DeltaX, DeltaY, ScrollDelta };

class InputContext {
public:
    explicit InputContext(const char* name);
    // 키 → 1D 축 (W:+1, S:-1 처럼 scale로 방향 반전)
    void MapKey    (const char* action, EKeyCode key, float scale = 1.0f);
    // 키 → bool 액션 (Space → Jump)
    void MapKeyBool(const char* action, EKeyCode key);
    // 포인터 delta/scroll → 1D 축
    void MapPointer(const char* action, EPointerSource src, float scale = 1.0f);
};

// IInputSystem — Layer 2 인터페이스
class IInputSystem {
public:
    // keys, pointer를 소유하지 않음 (수명은 호출자 책임)
    static std::unique_ptr<IInputSystem> Create(IKeyInput*, IPointerInput*);
    virtual ~IInputSystem() = default;

    virtual void Update() = 0;

    virtual void PushContext  (const InputContext& ctx) = 0;
    virtual void RemoveContext(const char* name)        = 0;

    // Update() 이후 유효
    virtual bool  IsTriggered(const char* action) const = 0;  // Down or Press
    virtual float GetAxis    (const char* action) const = 0;  // -1.0 ~ +1.0
};
```

### 10-6. ICamera와의 연동 (Step 6)

```cpp
// ICamera 업데이트 시그니처
virtual void Update(float deltaTime, const IInputSystem& input) = 0;

// InputContext 셋업 (앱 초기화 시, 플랫폼 독립)
moveCtx.MapKey("MoveForward", EKeyCode::W,  +1.0f);
moveCtx.MapKey("MoveForward", EKeyCode::S,  -1.0f);
moveCtx.MapKey("MoveRight",   EKeyCode::D,  +1.0f);
moveCtx.MapKey("MoveRight",   EKeyCode::A,  -1.0f);
moveCtx.MapKeyBool("SpeedBoost", EKeyCode::LeftShift);
moveCtx.MapPointer("LookX", EPointerSource::DeltaX, 0.1f);
moveCtx.MapPointer("LookY", EPointerSource::DeltaY, 0.1f);

// FreeCamera::Update 내부 (플랫폼 완전 독립)
float fwd   = input.GetAxis("MoveForward");
float right = input.GetAxis("MoveRight");
float yaw   = input.GetAxis("LookX");
float pitch = input.GetAxis("LookY");
bool  boost = input.IsTriggered("SpeedBoost");
```
