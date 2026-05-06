# VaEngine — 프로젝트 현황판

> 마지막 업데이트: 2026-05-06
>
> **[공지] 이 문서는 현황판입니다. ToDo 항목은 각 날짜의 Log 파일에만 기록합니다.**

---

## 프로젝트 개요

DirectX12 + Vulkan 크로스 플랫폼 3D 렌더링 엔진.
개발 방식은 개념/가이드 위주 학습. 코드는 컴파일 가능한 수준으로 완성도 있게 작성.

---

## 확정 아키텍처 (2026-05-04 기준)

**레이어 순서:**
```
Platform (OS 및 Target Properties 결정)
  → Engine  (Target에 맞는 API 채택, IExecute 정의)
    → Application (Client — Execute 구현체, 앱 로직)
```

**빌드 의존성:**
```
Engine.lib ← Application.lib ← ExecWindows.exe
                              ← ExecAndroid.so
```

**각 레이어 역할:**

| 레이어 | 프로젝트 | 역할 |
|---|---|---|
| Platform | ExecWindows / ExecAndroid | OS 진입점, Loop, IExecute 구현체 인스턴스화 및 실행 |
| Engine | Engine.lib | RHI 인터페이스, IExecute 정의, 플랫폼 무관 인프라 |
| Client | Application.lib | Execute 구현체 (IExecute), 앱 로직, RHI 초기화 |

**IExecute 설계 원칙:**
- `IExecute.h` → `Engine/Public/Interfaces/` 소유
- Loop 호출 주체 = Platform (ExecWindows / ExecAndroid)
- 구현체 `Execute` = `Application/Source/` (플랫폼 헤더 없이 `NativeDisplayInfo`만 수신)
- Application은 Engine 공개 인터페이스만 참조

---

## 현재 디렉토리 구조

```
VaEngine/
├── CMakeLists.txt              ← 최상위. option(USE_DIRECTX/USE_VULKAN), 전체 add_subdirectory
├── CMakePresets.json           ← windows-directx / windows-vulkan / android / engine 프리셋
├── ThirdParty/
│   └── DirectX-Headers/       ← d3dx12 헬퍼 헤더 (MS 공식, FetchContent)
├── Engine/
│   ├── CMakeLists.txt          ← FILE_SET HEADERS, 조건부 RHI 소스, compile_definitions
│   ├── Public/
│   │   ├── Common_Display.h    ← NativeDisplayInfo (EPlatform, Handle, Display)
│   │   ├── RHI/
│   │   │   ├── RHILoader.h     ← namespace RHI::CreateRenderDevice() 선언
│   │   │   ├── Common_RHI.h    ← SwapChainDesc, EResourceState(플래그), ResourceBarrier 등
│   │   │   ├── IRenderDevice.h ← GPU 초기화 + RHI 객체 Factory
│   │   │   ├── ISwapChain.h    ← Present / Resize / GetCurrentBackBufferRTV
│   │   │   ├── ICommandQueue.h ← ExecuteCommandLists / Signal / Wait
│   │   │   ├── ICommandList.h  ← Begin / Close / SetResourceBarrier / ClearRenderTargetView
│   │   │   ├── ICommandAlloc.h ← Register / Reset
│   │   │   ├── IFence.h        ← SetEventOnComplete / GetCompletedValue
│   │   │   ├── IGPUBuffer.h    ← (예약)
│   │   │   └── IRHIResource.h  ← (예약)
│   │   └── Interfaces/
│   │       └── IExecute.h      ← 앱 생명주기 인터페이스
│   ├── Private/
│   │   ├── RHI/
│   │   │   ├── RHILoader.cpp           ← USE_DIRECTX/USE_VULKAN/USE_DEBUGGING 분기
│   │   │   └── DirectX/               ← USE_DIRECTX일 때만 컴파일
│   │   │       ├── Common_DirectX.h        ← ComPtr, DXGI, D3D12, d3dx12
│   │   │       ├── RenderDevice_DirectX.h/.cpp
│   │   │       ├── CommandQueue_DirectX.h/.cpp
│   │   │       ├── CommandList_DirectX.h/.cpp
│   │   │       ├── CommandAlloc_DirectX.h/.cpp
│   │   │       ├── SwapChain_DirectX.h/.cpp
│   │   │       └── Fence_DirectX.h/.cpp
│   │   └── Vulkan/                    ← USE_VULKAN일 때만 컴파일
│   └── Engine.cpp
├── Application/
│   ├── CMakeLists.txt
│   ├── Application.cpp
│   └── Source/
│       ├── Public/
│       │   └── Execute.h       ← IExecute 구현체 선언
│       └── Private/
│           └── Execute.cpp     ← OnInitialize / OnDestroy / OnLoop 구현
└── Platforms/
    ├── ExecWindows/
    │   ├── CMakeLists.txt      ← WIN32 가드, WIN32_EXECUTABLE, UNICODE 정의
    │   └── ExecWindows.cpp     ← WinMain + PeekMessage 루프 + IExecute 연결
    └── ExecAndroid/
        ├── CMakeLists.txt      ← ANDROID 가드, SHARED
        └── ndk/                ← 로컬 NDK (gitignore)
```

