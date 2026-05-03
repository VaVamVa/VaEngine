# Vulkan RHI 구현 계획 (Windows — Black Window 파리티)

## Context

DX12 백엔드는 이미 Instance/Device/Queue/SwapChain/Fence 초기화 및 Black Window 렌더링까지 완료된 상태.  
`Windows-Vulkan` 브랜치에서 Vulkan 백엔드를 DX12와 동일한 수준(Black Window 출력)으로 구현한다.  
현재 Vulkan 파일(`RenderDevice_Vulkan.h/.cpp`)은 빈 스텁만 존재.  
사용자가 Vulkan 입문자이므로 모든 코드에 개념 설명 주석을 포함.

---

## 구현 범위

DX12 파리티 목표: `swapChain->Present()` 호출 시 검은 창이 출력되는 수준.  
Vulkan은 DX12와 달리 present에도 RenderPass + CommandBuffer가 필수이므로, SwapChain 내부에 clear pass용 CommandBuffer를 포함.

---

## 생성/수정 파일 목록

| 파일 | 작업 |
|------|------|
| `Engine/Private/RHI/Vulkan/Common_Vulkan.h` | 수정 (Vulkan 헤더, VK_CHECK 매크로) |
| `Engine/Private/RHI/Vulkan/RenderDevice_Vulkan.h` | 수정 (스텁 → 전체 구현) |
| `Engine/Private/RHI/Vulkan/RenderDevice_Vulkan.cpp` | 수정 (스텁 → 전체 구현) |
| `Engine/Private/RHI/Vulkan/CommandQueue_Vulkan.h` | 신규 생성 |
| `Engine/Private/RHI/Vulkan/CommandQueue_Vulkan.cpp` | 신규 생성 |
| `Engine/Private/RHI/Vulkan/SwapChain_Vulkan.h` | 신규 생성 |
| `Engine/Private/RHI/Vulkan/SwapChain_Vulkan.cpp` | 신규 생성 |
| `Engine/Private/RHI/Vulkan/Fence_Vulkan.h` | 신규 생성 |
| `Engine/Private/RHI/Vulkan/Fence_Vulkan.cpp` | 신규 생성 |

---

## DX12 ↔ Vulkan 개념 대응표

| DX12 | Vulkan | 역할 |
|------|--------|------|
| `IDXGIFactory6` | `VkInstance` | API 진입점 |
| `ID3D12Debug` | Validation Layer + `VkDebugUtilsMessengerEXT` | 디버그 |
| `IDXGIAdapter4` | `VkPhysicalDevice` | 물리 GPU |
| `ID3D12Device` | `VkDevice` | 논리 장치 |
| `ID3D12CommandQueue` | `VkQueue` | GPU 명령 제출 큐 |
| `IDXGISwapChain3` | `VkSwapchainKHR` | 백버퍼 관리 |
| `ID3D12Resource` (back buffer) | `VkImage` + `VkImageView` | 렌더 타겟 |
| `D3D12_RENDER_TARGET_VIEW` | `VkFramebuffer` + `VkRenderPass` | 렌더 타겟 기술 |
| `ID3D12Fence` | `VkFence` (CPU-GPU) + `VkSemaphore` (GPU-GPU) | 동기화 |

---

## 파일별 상세 설계

### 1. `Common_Vulkan.h`
```
#include <vulkan/vulkan.h>
#ifdef _WIN32
  #include <vulkan/vulkan_win32.h>
#endif
VK_CHECK(expr) 매크로 — FAILED() 대응, std::runtime_error 던짐
```

### 2. `RenderDevice_Vulkan`

**Initialize() 내부 호출 순서:**
1. `CreateInstance()` — VkInstance + 필요 확장:  
   `VK_KHR_surface`, `VK_KHR_win32_surface`, `VK_EXT_debug_utils`(Debug only)
2. `SetupDebugMessenger()` — Validation Layer 콜백 등록 (Debug only)
3. `PickPhysicalDevice()` — Graphics 큐 패밀리 보유 + `VK_KHR_swapchain` 지원 GPU 선택
4. `CreateLogicalDevice()` — `VkDevice` + Graphics VkQueue 생성

**추가 getters (다른 Vulkan 클래스들이 사용):**
```cpp
VkInstance       GetInstance()        const;
VkPhysicalDevice GetPhysicalDevice()  const;
VkDevice         GetDevice()          const;
uint32_t         GetGraphicsQueueFamilyIndex() const;
```

**Shutdown():** vkDestroyDevice → vkDestroyDebugUtilsMessenger → vkDestroyInstance

### 3. `CommandQueue_Vulkan`

**Register(device, desc) 내부:**
1. `vkGetDeviceQueue()` — 논리 장치에서 graphics VkQueue 핸들 획득
2. `vkCreateCommandPool()` — 이 큐에서 사용할 CommandPool 생성  
   (SwapChain이 CommandBuffer를 할당할 때 이 풀을 빌림)

**getter:**
```cpp
VkQueue           GetQueue()       const;
VkCommandPool     GetCommandPool() const;
uint32_t          GetFamilyIndex() const;
```

**Signal / Wait:** DX12의 queue->Signal/Wait은 현재 Execute에서 사용되지 않으므로 빈 구현. (CommandList 구현 시 채움)

### 4. `SwapChain_Vulkan`  ← Vulkan 구현의 핵심

