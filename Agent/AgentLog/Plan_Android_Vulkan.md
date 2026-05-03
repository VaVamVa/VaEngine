# Android Vulkan 구현 계획

## Context

Windows-Vulkan 브랜치에서 Vulkan RHI + 3D 큐브 렌더링이 완료된 상태.  
Surface 생성(`vkCreateWin32SurfaceKHR`)과 셰이더 로드(파일 시스템)가 Windows 전용으로 구현되어 있음.  
같은 `Execute` 클래스와 Vulkan RHI를 Android에서도 동작하도록 플랫폼 분기를 추가한다.

---

## 목표

- `ExecAndroid`가 `android_native_app_glue` 기반으로 `Execute`를 초기화·루프 호출
- `SwapChain_Vulkan::CreateSurface()`에 `vkCreateAndroidSurfaceKHR` 분기 추가
- `Pipeline_Vulkan`이 Android에서 APK assets의 SPIR-V를 `AAssetManager`로 로드
- `NativeDisplayInfo.Display` = `AAssetManager*` (Android) 로 활용
- 셰이더: 사전 컴파일된 `.spv`를 `ExecAndroid/app/src/main/assets/Shaders/Vulkan/`에 배치

---

## DX12/Windows ↔ Android 개념 대응

| Windows | Android | 역할 |
|---------|---------|------|
| `WinMain` | `android_main(android_app*)` | 진입점 |
| `HWND` | `ANativeWindow*` | 렌더 대상 창 핸들 |
| `HINSTANCE` | `AAssetManager*` | 부가 컨텍스트 (여기선 에셋 로더로 활용) |
| `PeekMessage` 루프 | `ALooper_pollAll` 루프 | OS 이벤트 처리 |
| `WM_ACTIVATE` | `APP_CMD_GAINED/LOST_FOCUS` | 포커스 전환 |
| `WM_DESTROY` | `APP_CMD_TERM_WINDOW` | 창 소멸 |
| 파일 시스템 fopen | `AAssetManager_open` | 에셋(셰이더) 로드 |

---

## 수정/생성 파일 목록

### [Engine] 수정

| 파일 | 변경 내용 |
|------|---------|
| `Engine/Private/RHI/Vulkan/Common_Vulkan.h` | `#ifdef ANDROID` 분기 추가 — `<android/native_window.h>`, `<vulkan/vulkan_android.h>`, `VK_KHR_ANDROID_SURFACE_EXTENSION_NAME` |
| `Engine/Private/RHI/Vulkan/SwapChain_Vulkan.cpp` | `CreateSurface()` — `#elif defined(ANDROID)` 에 `VkAndroidSurfaceCreateInfoKHR` + `vkCreateAndroidSurfaceKHR` 추가 |
| `Engine/Private/RHI/Vulkan/Pipeline_Vulkan.h` | `Initialize()` 마지막 파라미터로 `void* assetManager = nullptr` 추가, `assetManager` 멤버 저장 |
| `Engine/Private/RHI/Vulkan/Pipeline_Vulkan.cpp` | `LoadSpirvFile()` — `#ifdef ANDROID` 분기에 `AAssetManager_open` 로드 로직 추가 |
| `Engine/Public/Common_RHI.h` | `NativeDisplayInfo.Display` 주석 업데이트 — Android에서 `AAssetManager*` 역할 명시 |

### [Application] 수정

| 파일 | 변경 내용 |
|------|---------|
| `Application/Private/Execute.cpp` | `InitVulkan3DScene()` — Android 빌드 시 `displayInfo.Display`를 `AAssetManager*`로 캐스트하여 `pipeline->Initialize`에 전달 |

### [ExecAndroid] 수정/생성

| 파일 | 변경 내용 |
|------|---------|
| `Platforms/ExecAndroid/Source/main.cpp` | **신규** — `android_main` 진입점 (android_native_app_glue 사용) |
| `Platforms/ExecAndroid/Source/test.cpp` | 내용 비움 (stub 제거) |
| `Platforms/ExecAndroid/CMakeLists.txt` | `android_native_app_glue` 정적 라이브러리 추가, `main.cpp` 소스 포함, 링크 라이브러리 업데이트 |
| `Platforms/ExecAndroid/app/src/main/AndroidManifest.xml` | `android.app.NativeActivity`로 변경, `meta-data` 추가 |
| `Platforms/ExecAndroid/app/src/main/AndroidManifest.xml.template` | 동일 변경 |
| `Platforms/ExecAndroid/app/src/main/assets/Shaders/Vulkan/` | **신규 디렉토리** — 사전 컴파일 `.spv` 파일 배치 |

