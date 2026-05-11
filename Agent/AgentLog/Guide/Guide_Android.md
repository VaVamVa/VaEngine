# Android Vulkan 빌드 & 실행 가이드

---

## 목차

1. [사전 요구사항](#1-사전-요구사항)
2. [셰이더 준비 (필수)](#2-셰이더-준비-필수)
3. [빌드 방법 — Android Studio](#3-빌드-방법--android-studio)
4. [빌드 방법 — 커맨드라인 (gradlew)](#4-빌드-방법--커맨드라인-gradlew)
5. [실기기에서 실행](#5-실기기에서-실행)
6. [에뮬레이터에서 실행 (PC)](#6-에뮬레이터에서-실행-pc)
7. [로그 확인 (adb logcat)](#7-로그-확인-adb-logcat)
8. [트러블슈팅](#8-트러블슈팅)

---

## 1. 사전 요구사항

### 필수 소프트웨어

| 항목 | 버전 | 비고 |
|------|------|------|
| Android Studio | Flamingo (2022.2) 이상 | [developer.android.com](https://developer.android.com/studio) |
| JDK | 11 이상 | Android Studio 번들 JDK 사용 가능 |
| Android SDK | API 33 (Android 13) | Android Studio SDK Manager에서 설치 |
| Android NDK | **30.0.14904198** | 프로젝트에 `Platforms/ExecAndroid/ndk/`로 내장되어 있음 |
| Vulkan SDK | 1.3 이상 | 셰이더 컴파일(glslc)에만 필요, Windows 빌드 시 이미 설치됨 |

### 프로젝트의 NDK 위치

이 프로젝트는 NDK를 내장하고 있습니다. 별도 설치 불필요.

```
VaEngine/Platforms/ExecAndroid/ndk/30.0.14904198/
```

### 지원 ABI (빌드 플레이버)

| 플레이버 | 대상 기기 |
|---------|---------|
| `arm8` | 현대 Android 기기 (권장) |
| `arm7` | 구형 32비트 ARM 기기 |
| `x86` | 에뮬레이터 (32비트) |
| `x86-64` | 에뮬레이터 (64비트, 권장) |
| `all` | 모든 ABI 포함 (APK 용량 큼) |

---

## 2. 셰이더 준비 (필수)

Android 앱은 GLSL을 직접 컴파일할 수 없습니다.  
**Windows에서 미리 SPIR-V로 컴파일한 뒤** APK assets 폴더에 배치해야 합니다.

### 2-1. SPIR-V 컴파일 (Windows에서 실행)

Windows 빌드를 한 번 실행하면 자동으로 컴파일됩니다.

```bash
# windows-vulkan 프리셋으로 빌드
cmake --preset windows-vulkan
cmake --build Build/windows-vulkan --config Debug
```

빌드 후 아래 경로에 `.spv` 파일이 생성됩니다:

```
Build/windows-vulkan/ShaderIntermediate/Vulkan/
  mesh.vert.spv
  mesh.frag.spv
```

### 2-2. SPIR-V를 Android assets 폴더에 복사

아래 경로로 복사합니다:

```
Platforms/ExecAndroid/app/src/main/assets/Shaders/Vulkan/
  mesh.vert.spv   ← 복사
  mesh.frag.spv   ← 복사
```

Gradle이 빌드 시 `assets/` 폴더를 APK에 자동으로 패키징합니다.

### 2-3. glslc로 직접 컴파일 (선택)

Vulkan SDK가 설치된 경우 직접 컴파일도 가능합니다:

```bash
# Vulkan SDK의 glslc 사용 (경로는 설치 위치에 따라 다름)
glslc Engine/Shaders/Vulkan/mesh.vert -o mesh.vert.spv
glslc Engine/Shaders/Vulkan/mesh.frag -o mesh.frag.spv
```

---

## 3. 빌드 방법 — Android Studio

### 3-1. 프로젝트 열기

1. Android Studio 실행
2. **File → Open** 선택
3. 다음 경로를 선택:
   ```
   VaEngine/Platforms/ExecAndroid/
   ```
   > `settings.gradle`이 있는 폴더를 엽니다. `VaEngine/` 루트가 아닙니다.

### 3-2. Gradle Sync

프로젝트가 열리면 자동으로 Sync가 시작됩니다.  
오류가 발생하면 **File → Sync Project with Gradle Files** 를 수동 실행합니다.

### 3-3. 빌드 구성 선택

상단 툴바의 빌드 구성 드롭다운에서 선택:

```
[arm8] [Debug ▼]
```

- 실기기: `arm8Debug`
- 에뮬레이터: `x86-64Debug`

### 3-4. APK 빌드

```
메뉴 → Build → Build Bundle(s) / APK(s) → Build APK(s)
```

빌드 완료 후 APK 위치:

```
Platforms/ExecAndroid/app/build/outputs/apk/arm8/debug/app-arm8-debug.apk
```

---

## 4. 빌드 방법 — 커맨드라인 (gradlew)

`Platforms/ExecAndroid/` 디렉토리에서 실행합니다.

```bash
cd VaEngine/Platforms/ExecAndroid

# arm8 Debug APK 빌드
./gradlew assembleArm8Debug       # Linux/Mac
gradlew.bat assembleArm8Debug     # Windows

# x86-64 Debug (에뮬레이터용)
./gradlew assembleX86-64Debug

# 모든 ABI + Debug
./gradlew assembleAllDebug
```

> `gradlew.bat`은 `Platforms/ExecAndroid/` 폴더에 있습니다.

---

## 5. 실기기에서 실행

### 5-1. 기기 준비

1. Android 기기의 **설정 → 휴대폰 정보 → 빌드 번호** 를 7번 탭
2. **설정 → 개발자 옵션 → USB 디버깅** 활성화
3. USB로 PC에 연결, 기기에서 "USB 디버깅 허용" 확인

### 5-2. Vulkan 지원 확인

이 앱은 Vulkan을 필수로 사용합니다. 기기가 **Android 10 (API 29) 이상**이어야 합니다.  
대부분의 2019년 이후 출시 기기는 Vulkan 1.1 이상을 지원합니다.

### 5-3. 실행

**Android Studio에서:**

1. 기기 연결 상태 확인 (상단 기기 목록에 표시됨)
2. 빌드 구성을 `arm8Debug`로 설정
3. ▶ (Run) 버튼 클릭

**adb로 직접 설치:**

```bash
adb install app/build/outputs/apk/arm8/debug/app-arm8-debug.apk
```

---

## 6. 에뮬레이터에서 실행 (PC)

### 6-1. Vulkan을 지원하는 AVD 생성

Android Emulator는 **API 29 이상**에서 Vulkan을 지원합니다.  
호스트 GPU를 직접 활용하므로 별도의 하드웨어 가속이 필요합니다.

**Android Studio → Device Manager → Create Device:**

| 항목 | 권장 설정 |
|------|---------|
| 기기 | Pixel 6 (또는 Pixel 5) |
| System Image | **API 33, x86_64, Google APIs** |
| Graphics | **Hardware - GLES 2.0** (Vulkan 자동 활성화) |
| RAM | 2048 MB 이상 |

> **중요:** `x86_64` 이미지를 선택해야 PC에서 하드웨어 가속이 됩니다.  
> `arm64` 이미지는 소프트웨어 에뮬레이션으로 매우 느립니다.

### 6-2. Windows 하드웨어 가속 설정

에뮬레이터 성능을 위해 아래 중 하나가 필요합니다:

| 가속 방식 | 조건 |
|---------|------|
| **WHPX** (Windows Hypervisor Platform) | Windows 10/11, 설정에서 활성화 |
| **HAXM** | Intel CPU 전용 |

**WHPX 활성화:**
```
제어판 → 프로그램 → Windows 기능 켜기/끄기
→ "Windows 하이퍼바이저 플랫폼" 체크
→ 재부팅
```

### 6-3. 에뮬레이터 실행

1. Device Manager에서 생성한 AVD의 ▶ 클릭
2. 에뮬레이터 부팅 완료 후 빌드 구성을 `x86-64Debug`로 변경
3. ▶ (Run) 버튼 클릭

**커맨드라인으로 APK 설치:**

```bash
# 에뮬레이터가 실행 중인 상태에서
adb -e install app/build/outputs/apk/x86-64/debug/app-x86-64-debug.apk
```

### 6-4. Vulkan 동작 확인

에뮬레이터에서 Vulkan이 정상 동작하는지 확인하려면:

```bash
adb shell vulkaninfo 2>&1 | head -20
```

`Vulkan Instance Version` 항목이 출력되면 Vulkan 지원 확인입니다.

---

## 7. 로그 확인 (adb logcat)

### 7-1. 전체 로그 (태그 필터)

```bash
# VaEngine 관련 로그만 출력
adb logcat -s "VaEngine" "vulkan" "Vulkan"

# Vulkan Validation Layer 로그 포함
adb logcat -s "VALIDATION" "vulkan" "VkLayer"
```

### 7-2. 크래시 로그 확인

```bash
# 네이티브 크래시(SIGSEGV 등) 로그
adb logcat | grep -E "FATAL|SIGSEGV|tombstone"
```

### 7-3. 로그 레벨

| 태그 예시 | 의미 |
|---------|------|
| `E/VkLayer` | Vulkan Validation Layer 에러 |
| `W/VkLayer` | Vulkan 경고 |
| `I/VaEngine` | 앱 정보 로그 |
| `D/gralloc` | GPU 메모리 할당 관련 |

---

## 8. 트러블슈팅

### ❌ `ANativeActivity_onCreate` 심볼 오류

```
undefined reference to 'ANativeActivity_onCreate'
```

**원인:** 링커가 `android_native_app_glue`의 진입점 심볼을 제거함  
**해결:** `ExecAndroid/CMakeLists.txt`의 링크 옵션 확인
```cmake
target_link_options(ExecAndroid PRIVATE -u ANativeActivity_onCreate)
```

---

### ❌ 셰이더 로드 실패 (앱 시작 시 즉시 크래시)

```
[Vulkan] APK 에셋에서 SPIR-V를 열 수 없습니다: Shaders/Vulkan/mesh.vert.spv
```

**원인:** `assets/Shaders/Vulkan/` 폴더에 `.spv` 파일이 없음  
**해결:** [2. 셰이더 준비](#2-셰이더-준비-필수) 항목을 따라 `.spv` 파일 복사

---

### ❌ Vulkan Surface 생성 실패

```
[Vulkan] vkCreateAndroidSurfaceKHR failed
```

**원인:** 기기가 Vulkan을 지원하지 않거나 API < 29  
**해결:** `adb shell getprop ro.product.first_api_level` 로 API 레벨 확인. 29 미만이면 미지원

---

### ❌ 에뮬레이터에서 검은 화면 (렌더링 없음)

**원인:** 에뮬레이터 Graphics 설정이 소프트웨어(SwiftShader)로 되어 있을 수 있음  
**해결:** AVD 편집 → Show Advanced Settings → Graphics: **Hardware - GLES 2.0** 으로 변경

---

### ❌ Gradle Sync 실패 — NDK 경로 오류

```
NDK not configured. Download it with SDK manager.
```

**원인:** Android Studio가 내장 NDK 경로를 인식하지 못함  
**해결:** `app/build.gradle`에 NDK 경로 명시 추가

```groovy
android {
    ndkPath "../ndk/30.0.14904198"  // 추가
    ...
}
```
