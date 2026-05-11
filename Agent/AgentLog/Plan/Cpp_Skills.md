# VaEngine: Native C++ Modern C++ 및 Low-Level 시스템 역량 분석

본 문서는 VaEngine 프로젝트(`E:\Project\VaEngine\VaEngine`)에서 발견된 **Modern C++(C++17/20)** 기법과 **Low-Level 시스템 엔지니어링** 패턴을 소스 코드 기반으로 정리하고, 추가로 적용 가능한 개선 방향을 제안합니다.

---

## 1. 스마트 포인터 기반 소유권 체계

### 1.1 `std::unique_ptr` 팩토리 소유권 체인
- **관련 파일**: `Engine/Public/RHI/IRenderDevice.h`, `Engine/Public/Interfaces/IExecute.h`, `Application/Source/Public/Execute.h`
- **코드 사례**:
  ```cpp
  // IRenderDevice.h - 순수 가상 팩토리
  virtual std::unique_ptr<ICommandQueue> CreateCommandQueue(ECommandQueueType type) = 0;
  virtual std::unique_ptr<IFence>        CreateFence() = 0;
  virtual std::unique_ptr<ISwapChain>    CreateSwapChain(...) = 0;

  // Execute.h - 최상위 소유자
  std::unique_ptr<IRenderDevice>  renderDevice;
  std::unique_ptr<ICommandQueue>  commandQueue;
  std::unique_ptr<IFence>         fence;
  std::unique_ptr<ISwapChain>     swapChain;
  ```
- **선정 이유**: 인터페이스 반환 타입 자체가 소유권을 표현하므로, `delete` 누락 및 이중 해제 버그가 구조적으로 불가능해집니다.
- **기술적 분석**: `unique_ptr`를 반환하는 팩토리 패턴은 호출자에게 소유권을 명시적으로 이전(move semantics)하며, 인터페이스 소멸자의 `virtual ~T() = default`와 결합하여 파생 클래스도 올바르게 해제됩니다.

### 1.2 `ComPtr<T>` (WRL) COM 오브젝트 스마트 포인터
- **관련 파일**: `Engine/Private/RHI/DirectX/RenderDevice_DirectX.h`, `SwapChain_DirectX.h`
- **코드 사례**:
  ```cpp
  #include <wrl/client.h>
  using Microsoft::WRL::ComPtr;

  ComPtr<IDXGIFactory6>  dxgiFactory;
  ComPtr<IDXGIAdapter4>  adapter;
  ComPtr<ID3D12Device>   device;
  ```
- **선정 이유**: DirectX COM 객체는 `AddRef`/`Release` 수동 호출 없이 참조 카운트 관리가 자동화됩니다.
- **기술적 분석**: `ComPtr::As<T>()` 메서드를 통해 QueryInterface 없이 타입 안전한 인터페이스 업캐스팅이 가능합니다(`tempSwapChain.As(&swapChain)`). `raw_ptr()`로 내부 포인터를 접근하거나 `&`로 출력 파라미터로 넘길 수 있어 기존 DX API와 완벽히 호환됩니다.

---

## 2. 컴파일 타임 설계 및 타입 시스템

### 2.1 `#if` 전처리기 기반 RHI 백엔드 선택
- **관련 파일**: `Engine/Private/RHI/RHILoader.cpp`, `CMakeLists.txt`
- **코드 사례**:
  ```cpp
  // CMakeLists.txt
  option(USE_DX12 "Use DirectX 12" ON)
  option(USE_VULKAN "Use Vulkan" OFF)

  // RHILoader.cpp
  std::unique_ptr<IRenderDevice> RHI::CreateRenderDevice() {
  #if USE_DX12
      return std::make_unique<RenderDevice_DirectX>();
  #elif USE_VULKAN
      return std::make_unique<RenderDevice_Vulkan>();
  #else
      return nullptr;
  #endif
  }
  ```
- **선정 이유**: 런타임 분기 없이 컴파일 시점에 백엔드를 고정하여 불필요한 코드가 바이너리에 포함되지 않습니다.
- **기술적 분석**: CMake `option()`으로 정의된 플래그가 컴파일러 정의(`-DUSE_DX12`)로 전달되어 전처리기 조건부 컴파일을 구동합니다. 순수 가상 인터페이스(`IRenderDevice`)와 결합하여, 나머지 코드는 백엔드를 전혀 모르는 상태로 동작합니다.

