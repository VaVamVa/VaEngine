if(NOT ANDROID)
    return()
endif()

# ============================================================
# android_native_app_glue 정적 라이브러리 빌드
#
# Android NDK에 내장된 헬퍼 라이브러리입니다.
# 역할:
#   - JNI 진입점(ANativeActivity_onCreate) 자동 구현
#   - OS 이벤트를 C++ 스레드에서 처리하기 위한 루프 제공
#   - android_main() 함수를 별도 스레드에서 호출
#
# Windows의 wWinMain이 OS에 의해 호출되는 것처럼,
# android_main은 이 라이브러리에 의해 호출됩니다.
# ============================================================
add_library(android_native_app_glue STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)
target_include_directories(android_native_app_glue PUBLIC
    ${ANDROID_NDK}/sources/android/native_app_glue
)

# ============================================================
# ExecAndroid 공유 라이브러리
#
# APK에 포함되는 네이티브 C++ 코드입니다. (.so 파일)
# NativeActivity가 System.loadLibrary()로 이 라이브러리를 로드합니다.
# ============================================================
file(GLOB_RECURSE EXECANDROID_SOURCES
    CONFIGURE_DEPENDS
    "Source/*.cpp"
)

add_library(ExecAndroid SHARED)
target_sources(ExecAndroid PRIVATE ${EXECANDROID_SOURCES})

target_link_libraries(ExecAndroid PRIVATE
    Application             # Execute 클래스 및 IExecute 인터페이스
    android_native_app_glue # android_main 진입점 지원
    android                 # ANativeWindow, ALooper 등 Android 시스템 라이브러리
    log                     # __android_log_print (Android 로그 출력)
)

# android_native_app_glue가 정의하는 ANativeActivity_onCreate 심볼을
# 링커가 "사용되지 않음"으로 판단하여 제거하지 않도록 강제합니다.
# 이 심볼이 없으면 앱이 시작 시 즉시 크래시합니다.
target_link_options(ExecAndroid PRIVATE
    -u ANativeActivity_onCreate
)
