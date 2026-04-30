# VaEngine

DirectX12 + Vulkan 크로스 플랫폼 3D 렌더링 엔진.

---

## 빌드 환경

- Visual Studio 2022 | Rider
- CMake 3.20 이상
- C++20
- Windows SDK / DirectX12 최신 버전

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