> **Engine = 플랫폼 무관 원칙**: `windows.h`, Win32 API는 Engine에 침투하지 않음.
> DX12 전용 헤더(`d3d12.h` 등)는 `Engine/Private/RHI/DirectX/` 내부에만 존재.

---

## 프로젝트 상태

| 프로젝트 | 타입 | 빌드 | 코드 작성 |
|---|---|---|---|
| Engine | Static Library | ✅ 성공 | 🔧 RHI 인터페이스 완성, DX12 핵심 객체 구현 완료 |
| Application | Static Library | ✅ 성공 | 🔧 Execute 구현 — 초기화/종료 완성, OnRender 작업 중 |
| ExecWindows | Executable | ✅ 성공 | ✅ WinMain 구현 완료, 창 실행 확인 |
| ExecAndroid | Android | 미확인 | ⬜ 기본 틀만 |

---

## 핵심 설계 원칙

### Public / Private 헤더 분리
| Public (외부 노출) | Private (Engine 내부 전용) |
|---|---|
| 인터페이스 (`I~~`) | 구현체 (`RenderDevice_DirectX` 등) |
| 공통 구조체 (`NativeDisplayInfo`, `SwapChainDesc`) | DX12/Vulkan 전용 헤더 (`d3d12.h`, `vulkan.h`) |

판단 기준: **"Engine 외부(Application / ExecPlatform)가 이 헤더를 `#include`할 필요가 있는가?"**

### 렌더링 백엔드 추상화
```cpp
class IRenderDevice { ... };                      // Engine/Public/RHI/ — 플랫폼 무관
class RenderDevice_DirectX : IRenderDevice {};    // Engine/Private/RHI/DirectX/ — Windows
class RenderDevice_Vulkan  : IRenderDevice {};    // Engine/Private/RHI/Vulkan/  — Android/Linux
```

Engine이 모든 그래픽 API 소스를 소유. `CMakePresets.json` + `option(USE_DIRECTX/USE_VULKAN)`으로 백엔드 선택.

| 프리셋 | USE_DIRECTX | USE_VULKAN | USE_DEBUGGING | 용도 |
|---|---|---|---|---|
| windows-directx | ON | OFF | OFF | DX12 사용자 빌드 |
| windows-vulkan | OFF | ON | OFF | Vulkan 사용자 빌드 |
| android | OFF | ON | OFF | Android Vulkan 빌드 |
| engine | ON | ON | ON | 엔진 개발자 — 전 백엔드 인터페이스 검증 |

### RHI 팩토리
```cpp
// 소비자 코드
auto device = RHI::CreateRenderDevice();
device->Initialize();
auto queue     = device->CreateCommandQueue(queueDesc);
auto fence     = device->CreateFence();
auto swapChain = device->CreateSwapChain(swapChainDesc);
auto alloc     = device->CreateCommandAllocator(allocDesc);
auto cmdList   = device->CreateCommandList(listDesc);
```

### 플랫폼 핸들 추상화
`NativeDisplayInfo` (`Engine/Public/Common_Display.h`) — `void* Handle` + `EPlatform` enum. 플랫폼 헤더 없음.
`#if`는 `Engine/Private/RHI/` 구현부에만 존재.