---

## 파일별 상세 설계

### 1. `Common_Vulkan.h` 추가 분기

```cpp
#ifdef ANDROID
    // Android에서 ANativeWindow와 Android Surface 확장 헤더 포함
    #include <android/native_window.h>
    #include <vulkan/vulkan_android.h>
    #define VK_PLATFORM_SURFACE_EXTENSION_NAME VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
#endif
```

`RenderDevice_Vulkan::CreateInstance()`는 이미 `VK_PLATFORM_SURFACE_EXTENSION_NAME` 매크로를 사용하므로 **변경 불필요**.

### 2. `SwapChain_Vulkan::CreateSurface()` Android 분기

```cpp
#ifdef _WIN32
    // ... 기존 Windows 코드
#elif defined(ANDROID)
    // Android Surface: ANativeWindow*를 Vulkan Surface로 변환
    // ANativeWindow = Android의 렌더 가능한 창 오브젝트 (HWND에 대응)
    VkAndroidSurfaceCreateInfoKHR surfaceInfo{};
    surfaceInfo.sType  = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.window = static_cast<ANativeWindow*>(windowHandle);
    VK_CHECK(vkCreateAndroidSurfaceKHR(instance, &surfaceInfo, nullptr, &surface));
#else
    throw std::runtime_error("[Vulkan] 이 플랫폼은 Surface 생성을 지원하지 않습니다.");
#endif
```

### 3. `Pipeline_Vulkan::Initialize()` 시그니처 변경

```cpp
// 기존: Initialize(physDev, dev, renderPass, extent, vertPath, fragPath)
// 변경: assetManager 파라미터 추가 (Android: AAssetManager*, 그 외: nullptr)
void Initialize(
    VkPhysicalDevice physicalDevice,
    VkDevice         device,
    VkRenderPass     renderPass,
    VkExtent2D       extent,
    const std::string& vertShaderPath,
    const std::string& fragShaderPath,
    void*            assetManager = nullptr
);
```

### 4. `Pipeline_Vulkan::LoadSpirvFile()` 플랫폼 분기

```cpp
static std::vector<uint8_t> LoadSpirvFile(const std::string& path, void* assetManager)
{
#ifdef ANDROID
    // APK assets 폴더에서 SPIR-V 로드
    // AAsset: Android APK 내부 파일에 대한 핸들 (Windows의 HANDLE에 대응)
    AAsset* asset = AAssetManager_open(
        static_cast<AAssetManager*>(assetManager),
        path.c_str(), AASSET_MODE_BUFFER);
    if (!asset) throw std::runtime_error("[Vulkan] 셰이더 에셋을 열 수 없습니다: " + path);

    size_t size = static_cast<size_t>(AAsset_getLength(asset));
    std::vector<uint8_t> buffer(size);
    AAsset_read(asset, buffer.data(), size);
    AAsset_close(asset);
    return buffer;
#else
    // PC: 파일 시스템에서 SPIR-V 로드 (기존 코드)
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    ...
#endif
}
```

### 5. `Execute.cpp::InitVulkan3DScene()` Android 분기

```cpp
#ifdef ANDROID
    AAssetManager* assetMgr = static_cast<AAssetManager*>(displayInfo.Display);
#else
    void* assetMgr = nullptr;
#endif

pipeline->Initialize(physDev, dev,
    vkSC->GetRenderPass(), vkSC->GetExtent(),
    "Shaders/Vulkan/mesh.vert.spv",
    "Shaders/Vulkan/mesh.frag.spv",
    assetMgr);
```

### 6. `main.cpp` (ExecAndroid 진입점)

```cpp
#include <android_native_app_glue.h>
#include "Execute.h"

static std::unique_ptr<IExecute> execEngine;

static void HandleCommand(android_app* app, int32_t cmd)
{
    switch (cmd)
    {
    case APP_CMD_INIT_WINDOW:
        // 창 준비 완료 → Execute 초기화
        // app->window = ANativeWindow*  (HWND에 대응)
        // app->activity->assetManager = AAssetManager* (셰이더 로드용)
        execEngine = std::make_unique<Execute>();
        NativeDisplayInfo info{};
        info.platform = NativeDisplayInfo::EPlatform::Android;
        info.Handle   = app->window;
        info.Display  = app->activity->assetManager;
        execEngine->OnInitialize(info);
        break;
    case APP_CMD_TERM_WINDOW:
        execEngine->OnDestroy();
        execEngine.reset();
        break;
    case APP_CMD_GAINED_FOCUS:
        if (execEngine) execEngine->OnResume();
        break;
    case APP_CMD_LOST_FOCUS:
        if (execEngine) execEngine->OnSuspend();
        break;
    }
}

// android_native_app_glue 라이브러리가 JNI 진입점을 자동 제공하고
// C++ 스레드에서 android_main을 호출합니다.
void android_main(android_app* app)
{
    app->onAppCmd = HandleCommand;

    while (!app->destroyRequested)
    {
        int events;
        android_poll_source* source;
        // execEngine이 없으면 무한 대기(-1), 있으면 즉시 반환(0)
        while (ALooper_pollAll(execEngine ? 0 : -1,
                               nullptr, &events,
                               reinterpret_cast<void**>(&source)) >= 0)
        {
            if (source) source->process(app, source);
        }
        if (execEngine) execEngine->OnLoop();
    }
}
```