**Register(device, desc) 내부 순서:**
1. `CreateSurface()` — `vkCreateWin32SurfaceKHR(instance, HWND, ...)` → `VkSurfaceKHR`
2. `VerifyPresentSupport()` — `vkGetPhysicalDeviceSurfaceSupportKHR()` 확인
3. `QuerySwapchainSupport()` — 포맷/프레젠트 모드/Extent 조회
4. `ChooseFormat()` — `VK_FORMAT_B8G8R8A8_UNORM` + `VK_COLOR_SPACE_SRGB_NONLINEAR_KHR`
5. `ChoosePresentMode()` — `VK_PRESENT_MODE_FIFO_KHR` (VSync) / `VK_PRESENT_MODE_IMMEDIATE_KHR`
6. `ChooseExtent()` — 창 크기 기반
7. `CreateSwapchain()` — `vkCreateSwapchainKHR()`
8. `RetrieveImages()` — `vkGetSwapchainImagesKHR()`
9. `CreateImageViews()` — 각 `VkImage`에 `VkImageView` 생성
10. `CreateRenderPass()` — 단일 color attachment, loadOp=CLEAR, finalLayout=PRESENT_SRC
11. `CreateFramebuffers()` — 각 ImageView에 대응하는 `VkFramebuffer`
12. `AllocateCommandBuffers()` — CommandQueue의 Pool에서 프레임 수만큼 할당
13. `RecordClearCommands()` — 각 CommandBuffer에 "RenderPass begin(clear black) → end" 미리 기록
14. `CreateSyncObjects()` — `imageAvailableSemaphore`, `renderFinishedSemaphore`, `inFlightFence`

**Present(bVsync) 내부:**
```
1. vkWaitForFences(inFlightFence)          // 이전 프레임 GPU 작업 완료 대기
2. vkResetFences(inFlightFence)
3. vkAcquireNextImageKHR(imageAvailableSemaphore) → imageIndex
4. vkQueueSubmit(
     waitSemaphore  = imageAvailableSemaphore,
     commandBuffer  = commandBuffers[imageIndex],
     signalSemaphore= renderFinishedSemaphore,
     signalFence    = inFlightFence)
5. vkQueuePresentKHR(waitSemaphore = renderFinishedSemaphore)
```

**멤버 변수:**
```cpp
VkSurfaceKHR     surface;
VkSwapchainKHR   swapChain;
VkRenderPass     renderPass;
VkFormat         imageFormat;
VkExtent2D       extent;

static constexpr uint32_t MAX_BUFFER_COUNT = 3;
uint32_t           imageCount;
VkImage            images[MAX_BUFFER_COUNT];
VkImageView        imageViews[MAX_BUFFER_COUNT];
VkFramebuffer      framebuffers[MAX_BUFFER_COUNT];
VkCommandBuffer    commandBuffers[MAX_BUFFER_COUNT];

VkSemaphore  imageAvailableSemaphore;
VkSemaphore  renderFinishedSemaphore;
VkFence      inFlightFence;

// Register 시 빌린 포인터
RenderDevice_Vulkan*  vulkanDevice = nullptr;
CommandQueue_Vulkan*  vulkanQueue  = nullptr;
```

### 5. `Fence_Vulkan`

DX12의 `ID3D12Fence`는 타임라인 값 기반이지만, `Execute`에서 현재 Fence는 사용 안 함  
(OnRender/OnPostRender가 비어있음). 인터페이스 호환용으로 VkFence 래핑.

**Register():** `vkCreateFence(VK_FENCE_CREATE_SIGNALED_BIT)`  
**SetEventOnComplete(value, eventHandle):**  
  `vkWaitForFences()` 후 `SetEvent(static_cast<HANDLE>(eventHandle))`  
**GetCompletedValue():** 내부 simulated counter 반환

---

## 구현 시 주의사항

### Validation Layer 이름
```cpp
static constexpr const char* VALIDATION_LAYER = "VK_LAYER_KHRONOS_validation";
```
Debug 빌드에서만 활성화 (`#ifdef _DEBUG`).

### Queue Family 선택 전략
단순화를 위해 Graphics Family = Present Family 가정.  
모든 데스크탑 GPU에서 성립. Android용 확장 시 separate present queue 지원 필요.

### Surface Extension 분리
`Common_Vulkan.h`에서 플랫폼별 surface extension 문자열을 매크로로 분리:
```cpp
#ifdef _WIN32
  #define VK_PLATFORM_SURFACE_EXTENSION VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#endif
```

### Vulkan 객체 소멸 순서 (Shutdown / SwapChain 소멸자)
```
inFlightFence → renderFinishedSemaphore → imageAvailableSemaphore →
commandBuffers(free) → framebuffers → renderPass →
imageViews → swapChain → surface
```

---

## 검증 방법

1. CMake `windows-vulkan` 프리셋으로 빌드 (`cmake --preset windows-vulkan && cmake --build`)
2. 실행 파일 구동 → 1280×720 검은 창 출력 확인
3. Validation Layer 경고/에러 없는지 디버거 출력 확인
4. 창 닫기 시 크래시 없이 정상 종료 확인

---

## 참고: 변경 불필요 파일

- `Public/Interfaces/IRenderDevice.h` — 변경 없음
- `Public/Interfaces/ICommandQueue.h` — 변경 없음
- `Public/Interfaces/ISwapChain.h` — 변경 없음
- `Public/Interfaces/IFence.h` — 변경 없음
- `Private/RHI/RHILoader.cpp` — 이미 `USE_VULKAN` 분기 존재
- `Engine/CMakeLists.txt` — 이미 `find_package(Vulkan REQUIRED)` 설정 완료
- `Platforms/ExecWindows/Execute.cpp` — 변경 없음 (Vulkan용 별도 Execute 불필요)