### 2.2 `static constexpr` 컴파일 타임 상수
- **관련 파일**: `Engine/Private/RHI/DirectX/SwapChain_DirectX.h`
- **코드 사례**:
  ```cpp
  static constexpr uint32_t MAX_BUFFER_COUNT = 3;
  ComPtr<ID3D12Resource> backBuffers[MAX_BUFFER_COUNT];
  ```
- **선정 이유**: 매직 넘버를 제거하고, 배열 크기가 타입 시스템에 의해 고정되어 범위 초과 버그를 컴파일 타임에 방지합니다.
- **기술적 분석**: `static constexpr`는 클래스 범위에서 ODR(One Definition Rule)을 준수하며, C 스타일 `#define`과 달리 타입 정보와 스코프를 가집니다.

### 2.3 `USE_DEBUGGING` 플래그 기반 다중 백엔드 인터페이스 검증
- **관련 파일**: `Engine/Private/RHI/RHILoader.cpp`, `CMakePresets.json`
- **코드 사례**:
  ```cpp
  // CMakePresets.json — Engine 프리셋이 USE_DEBUGGING만 켬. 백엔드 플래그와 독립적
  { "name": "engine", "cacheVariables": { "USE_DEBUGGING": "ON" } }

  // RHILoader.cpp — USE_DEBUGGING일 때만 모든 백엔드 헤더를 포함
  #if USE_DEBUGGING
      #include "DirectX/RenderDevice_DirectX.h"
      #include "Vulkan/RenderDevice_Vulkan.h"
  #elif USE_DIRECTX
      #include "DirectX/RenderDevice_DirectX.h"
  #elif USE_VULKAN
      #include "Vulkan/RenderDevice_Vulkan.h"
  #endif

  std::unique_ptr<IRenderDevice> RHI::CreateRenderDevice()
  {
  #if USE_DEBUGGING
  #pragma message("RHI Debugging. Can't Execute Engine")
      { [[maybe_unused]] auto device = std::make_unique<RenderDevice_DirectX>(); }
      { [[maybe_unused]] auto device = std::make_unique<RenderDevice_Vulkan>(); }

  #elif USE_DIRECTX
      return std::make_unique<RenderDevice_DirectX>();
  #elif USE_VULKAN
      return std::make_unique<RenderDevice_Vulkan>();
  #endif
      return nullptr;
  }
  ```
- **선정 이유**: 단일 백엔드 사용자는 원하는 SDK만 설치하면 됩니다. 엔진 개발자는 Engine 프리셋 하나로 모든 백엔드 헤더를 포함시켜 인터페이스 변경 시 DX12/Vulkan 양쪽 구현 누락을 단일 빌드에서 즉시 검출할 수 있습니다.
- **기술적 분석**: `[[maybe_unused]]` 지역 변수로 각 백엔드를 실제 인스턴스화하면 컴파일러가 해당 타입이 추상 클래스인지(순수 가상 함수 미구현) 검사합니다. `USE_DEBUGGING`을 백엔드 플래그(`USE_DIRECTX`, `USE_VULKAN`)와 분리함으로써, 백엔드를 추가할 때 `#if USE_DEBUGGING` 블록에 헤더와 인스턴스화 한 줄씩만 추가하면 됩니다. 조합 폭발(`USE_DIRECTX && USE_VULKAN && USE_METAL …`) 없이 선형으로 확장됩니다.

### 2.4 Plain `enum : uint32_t` 비트마스크 (GPU 상태 플래그)
- **관련 파일**: `Engine/Public/RHI/Common_RHI.h`
- **코드 사례**:
  ```cpp
  enum EResourceState : uint32_t {
      Common       = 1 << 0,
      VertexBuffer = 1 << 1,
      RenderTarget = 1 << 3,
      Present      = 1 << 4,
      ...
  };
  ```
- **선정 이유**: `enum class`와 달리 암묵적 `uint32_t` 변환이 허용되어, DirectX API가 기대하는 `D3D12_RESOURCE_STATES` 타입과 직접 `static_cast` 없이 결합 가능합니다.
- **기술적 분석**: GPU API 래퍼 레이어에서는 `enum class`의 타입 안전성보다 하위 API 타입과의 직접 매핑이 더 중요합니다. `EPixelFormat` 값을 `DXGI_FORMAT` 정수와 일치시키는 것도 같은 이유입니다.

