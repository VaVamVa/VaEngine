# VaEngine

DirectX12 + Vulkan 크로스 플랫폼 3D 렌더링 엔진.

---

## 프로젝트 현황판

`Agent/AgentLog/README.md`

---

## 아키텍처

### 계층 구조

```
Platform  (ExecWindows / ExecAndroid)
    │  OS 진입점, 메시지 루프, Graphics API 제공
    ▼
Engine  (Static Library)
    │  렌더 루프 소유, RHI 추상화, 씬 렌더링
    │  ApplicationManager 인터페이스를 통해 Application과 통신
    ▼
Application/Core  (Static Library)
    │  앱 간 공통 유틸리티 (FreeCamera, Character, GameObject 등)
    │  Engine Public API만 참조
    ▼
Application/{Source}  (Client 작성 영역)
       게임/앱 고유 로직, ApplicationManager 구현체
       타겟이 바뀌면 이 디렉토리만 교체
```

### 데이터 흐름

```
Application
  └─ OnUpdate()          게임 로직, Transform 갱신
  └─ SubmitRenderState() 렌더링할 메시·카메라 스냅샷 제출
          │
          ▼
Engine
  └─ RenderGraph 빌드    렌더 패스 순서 결정
  └─ Renderer 실행       GPU 커맨드 기록 및 Present
```

Application은 렌더링 명령에 전혀 관여하지 않는다.
Engine은 게임 로직을 모른다.

---

## 프로젝트 구성

| 프로젝트 | 타입 | 역할 |
|---|---|---|
| `Engine` | Static Library | RHI 추상화, 렌더 루프, 씬 렌더링 인프라. DX12/Vulkan 백엔드 포함. |
| `Application/Core` | Static Library | 공통 유틸리티 (카메라, 캐릭터, 기본 오브젝트). Engine을 사용하는 앱이라면 재사용 가능. |
| `Application/{Source}` | Static Library | 프로그램 고유 로직. `ApplicationManager` 구현체. 프로젝트마다 교체. |
| `ExecWindows` | Executable | Windows 진입점 (WinMain). DirectX 12 또는 Vulkan 백엔드 선택. |
| `ExecAndroid` | Shared Library | Android 진입점 (NativeActivity). Vulkan 백엔드. |

---

## 빌드 환경

- Visual Studio 2022 | Rider
- CMake 3.20 이상
- C++20
- Windows SDK / DirectX 12 최신 버전

---

## DirectX 12

Visual Studio Installer에서 **"C++를 사용한 데스크톱 개발"** 워크로드 설치 시 자동으로 포함됨. 별도 설치 불필요.

---

## Vulkan SDK 설치 (windows-vulkan 프리셋 사용 시)

- 권장 버전: **1.3.x 최신 안정 버전**
- 다운로드: [https://vulkan.lunarg.com/sdk/home](https://vulkan.lunarg.com/sdk/home)

설치 후 `VULKAN_SDK` 환경변수가 자동 등록되며 CMake가 자동으로 탐색함.

> `windows-dx12` 프리셋만 사용하는 경우 Vulkan SDK 설치 불필요.

---

## Android 빌드를 위한 NDK 설치

Android 프리셋(`android`)으로 빌드하려면 NDK가 필요함.

### NDK 설치 경로

압축 해제 후 아래 경로에 배치:

```
VaEngine/
└── VaEngine/
    └── Platforms/
        └── ExecAndroid/
            └── ndk/        ← 여기에 NDK 내용물 직접 배치
                ├── build/
                ├── platforms/
                ├── toolchains/
                └── ...
```

### NDK 다운로드

- [NDK 다운로드 링크](https://drive.google.com/drive/folders/17rIH7HEMImozdsT9SUYioRr0NdCs8wjn?usp=sharing)
- [공식 홈페이지](https://developer.android.com/tools/sdkmanager)

---

## CMake 프리셋

| 프리셋 | 타깃 | 그래픽 API |
|---|---|---|
| `windows-dx12` | Windows x64 | DirectX 12 |
| `windows-vulkan` | Windows x64 | Vulkan |
| `android` | Android arm64 | Vulkan |

IDE에서 폴더 열기 → 상단 툴바 프리셋 드롭다운에서 선택.