### 7. `ExecAndroid/CMakeLists.txt`

```cmake
if(NOT ANDROID)
    return()
endif()

# NDK 내장 android_native_app_glue 빌드
# 이 라이브러리는 JNI OnCreate 처리 및 android_main 호출을 담당합니다.
add_library(android_native_app_glue STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)
target_include_directories(android_native_app_glue PUBLIC
    ${ANDROID_NDK}/sources/android/native_app_glue
)

file(GLOB_RECURSE EXECANDROID_SOURCES CONFIGURE_DEPENDS "Source/*.cpp")

add_library(ExecAndroid SHARED)
target_sources(ExecAndroid PRIVATE ${EXECANDROID_SOURCES})

target_link_libraries(ExecAndroid PRIVATE
    Application
    android_native_app_glue
    android       # Android OS 시스템 라이브러리 (ANativeWindow 등)
    log           # __android_log_print 로깅
)

# android_native_app_glue가 제공하는 ANativeActivity_onCreate 심볼을
# 링커가 제거하지 않도록 강제 유지합니다.
target_link_options(ExecAndroid PRIVATE
    -u ANativeActivity_onCreate
)
```

### 8. `AndroidManifest.xml`

```xml
<activity
    android:name="android.app.NativeActivity"
    android:label="@string/app_name"
    android:configChanges="orientation|keyboardHidden|screenSize"
    android:exported="true">
    <!-- lib_name: 빌드된 공유 라이브러리 이름 (libExecAndroid.so → "ExecAndroid") -->
    <meta-data
        android:name="android.app.lib_name"
        android:value="ExecAndroid"/>
    <intent-filter>
        <action android:name="android.intent.action.MAIN"/>
        <category android:name="android.intent.category.LAUNCHER"/>
    </intent-filter>
</activity>
```

### 9. 셰이더 에셋 배치

셰이더 파일: `Engine/Shaders/Vulkan/mesh.vert`, `mesh.frag`  
Windows 빌드 결과물 `.spv` 파일을 아래 위치에 복사:
```
ExecAndroid/app/src/main/assets/Shaders/Vulkan/mesh.vert.spv
ExecAndroid/app/src/main/assets/Shaders/Vulkan/mesh.frag.spv
```
Gradle이 `assets/` 디렉토리를 APK에 자동으로 패키징합니다.

---

## 변경 불필요 파일

- `Engine/Private/RHI/Vulkan/RenderDevice_Vulkan.cpp` — `VK_PLATFORM_SURFACE_EXTENSION_NAME` 매크로 사용 중, Common_Vulkan.h 수정으로 자동 해결
- `Engine/Private/RHI/Vulkan/CommandQueue_Vulkan.*` — 완전 플랫폼 중립
- `Engine/Private/RHI/Vulkan/Buffer_Vulkan.*` — 완전 플랫폼 중립
- `Engine/Private/RHI/Vulkan/Fence_Vulkan.*` — Windows `SetEvent` 블록이 이미 `#ifdef _WIN32` 로 분리됨
- `Application/Public/Execute.h` — Vulkan 블록이 이미 `#ifdef USE_VULKAN` 로 분리됨
- `Application/CMakeLists.txt` — Android도 Application 링크 사용
- `Engine/CMakeLists.txt` — Android 분기 이미 존재

---

## 검증 방법

1. Android 기기/에뮬레이터에 APK 설치
2. 앱 실행 → 회전하는 6색 큐브 출력 확인
3. 홈 버튼 → 앱 복귀 시 `OnSuspend`/`OnResume` 정상 호출 확인 (크래시 없음)
4. 뒤로 가기 → `OnDestroy` 정상 호출 및 GPU 리소스 해제 확인
5. `adb logcat` 에서 Validation Layer 경고/에러 없는지 확인