### 2.5 `[[nodiscard]]` 팩토리 함수 반환값 강제
- **관련 파일**: `Engine/Public/RHI/IRenderDevice.h` — 24~28번째 줄 (팩토리 함수 5개)
- **코드 사례**:
  ```cpp
  [[nodiscard]] virtual std::unique_ptr<ICommandQueue>  CreateCommandQueue(const CommandQueueDesc& desc) = 0;
  [[nodiscard]] virtual std::unique_ptr<IFence>         CreateFence() = 0;
  [[nodiscard]] virtual std::unique_ptr<ISwapChain>     CreateSwapChain(const SwapChainDesc& desc) = 0;
  [[nodiscard]] virtual std::unique_ptr<ICommandList>   CreateCommandList(const CommandListDesc& desc) = 0;
  [[nodiscard]] virtual std::unique_ptr<ICommandAlloc>  CreateCommandAllocator(const CommandAllocatorDesc& desc) = 0;
  ```
- **선정 이유**: 팩토리가 반환한 `unique_ptr`를 변수에 받지 않으면 그 즉시 소멸하여 리소스가 사라집니다. 이 실수를 런타임이 아닌 컴파일 경고로 즉시 검출합니다.
- **기술적 분석**: C++17 표준 속성(`[[nodiscard]]`)으로 순수 가상 함수에도 적용 가능합니다. 파생 클래스에서 `override` 시 속성이 상속되므로 구체 구현체에 별도로 붙이지 않아도 됩니다.

### 2.6 지정 초기화(Designated Initializers, C++20)
- **관련 파일**: `Engine/Public/RHI/Common_RHI.h` — `SwapChainDesc`, `CommandQueueDesc` 등 구조체 초기화 호출부
- **코드 사례**:
  ```cpp
  SwapChainDesc desc {
      .displayInfo     = nativeInfo,
      .RegisteredQueue = queue,
      .bufferCount     = 3,
      .width           = 1280,
      .height          = 720,
      .pixelFormat     = EPixelFormat::B8G8R8A8_UNORM,
      .bIsWindowed     = true,
  };
  ```
- **선정 이유**: 구조체 필드가 추가되거나 순서가 바뀌어도 초기화 누락을 컴파일 오류로 즉시 감지합니다. 구조체 정의를 보지 않아도 어떤 필드에 무슨 값을 넣는지 코드 자체에서 명확히 드러납니다.
- **기술적 분석**: C++20 표준 기능으로 집합 초기화(aggregate initialization)의 확장입니다. 필드명을 `.field = value` 형식으로 명시하며, 지정하지 않은 필드는 zero-initialize됩니다. C의 designated initializer와 달리 선언 순서를 따라야 합니다.

---

## 3. 객체지향 아키텍처 패턴

### 3.1 순수 가상 인터페이스 + `virtual ~T() = default`
- **관련 파일**: `Engine/Public/RHI/IRenderDevice.h`, `Engine/Public/Interfaces/IExecute.h`
- **코드 사례**:
  ```cpp
  class IRenderDevice {
  public:
      virtual ~IRenderDevice() = default;
      virtual std::unique_ptr<ICommandQueue> CreateCommandQueue(ECommandQueueType type) = 0;
      virtual std::unique_ptr<IFence>        CreateFence() = 0;
      virtual bool Initialize(const NativeDisplayInfo& displayInfo) = 0;
  };
  ```
- **선정 이유**: 인터페이스와 구현을 완전히 분리하여 DirectX/Vulkan 등 백엔드 교체 시 상위 레이어 코드 변경이 없습니다.
- **기술적 분석**: `= default` 가상 소멸자는 `unique_ptr<IRenderDevice>`가 파생 클래스를 올바르게 소멸시킴을 보장합니다. 순수 가상 함수만 선언된 인터페이스는 vtable 슬롯만 차지하며 데이터 멤버가 없어 오버헤드가 최소화됩니다.

### 3.2 `static_cast` 기반 Zero-cost 다운캐스팅
- **관련 파일**: `Engine/Private/RHI/DirectX/CommandQueue_DirectX.cpp`
- **코드 사례**:
  ```cpp
  // 인터페이스 포인터를 구체 타입으로 다운캐스팅 (RTTI 없이)
  auto* dx12Device = static_cast<RenderDevice_DirectX*>(inDevice);
  ```
