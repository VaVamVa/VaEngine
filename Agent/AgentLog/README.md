# VaEngine — 프로젝트 현황판

> 마지막 업데이트: 2026-04-30

---

## 프로젝트 개요

DirectX12 + Vulkan 크로스 플랫폼 3D 렌더링 엔진.
개발 방식은 개념/가이드 위주 학습. 코드는 컴파일 가능한 수준으로 완성도 있게 작성.

---

## 확정 아키텍처 (2026-05-01 기준)

```
Engine.lib      ← 렌더러, RHI 인터페이스 (IRenderDevice 등)
Application.lib ← IApplication 인터페이스 정의

Platforms/
├── ExecWindows.exe ← WindowsApplication(IApplication 구현) + WinMain Loop
└── ExecAndroid     ← AndroidApplication(IApplication 구현) + NativeActivity Loop
```

**빌드 의존성:**
```
Engine.lib ← Application.lib ← ExecWindows.exe
                              ← ExecAndroid
```

**IApplication 설계 원칙:**
- Loop 호출 주체 = Platform (ExecWindows / ExecAndroid)
- 구현체 = 각 Exec 프로젝트 (HWND / ANativeWindow 직접 사용)
- Engine은 IApplication을 알지 못함 (렌더링 인프라만 제공)

---

## 현재 디렉토리 구조

```
VaEngine/
├── VaEngine.slnx
├── ThirdParty/
│   ├── Main.py                    ← 자동화 코드 진입점
│   └── GeneratePublicHeader.py    ← InnerDependencies.h 갱신 + Private cpp 자동 생성
├── Engine/
│   ├── Public/
│   │   ├── InnerDependencies.h    ← 자동 관리 (빌드 시 갱신)
│   │   └── Interfaces/            ← (예정) IRenderDevice.h 등
│   ├── Private/
│   │   ├── RHI/
│   │   │   ├── DX12/              ← (예정) DX12RenderDevice
│   │   │   └── Vulkan/            ← (예정) VulkanRenderDevice
│   │   └── Core/                  ← (예정) 플랫폼 무관 내부 구현
│   └── Engine.cpp
├── Application/
│   ├── Public/Interfaces/
│   │   └── IExecute.h             ← 순수 인터페이스 (Application 소유)
│   └── Application.cpp
└── Platforms/
    ├── ExecWindows/
    │   ├── WindowsApplication     ← IApplication 구현체 (예정)
    │   └── ExecWindows.cpp        ← WinMain + Loop
    └── ExecAndroid/               ← Gradle 빌드 환경 설정 완료
```

> **Engine = 플랫폼 무관 원칙**: `windows.h`, Win32 API는 Engine에 침투하지 않음.
> 플랫폼 코드는 ExecWindows / ExecAndroid 프로젝트 내부에만 위치.

---

## 프로젝트 상태

| 프로젝트 | 타입 | 빌드 | 코드 작성 |
|---|---|---|---|
| Engine | Static Library | ✅ 성공 | 🔧 자동화 환경 구성 완료 |
| Application | Static Library | ✅ 성공 | 🔧 IExecute.h 작성 완료 |
| ExecWindows | Executable | ✅ 성공 | ⬜ 기본 틀만 |
| ExecAndroid | Android (Gradle) | ✅ 성공 | ⬜ 기본 틀만 |

---

## 핵심 설계 원칙

### Public / Private 헤더 분리
| Public (외부 노출) | Private (Engine 내부 전용) |
|---|---|
| 인터페이스 (`I~~`) | 구현체 (`DX12Device`, `VulkanDevice`) |
| 외부가 쓰는 타입 (`Handle`, `Vector`) | 내부 헬퍼, `d3d12.h` / `vulkan.h` 포함 파일 |

판단 기준: **"Application.lib 코드가 이 헤더를 `#include`할 필요가 있는가?"**

### 렌더링 백엔드 추상화
```cpp
class IRenderDevice { ... };               // Engine/Public/ — 플랫폼 무관
class DX12RenderDevice : IRenderDevice {}; // Engine/Private/RHI/DX12/ — Windows
class VulkanRenderDevice : IRenderDevice {}; // Engine/Private/RHI/Vulkan/ — Android/Linux
```

Engine이 모든 그래픽 API 소스를 소유. 패키징 시 `CMakePresets.json` + `option(USE_DX12/USE_VULKAN)` 으로 백엔드 선택.

| 대상 | USE_DX12 | USE_VULKAN |
|---|---|---|
| Windows | ON | OFF |
| Android / Linux | OFF | ON |

### 플랫폼 핸들 추상화
`FNativeWindowInfo` (`Engine/Public/`) — `void* Handle` + `EPlatform` enum. 플랫폼 헤더 없음.
`#if` 는 `Engine/Private/RHI/` 구현부에만 존재.

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
- **IDE**: Visual Studio 2022
- **빌드**: CMake (추후 도입 예정, 현재 VS 프로젝트)
- **SDK**: Windows SDK / DirectX 12 최신, Android NDK

---

## 문서 구조

```
Agent/
├── Context.md               ← 작업 규칙 원본
└── AgentLog/
    ├── README.md            ← 이 파일 (현황판)
    ├── {날짜}_Log.md        ← Start / Compact / End Log
    └── {날짜}_Q&A.md        ← 질문 / 답변 기록
```

| 파일 | 내용 |
|---|---|
| [2026-03-16.md](2026-03-16.md) | Q10 아키텍처 확정 전 설계 토론 (Q1~Q10) |
| [2026-04-22_Q&A.md](2026-04-22_Q&A.md) | Public/Private 분리, RHI 구조, Platform 위치, 라이프사이클, 빌드 워크플로우 |
| [2026-04-22_Log.md](2026-04-22_Log.md) | Q10 아키텍처 재구성, 프로젝트 4개 생성, Android 빌드 오류 해결 |
| [2026-04-30_Q&A.md](2026-04-30_Q&A.md) | ExecAndroid 참조 경고, IApplication 혼용 문제, 플랫폼별 구현 구조 확정 |
| [2026-04-30_Log.md](2026-04-30_Log.md) | 참조 설정, 자동화 도입, IApplication 아키텍처 재설계, RHI 백엔드 아키텍처 확정 |
| [2026-05-01_Q&A.md](2026-05-01_Q&A.md) | CMake 파일 수집 방식, RHI 백엔드 CMake 제어, FNativeWindowInfo 구조, Vulkan/DX12 Surface 생성 |
