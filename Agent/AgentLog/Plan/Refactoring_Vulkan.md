# Vulkan RHI 리팩터링 가이드

Windows-Vulkan / Android-Vulkan 현재 구현 기준으로,  
추후 수정이 필요한 항목을 계층별로 정리한 문서입니다.

---

## 목차

1. [Application 계층](#1-application-계층)
2. [RHI 인터페이스 계층](#2-rhi-인터페이스-계층)
3. [Vulkan 구현체 계층](#3-vulkan-구현체-계층)
4. [Android 전용](#4-android-전용)
5. [수정 우선순위 요약](#5-수정-우선순위-요약)

---

## 1. Application 계층

### 1-1. `Execute.h` — Vulkan 타입 직접 멤버 보유

**현재 코드:**
```cpp
#ifdef USE_VULKAN
    #include "Pipeline_Vulkan.h"
    #include "Buffer_Vulkan.h"
    #include <vulkan/vulkan.h>

    std::unique_ptr<Pipeline_Vulkan> pipeline;
    std::unique_ptr<Buffer_Vulkan>   vertexBuffer;
    std::unique_ptr<Buffer_Vulkan>   indexBuffer;
    std::unique_ptr<Buffer_Vulkan>   uniformBuffer;
    VkDescriptorSet                  descriptorSet = VK_NULL_HANDLE;
#endif
```

**문제:**  
RHI의 설계 의도는 Application이 어떤 Graphics API를 쓰는지 몰라야 한다는 것이다.  
`IPipelineState`, `IBuffer`가 없기 때문에 Vulkan 구체 타입을 직접 멤버로 보유하는 것이다.

**수정 방향:**
```cpp
// #ifdef USE_VULKAN 완전 제거 후 추상 인터페이스만 보유
std::unique_ptr<IPipelineState> pipeline;
std::unique_ptr<IBuffer>        vertexBuffer;
std::unique_ptr<IBuffer>        indexBuffer;
std::unique_ptr<IBuffer>        uniformBuffer;
// IDescriptorSet을 통한 리소스 바인딩
```

---

### 1-2. `Execute.cpp` — SwapChain 다운캐스팅 + Vulkan API 직접 호출

**현재 코드:**
```cpp
auto* vkSC = static_cast<SwapChain_Vulkan*>(swapChain.get());  // 다운캐스팅
VkCommandBuffer cb = vkSC->BeginFrame();                        // Vulkan 타입 노출

vkCmdSetViewport(cb, 0, 1, &viewport);                         // Vulkan API 직접 호출
vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, ...);
vkCmdDrawIndexed(cb, CubeMesh::INDEX_COUNT, 1, 0, 0, 0);
```

**문제:**  
`BeginFrame()`이 `ISwapChain`에 없고, `VkCommandBuffer`를 Execute가 직접 들고 있다.  
`ICommandList`에 드로우 메서드가 없어서 Vulkan API를 직접 호출할 수밖에 없는 구조다.

**수정 방향:**
```cpp
// ISwapChain에 BeginFrame이 추가되고
// ICommandList에 드로우 메서드가 추가되면
ICommandList* cmd = swapChain->BeginFrame();
cmd->SetViewport(0, 0, width, height);
cmd->BindPipeline(pipeline.get());
cmd->DrawIndexed(CubeMesh::INDEX_COUNT);
swapChain->EndFrame();
```

---

### 1-3. `Execute.cpp` — 하드코딩된 해상도

**현재 코드:**
```cpp
// OnInitialize
swapChainDesc.width  = 1280;
swapChainDesc.height = 720;

// OnUpdate
MatUtil::Perspective(0.7854f, 1280.0f / 720.0f, 0.1f, 100.0f, ubo.projection);
```

**문제:**  
Android에서는 기기마다 화면 크기가 다르다.  
또한 화면 회전 시 가로세로가 바뀌어도 종횡비가 갱신되지 않는다.

**수정 방향:**
- `OnInitialize`에서 `ANativeWindow_getWidth()` / `ANativeWindow_getHeight()` 로 실제 크기 가져오기  
- `ISwapChain::GetExtent()` 또는 `GetWidth()/GetHeight()` 인터페이스 추가  
- `OnUpdate`에서 매 프레임 SwapChain에서 현재 해상도를 조회하여 종횡비 계산

---

### 1-4. `Execute::OnSuspend` / `OnResume` — 빈 구현

**현재 코드:**
```cpp
void Execute::OnSuspend() {}
void Execute::OnResume()  {}
```

**문제:**  
Android에서 앱이 백그라운드로 가면 Surface가 무효화될 수 있다.  
`OnSuspend`에서 Vulkan 렌더링을 중단하고, `OnResume`에서 SwapChain을 재생성해야 한다.

**수정 방향:**
```cpp
void Execute::OnSuspend()
{
    // vkDeviceWaitIdle 후 렌더링 플래그 비활성화
    // (Android) Surface 소멸에 대비
}

void Execute::OnResume()
{
    // SwapChain 유효성 재확인
    // 필요 시 Resize() 호출
}
```

---

## 2. RHI 인터페이스 계층

### 2-1. `ICommandList` — 내용 완전 부재

**현재 코드:**
```cpp
class ICommandList
{
};  // 메서드 하나도 없음
```

**수정 방향:**

```cpp
class ICommandList
{
public:
    virtual ~ICommandList() = default;

    virtual void Open()  = 0;  // 기록 시작
    virtual void Close() = 0;  // 기록 종료

    virtual void SetViewport(float x, float y, float w, float h, float minZ, float maxZ) = 0;
    virtual void SetScissorRect(int32_t left, int32_t top, int32_t right, int32_t bottom) = 0;

    virtual void SetPipelineState(IPipelineState* pso) = 0;
    virtual void SetVertexBuffer(uint32_t slot, IBuffer* buffer) = 0;
    virtual void SetIndexBuffer(IBuffer* buffer) = 0;

    virtual void DrawInstanced(uint32_t vertexCount, uint32_t instanceCount,
                               uint32_t startVertex, uint32_t startInstance) = 0;
    virtual void DrawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount,
                                      uint32_t startIndex, int32_t baseVertex,
                                      uint32_t startInstance) = 0;
};
```

---

### 2-2. `ISwapChain` — 프레임 진입·RT 접근 방법 없음

**현재 ISwapChain 문제:**

| 누락 항목 | 설명 |
|----------|------|
| `BeginFrame()` | 현재 `SwapChain_Vulkan`에만 있음 (`ISwapChain`에 없음) |
| `EndFrame()` | 동일 |
| `GetCurrentRenderTarget()` | ICommandList::SetRenderTargets()에 넘길 추상 RT 반환 없음 |
| `GetWidth()` / `GetHeight()` | 현재 해상도 조회 불가 |
| `Resize()` 구현 | DX12·Vulkan 둘 다 TODO 상태 |

**수정 방향:**
```cpp
class ISwapChain
{
public:
    virtual ICommandList* BeginFrame() = 0;   // 반환: 이번 프레임용 CommandList
    virtual void          EndFrame()   = 0;
    virtual void          Present(bool bVsync) = 0;
    virtual void          Resize(uint32_t width, uint32_t height) = 0;

    virtual uint32_t GetCurrentBackBufferIndex() const = 0;
    virtual uint32_t GetWidth()  const = 0;
    virtual uint32_t GetHeight() const = 0;
};
```

---

### 2-3. `IFence` — Windows HANDLE 노출

**현재 코드:**
```cpp
virtual void SetEventOnComplete(uint64_t value, void* inEventHandle) = 0;
```

**문제:**  
`void* inEventHandle`은 사실상 Windows `HANDLE`이다.  
Android·Linux에서는 이 개념이 없으므로 크로스 플랫폼 추상화가 깨진다.

**수정 방향:**
```cpp
// SetEventOnComplete 제거 후 블로킹 대기로 캡슐화
virtual void WaitForValue(uint64_t value) = 0;
```

내부 구현에서 Win32 `HANDLE` 이벤트를 생성·대기하고, Android는 스핀 또는 futex로 구현한다.

---

### 2-4. `IRenderDevice` — 팩토리 메서드 누락

현재 `CreateSwapChain`, `CreateCommandQueue`, `CreateFence`, `CreateCommandList`만 있다.

추가 필요:

```cpp
virtual std::unique_ptr<IBuffer>        CreateBuffer(const BufferDesc& desc) = 0;
virtual std::unique_ptr<IPipelineState> CreatePipelineState(const PipelineStateDesc& desc) = 0;
virtual std::unique_ptr<ITexture>       CreateTexture(const TextureDesc& desc) = 0;
```

---

### 2-5. `EPixelFormat` — DXGI_FORMAT 수치 그대로 사용

**현재 코드:**
```cpp
enum EPixelFormat : uint32_t
{
    R8G8B8A8_UNORM   = 28,  // DXGI_FORMAT_R8G8B8A8_UNORM = 28 과 동일
    D24_UNORM_S8_UINT = 45,
    B8G8R8A8_UNORM   = 87,
};
```

`SwapChain_DirectX`에서 `static_cast<DXGI_FORMAT>(desc.pixelFormat)`으로 직접 캐스팅한다.  
열거값이 DX12의 수치와 묶여 있어 Vulkan 포맷(`VkFormat`)과 다른 경우 값이 틀린다.

**수정 방향:**
- `EPixelFormat`은 0부터 순번으로 정의 (API 수치와 무관)
- 각 RHI 구현체 내부에 변환 함수 추가:
  ```cpp
  // DX12 구현체 내부
  DXGI_FORMAT ToDXGIFormat(EPixelFormat fmt);
  // Vulkan 구현체 내부
  VkFormat ToVkFormat(EPixelFormat fmt);
  ```

---

### 2-6. 누락된 인터페이스 3종

아래 인터페이스가 없기 때문에 `Execute`에 Vulkan 타입이 노출된다.

#### `IBuffer`

```cpp
class IBuffer
{
public:
    virtual ~IBuffer() = default;
    virtual void  Upload(const void* data, size_t size) = 0;
    virtual void* GetNativeHandle() const = 0;
};

struct BufferDesc
{
    size_t        size;
    EBufferUsage  usage;   // Vertex / Index / Constant / Staging
    EMemoryAccess access;  // DeviceLocal / HostVisible
};
```

#### `IPipelineState`

```cpp
class IPipelineState
{
public:
    virtual ~IPipelineState() = default;
};

struct PipelineStateDesc
{
    std::string vertexShaderPath;
    std::string pixelShaderPath;
    // 버텍스 레이아웃, 블렌드, 래스터라이저, 뎁스 상태
};
```

> 셰이더 포맷이 DX12(DXIL)와 Vulkan(SPIR-V)에서 다르다.  
> 경로를 넘기면 내부에서 플랫폼에 맞는 포맷을 선택하는 방식으로 설계.

#### `IDescriptorSet`

```cpp
class IDescriptorSet
{
public:
    virtual ~IDescriptorSet() = default;
    virtual void BindBuffer(uint32_t binding, IBuffer* buffer) = 0;
    virtual void BindTexture(uint32_t binding, ITexture* texture) = 0;
};
```

---

## 3. Vulkan 구현체 계층

### 3-1. `Pipeline_Vulkan::Vertex` — CubeMesh::Vertex와 별개 정의

**현재 상황:**
```cpp
// Pipeline_Vulkan.h
struct Vertex { float position[3]; float color[3]; };

// CubeMesh.h
// 동일한 레이아웃이지만 별도로 정의되어 있음
```

`Pipeline_Vulkan`의 정점 레이아웃과 `CubeMesh`의 데이터 레이아웃이 실제로 일치하는지  
컴파일러가 검증할 방법이 없다. 레이아웃이 달라져도 조용히 깨진다.

**수정 방향:**
- 공통 `Vertex` 구조체를 RHI나 Engine 레벨에서 단일 정의
- `CubeMesh`는 그 공통 `Vertex`를 사용
- `Pipeline_Vulkan`도 같은 타입을 참조하여 `offsetof()`로 레이아웃 계산

---

### 3-2. `Pipeline_Vulkan::UniformBufferObject` — 파이프라인 내부에 정의

**현재 코드:**
```cpp
// Pipeline_Vulkan.h
struct UniformBufferObject
{
    float model[16];
    float view[16];
    float projection[16];
};
```

`Execute.cpp`에서 `Pipeline_Vulkan::UniformBufferObject`를 직접 사용하기 위해  
`Pipeline_Vulkan.h`를 include해야 한다. 이것 자체가 Execute가 Vulkan 헤더를 알아야 하는 이유 중 하나다.

**수정 방향:**
- `UniformBufferObject`를 공통 헤더(`Common_RHI.h` 또는 Math 레이어)로 분리
- 또는 `IPipelineState`가 UBO 레이아웃을 추상화하여 Execute가 `float model[16]` 등을 직접 채우지 않도록 설계

---

### 3-3. `SwapChain_Vulkan` — CommandBuffer를 SwapChain이 직접 관리

**현재 구조:**

```
SwapChain_Vulkan
  ├── VkCommandBuffer commandBuffers[3]   ← SwapChain이 할당·해제
  ├── BeginFrame() → VkCommandBuffer 반환
  └── EndFrame()   → vkEndCommandBuffer 호출
```

**문제:**  
`CommandBuffer`의 수명 관리가 `SwapChain`에 있는 것은 책임 분리 측면에서 어색하다.  
`CommandBuffer`는 `CommandQueue` 또는 별도 `CommandList` 레이어에서 관리하는 것이 자연스럽다.

**이상적인 구조:**
```
CommandQueue_Vulkan
  └── CommandPool 관리

SwapChain_Vulkan
  └── 이미지 순환 + Present만 담당

ICommandList 구현체
  └── CommandBuffer 래핑
```

> 단, 이 변경은 `ICommandList`가 완성된 뒤에 진행하는 것이 순서상 맞다.

---

### 3-4. `SwapChain_Vulkan` — 동기화 객체가 프레임당 1세트

**현재:**
```cpp
VkSemaphore imageAvailableSemaphore;   // 1개
VkSemaphore renderFinishedSemaphore;   // 1개
VkFence     inFlightFence;             // 1개
```

**문제:**  
`MAX_FRAMES_IN_FLIGHT`를 2 이상으로 하여 CPU-GPU 오버랩을 활용하려면  
프레임 수만큼의 세마포어·Fence가 필요하다.  
현재는 프레임당 대기(CPU stall)가 발생하여 GPU 활용률이 낮다.

**수정 방향:**
```cpp
static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

VkSemaphore imageAvailableSemaphore[MAX_FRAMES_IN_FLIGHT];
VkSemaphore renderFinishedSemaphore[MAX_FRAMES_IN_FLIGHT];
VkFence     inFlightFence[MAX_FRAMES_IN_FLIGHT];
uint32_t    currentFrameIndex = 0;  // MAX_FRAMES_IN_FLIGHT로 순환
```

---

### 3-5. `SwapChain_Vulkan::Resize()` — 미구현

**현재 코드:**
```cpp
void SwapChain_Vulkan::Resize(uint32_t width, uint32_t height)
{
    // TODO: ...
}
```

`vkAcquireNextImageKHR`이 `VK_ERROR_OUT_OF_DATE_KHR`을 반환하면 현재는 예외를 던진다.  
화면 크기 변경 시 앱이 크래시된다.

**수정 순서:**
1. `vkDeviceWaitIdle()`
2. `CleanupSwapchain()`
3. `QuerySwapchainSupport()` 재호출
4. `CreateSwapchain()` ~ `CreateSyncObjects()` 재생성
5. `BeginFrame()`에서 `VK_ERROR_OUT_OF_DATE_KHR` 수신 시 예외 대신 Resize 플래그 세우기

---

### 3-6. `Buffer_Vulkan::CreateDeviceLocal()` — VkCommandPool·VkQueue 외부 요구

**현재 코드:**
```cpp
void CreateDeviceLocal(VkPhysicalDevice physicalDevice, VkDevice device,
                       VkCommandPool commandPool, VkQueue queue,   // ← Vulkan 핸들 직접 요구
                       const void* data, VkDeviceSize size, VkBufferUsageFlags usage);
```

`IBuffer` 인터페이스로 추상화하면 `VkCommandPool`, `VkQueue` 같은 Vulkan 핸들이  
팩토리 호출부(`IRenderDevice::CreateBuffer`)로 새어 나오지 않아야 한다.

**수정 방향:**
- `IRenderDevice::CreateBuffer(BufferDesc, initialData)`로 통합  
- Staging 복사에 필요한 CommandPool/Queue는 `RenderDevice_Vulkan` 내부에서 조달

---

### 3-7. `NativeDisplayInfo.Display` — AAssetManager* 오용

**현재 방식:**
```cpp
// Android main.cpp
info.Display = app->activity->assetManager;  // AAssetManager* 끼워넣기

// Execute.cpp
void* assetMgr = displayInfo.Display;        // 꺼내서 Pipeline에 전달
```

**문제:**  
`Display`는 원래 "디스플레이 연결 핸들"의 의미다 (X11의 `Display*`, EGL의 `EGLDisplay`).  
`AAssetManager`는 파일 I/O 핸들이므로 개념이 다르다.  
`NativeDisplayInfo`를 경유하여 Pipeline까지 전달되는 경로가 부자연스럽다.

**수정 방향:**
- `AAssetManager`를 위한 별도 파일 I/O 추상화 계층 도입
- 예: `IFileSystem` 인터페이스 → `FileSystem_Android`(AAssetManager), `FileSystem_PC`(std::ifstream)
- `IRenderDevice::SetFileSystem(IFileSystem*)` 또는 생성 시 주입하는 방식으로 분리

---

## 4. Android 전용

### 4-1. 화면 크기 하드코딩

**현재 코드:**
```cpp
swapChainDesc.width  = 1280;
swapChainDesc.height = 720;
```

Android에서는 `ANativeWindow`에서 실제 크기를 가져와야 한다.

**수정 방향:**
```cpp
// main.cpp — APP_CMD_INIT_WINDOW에서
info.Handle = app->window;
// NativeDisplayInfo에 초기 크기 포함, 또는 ISwapChain이 내부에서 조회
```

---

### 4-2. 화면 회전 미처리

`main.cpp`의 `HandleCommand`에서 `APP_CMD_WINDOW_RESIZED` 이벤트를 처리하지 않는다.  
화면을 가로/세로로 돌리면 스왑체인이 맞지 않아 렌더링이 깨지거나 크래시가 발생한다.

**수정 방향:**
```cpp
case APP_CMD_WINDOW_RESIZED:
    if (execEngine)
    {
        int w = ANativeWindow_getWidth(app->window);
        int h = ANativeWindow_getHeight(app->window);
        // ISwapChain::Resize() 호출 (Resize가 구현된 뒤)
    }
    break;
```

---

### 4-3. `OnSuspend` / `OnResume` 빈 구현

Android에서 Surface는 앱이 백그라운드에 가면 파괴될 수 있다.  
`APP_CMD_TERM_WINDOW`에서 Execute를 전부 소멸시키는 방식은 과도하다.  
화면(Surface)만 재생성하고 GPU 리소스는 유지하는 것이 더 효율적이다.

**수정 방향:**
```
APP_CMD_TERM_WINDOW → SwapChain만 해제 (GPU 리소스 유지)
APP_CMD_INIT_WINDOW → SwapChain 재생성
```

현재는 `TERM_WINDOW`에서 `OnDestroy()` 전체를 호출하여 매번 GPU 리소스를 전부 파괴·재생성한다.

---

### 4-4. `MatrixUtil` — DX12 병용 불가 (Column-Major + Vulkan Y-flip 고정)

**현재 코드:**
```cpp
// Perspective 내부
m[5] = -f;  // Vulkan Y-flip 고정
```

현재 `MatrixUtil`은 **Vulkan 전용** 행렬이다.  
DX12(HLSL)는 Row-Major 컨벤션이고 Y-flip이 없어서 이 함수를 그대로 쓸 수 없다.

**수정 방향:**
- `MatUtil::Perspective_Vulkan()` / `MatUtil::Perspective_DX12()` 분리, 또는
- `IRenderDevice::GetClipSpaceDesc()` 같은 API로 NDC 컨벤션을 쿼리하여 행렬 생성 시 분기

---

## 5. 수정 우선순위 요약

### Phase 1 — 기능 안정화 (버그/크래시 방지)

| 항목 | 파일 | 이유 |
|------|------|------|
| `SwapChain_Vulkan::Resize()` 구현 | `SwapChain_Vulkan.cpp` | 창 크기 변경·화면 회전 시 크래시 |
| Android 화면 크기 동적 조회 | `Execute.cpp`, `main.cpp` | 1280×720 하드코딩 |
| Android `APP_CMD_WINDOW_RESIZED` 처리 | `main.cpp` | 화면 회전 미처리 |
| `SwapChain_DirectX::Resize()` 구현 | `SwapChain_DirectX.cpp` | 창 크기 변경 시 크래시 |

### Phase 2 — RHI 추상화 완성

| 항목 | 파일 | 이유 |
|------|------|------|
| `IBuffer` 인터페이스 + DX12·Vulkan 구현체 | 신규 | Execute에서 Vulkan 타입 제거 |
| `IPipelineState` 인터페이스 + 구현체 | 신규 | Execute에서 Vulkan 타입 제거 |
| `ICommandList` 드로우 메서드 추가 + 구현체 | `ICommandList.h` | Execute Vulkan API 직접 호출 제거 |
| `ISwapChain::BeginFrame()` / `EndFrame()` | `ISwapChain.h` | 다운캐스팅 제거 |
| `ISwapChain::GetWidth()` / `GetHeight()` | `ISwapChain.h` | 해상도 조회 추상화 |
| `EPixelFormat` 독립 열거값 + 변환 함수 | `Common_RHI.h` | DXGI 수치 의존 제거 |
| `IFence::WaitForValue()` | `IFence.h` | Windows HANDLE 노출 제거 |

### Phase 3 — 설계 개선

| 항목 | 파일 | 이유 |
|------|------|------|
| `IFileSystem` 도입 → `AAssetManager` 분리 | 신규 | NativeDisplayInfo.Display 오용 해소 |
| `CommandBuffer` 관리를 SwapChain에서 분리 | `SwapChain_Vulkan` | 책임 분리 |
| 동기화 객체 `MAX_FRAMES_IN_FLIGHT`로 확장 | `SwapChain_Vulkan` | GPU 활용률 향상 |
| `Vertex` 공통 구조체 단일 정의 | `Common_RHI.h` | Pipeline·CubeMesh 레이아웃 불일치 방지 |
| `MatrixUtil` API별 분기 또는 분리 | `MatrixUtil.h` | DX12 병용 가능하도록 |
| `Android OnSuspend/OnResume` Surface만 재생성 | `Execute.cpp`, `main.cpp` | 불필요한 GPU 리소스 전체 파괴 방지 |