- **선정 이유**: `dynamic_cast`의 런타임 RTTI 비용을 제거하면서, 아키텍처상 타입이 보장되는 지점에서만 사용하여 안전성을 유지합니다.
- **기술적 분석**: RHI 레이어처럼 "같은 백엔드끼리만 조합된다"는 불변식이 성립하는 구조에서는 `static_cast`가 `dynamic_cast`보다 올바른 선택입니다. 캐스팅 실패가 가능한 경로에는 사용하지 않는 것이 핵심입니다.

### 3.3 `std::span` 기반 커맨드 리스트 배치 제출
- **관련 파일**: `Engine/Public/RHI/ICommandQueue.h` — 24번째 줄
- **코드 사례**:
  ```cpp
  virtual void ExecuteCommandLists(std::span<ICommandList* const> commandLists) = 0;
  ```
- **선정 이유**: `const std::vector<ICommandList*>&`는 `vector`만 받을 수 있지만, `std::span`은 `vector`, C-스타일 배열, 초기화 리스트 모두 암묵적으로 호환되어 호출부 코드가 단순해집니다.
- **기술적 분석**: C++20의 non-owning view 타입으로, 포인터와 길이만 보유하며 복사 없이 연속 메모리를 전달합니다. `ICommandList* const`로 원소를 const 지정하여 큐 내부에서 커맨드 리스트 포인터 자체를 교체하지 못하도록 인터페이스 수준에서 보장합니다.

---

## 4. Low-Level 시스템 엔지니어링

### 4.1 `union` 기반 플랫폼 무관 핸들 구조체
- **관련 파일**: `Engine/Public/RHI/Common_RHI.h`
- **코드 사례**:
  ```cpp
  struct NativeDisplayInfo {
      enum class EPlatform { None, Windows, Android, iOS, Max };
      union {
          void*     Handle;    // Windows: HWND
          uintptr_t RawData;   // 정수 연산용 별칭
      };
      void* Display;           // Android: ANativeWindow*
      EPlatform Platform;
  };
  ```
- **선정 이유**: 플랫폼별 네이티브 핸들 타입(`HWND`, `ANativeWindow*`)을 단일 구조체로 통합하여, 상위 레이어가 플랫폼을 인식하지 않아도 됩니다.
- **기술적 분석**: `union`을 통해 같은 메모리 공간을 다른 타입으로 해석하며, `uintptr_t` 별칭으로 포인터 크기 정수 연산(비교, 직렬화)도 가능합니다. 이는 C에서도 쓰이는 고전적 Low-Level 기법입니다.

### 4.2 익명 네임스페이스로 파일 스코프 격리
- **관련 파일**: `Platforms/ExecWindows/ExecWindows.cpp`
- **코드 사례**:
  ```cpp
  namespace {
      std::unique_ptr<IExecute> execEngine = nullptr;
  }
  constexpr LPCWSTR kClassName = L"VaEngine";
  ```
- **선정 이유**: 전역 변수를 `static`으로 선언하는 구식 방법 대신, 익명 네임스페이스로 번역 단위(TU) 내부에 완전히 격리하여 링커 심볼 충돌을 방지합니다.
- **기술적 분석**: 익명 네임스페이스의 심볼은 외부 링키지가 없어(`internal linkage`) 다른 `.cpp` 파일에서 접근이 불가능합니다. 이는 C++11 이후 `static` 전역 변수의 권장 대체 수단입니다.

### 4.3 `throw std::runtime_error` 기반 초기화 실패 처리
- **관련 파일**: `Engine/Private/RHI/DirectX/RenderDevice_DirectX.cpp`
- **코드 사례**:
  ```cpp
  if (FAILED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                               IID_PPV_ARGS(&device)))) {
      throw std::runtime_error("Failed to create D3D12 device");
  }
  ```
- **선정 이유**: 그래픽 디바이스 초기화는 실패하면 엔진이 동작 불가능하므로, 반환값 체크를 강제하지 않는 오류 코드 방식보다 예외가 적합합니다.
- **기술적 분석**: 예외는 콜 스택을 통해 자동으로 전파되어 초기화 체인 어느 단계에서 실패해도 최상위 핸들러가 일관되게 처리할 수 있습니다. `unique_ptr`와 결합 시 예외 발생 후 자동 자원 해제(스택 언와인딩)가 보장됩니다.