### 플랫폼별 라이프사이클
| | ExecWindows | ExecAndroid |
|---|---|---|
| 진입점 | `WinMain` | `ANativeActivity_onCreate` |
| 렌더링 시작 | 창 생성 즉시 | `onNativeWindowCreated` 이후 |
| 이벤트 처리 | `PeekMessage` | `ALooper_pollAll` |
| 백그라운드 전환 | `WM_ACTIVATE` | Surface 소멸 → 렌더러 일시 해제 |

---

## 코드 규칙

| 항목 | 규칙 |
|---|---|
| 변수 | camelCase |
| 함수 / 클래스 / 구조체 | PascalCase |
| 순수 가상 클래스 | `I~~` |
| 공통 구현 포함 가상 클래스 | `Base~~` |
| NVI 훅 / Bridge 실제 구현 함수 | `Impl_~~()` |
| COM 리소스 관리 | `ComPtr` 필수, RAII 준수 |
| HRESULT | 반드시 검사, 에러 시 예외 처리 |
| 커맨드 리스트/큐 | 목적별 분리 (Direct / Copy / Compute) |
| 리소스 상태 전환 | Barrier 명시적으로 표현 |

---

## 환경

- **언어**: C++20, HLSL
- **IDE**: Rider (JetBrains) / Visual Studio 2022
- **빌드**: CMake + CMakePresets.json
- **SDK**: Windows SDK / DirectX 12 최신, Android NDK

---

## 문서 구조

```
Agent/
├── Context.md               ← 작업 규칙 원본
└── AgentLog/
    ├── README.md            ← 이 파일 (현황판, ToDo 제외)
    ├── {날짜}_Log.md        ← Start / Compact / End Log (ToDo 포함)
    └── {날짜}_Q&A.md        ← 질문 / 답변 기록
```

| 파일 | 내용 |
|---|---|
| [2026-04-22_Q&A.md](2026-04-22_Q&A.md) | Public/Private 분리, RHI 구조, Platform 위치, 라이프사이클, 빌드 워크플로우 |
| [2026-04-22_Log.md](2026-04-22_Log.md) | Q10 아키텍처 재구성, 프로젝트 4개 생성, Android 빌드 오류 해결 |
| [2026-04-30_Q&A.md](2026-04-30_Q&A.md) | ExecAndroid 참조 경고, IApplication 혼용 문제, 플랫폼별 구현 구조 확정 |
| [2026-04-30_Log.md](2026-04-30_Log.md) | 참조 설정, IApplication 아키텍처 재설계, RHI 백엔드 아키텍처 확정, FNativeWindowInfo 구조체 설계 |
| [2026-05-01_Q&A.md](2026-05-01_Q&A.md) | CMake 파일 수집, RHI 백엔드 CMake 제어, FNativeWindowInfo, Vulkan/DX12 Surface, NDK, Android Vulkan, 레이어 순서 확정 |
| [2026-05-02_Q&A.md](2026-05-02_Q&A.md) | IExecute 구현체 명명, WinMain 설계, RHI 인터페이스 설계, DX12 개요, Factory 패턴, FILE_SET, USE_DX12 매크로, DirectX-Headers |
| [2026-05-02_Log.md](2026-05-02_Log.md) | WinMain 구현 완료, RHI 인터페이스 전체 선언, RHILoader 팩토리, DX12 환경 구성 |
| [2026-05-03_Log.md](2026-05-03_Log.md) | DX12 RHI 핵심 객체 구현 완료 (CommandQueue, Fence, SwapChain), ThirdParty FetchContent 전환 |
| [2026-05-04_Q&A.md](2026-05-04_Q&A.md) | IExecute 필요성, Execute 위치 결정, 엔진 배포 전략 |
| [2026-05-04_Log.md](2026-05-04_Log.md) | Execute → Application 이동, RHI 인터페이스 재구조화, CommandList/CommandAlloc 구현, engine 프리셋 추가 |
| [2026-05-05_Q&A.md](2026-05-05_Q&A.md) | if constexpr vs #if, USE_DEBUGGING 설계, engine 프리셋 구조, CMakePresets 검증 |
| [2026-05-05_Log.md](2026-05-05_Log.md) | 첫 렌더링 명령(Clear) 및 WaitForPreviousFrame 구현 예정 |