### 4.4 GPU Drain 패턴 (종료 전 GPU 동기화)
- **관련 파일**: `Application/Source/Private/Execute.cpp`
- **코드 사례**:
  ```cpp
  // GPU가 모든 커맨드를 처리할 때까지 대기 (스핀 루프)
  commandQueue->Signal(fence.get(), ++fenceValue);
  while (fence->GetCompletedValue() < fenceValue) { /* spin */ }

  // 이후 리소스 해제
  commandList.reset();
  swapChain.reset();
  renderDevice.reset();
  ```
- **선정 이유**: GPU와 CPU 사이의 동기화 없이 CPU가 먼저 자원을 해제하면 GPU가 이미 해제된 메모리를 접근하는 TDR(Timeout Detection and Recovery) 크래시가 발생합니다.
- **기술적 분석**: 이는 DirectX 12 애플리케이션의 표준 종료 절차입니다. `unique_ptr::reset()` 호출 순서(CommandList → SwapChain → Device)도 의존성 역순을 따릅니다.

---

## 5. 추가로 적용 가능한 Modern C++ 패턴

### 5.1 `std::optional` 로 실패 가능 단계 분리
- **개선 위치**: `Engine/Private/RHI/DirectX/RenderDevice_DirectX.cpp` — `PickAdapter()`(89~121번째 줄)
  ```cpp
  // 현재: PickAdapter() 내부에서 직접 throw
  if (adapter == nullptr) {
      throw std::runtime_error("Failed to find a suitable GPU adapter");
  }

  // 개선: 탐색 결과를 optional로 반환, 호출자가 처리 방식 결정
  [[nodiscard]] std::optional<ComPtr<IDXGIAdapter4>> RenderDevice_DirectX::FindBestAdapter();

  // Initialize() 에서:
  auto found = FindBestAdapter();
  if (!found) { throw std::runtime_error("No suitable GPU adapter"); }
  adapter = std::move(*found);
  ```
- **효과**: 어댑터 탐색 로직을 순수 함수로 분리하여 단위 테스트 가능. `throw`는 호출부에서 한 곳에서만 관리.

### 5.2 `std::array` 로 C-스타일 배열 대체
- **개선 위치**: `Engine/Private/RHI/DirectX/SwapChain_DirectX.h` — 25번째 줄
  ```cpp
  // 현재
  ComPtr<ID3D12Resource> backBuffers[MAX_BUFFER_COUNT];

  // 개선
  std::array<ComPtr<ID3D12Resource>, MAX_BUFFER_COUNT> backBuffers;
  ```
- **효과**: `backBuffers.size()`, range-for, `std::fill` 등 STL 알고리즘 직접 적용 가능. 포인터 붕괴(array-to-pointer decay) 없음. `CreateRTV()` 내부의 인덱스 루프를 range-for로 단순화 가능.

---

## 6. 역량 요약

| 카테고리 | 기법 | 파일 |
|---|---|---|
| 스마트 포인터 | `unique_ptr` 팩토리 체인 | `IRenderDevice.h`, `Execute.cpp` |
| COM 관리 | `ComPtr<T>` (WRL) | `RenderDevice_DirectX.h` |
| 컴파일 타임 | `#if USE_DX12` RHI 선택 | `RHILoader.cpp` |
| 컴파일 타임 | `USE_DEBUGGING` 다중 백엔드 검증 | `RHILoader.cpp`, `CMakePresets.json` |
| 컴파일 타임 | `[[nodiscard]]` 팩토리 반환값 강제 | `IRenderDevice.h` |
| 인터페이스 설계 | `std::span` 커맨드 리스트 제출 | `ICommandQueue.h` |
| 컴파일 타임 | 지정 초기화(Designated Initializers) | `Common_RHI.h` 호출부 |
| 컴파일 타임 | `static constexpr` 버퍼 수 | `SwapChain_DirectX.h` |
| 인터페이스 설계 | 순수 가상 + `= default` 소멸자 | `IRenderDevice.h` |
| 다운캐스팅 | `static_cast` (RTTI 없음) | `CommandQueue_DirectX.cpp` |
| Low-Level | `union` 플랫폼 핸들 | `Common_RHI.h` |
| Low-Level | 익명 네임스페이스 격리 | `ExecWindows.cpp` |
| 예외 처리 | `throw std::runtime_error` | `RenderDevice_DirectX.cpp` |
| GPU 동기화 | Drain 패턴 (종료 전 대기) | `Execute.cpp` |
