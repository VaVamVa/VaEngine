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
    "${CMAKE_SOURCE_DIR}/Platforms/ExecAndroid/ndk/sources/android/native_app_glue/android_native_app_glue.c"
)
target_include_directories(android_native_app_glue PUBLIC
    "${CMAKE_SOURCE_DIR}/Platforms/ExecAndroid/ndk/sources/android/native_app_glue"
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

# ============================================================
# Vulkan GLSL → SPIR-V 셰이더 컴파일 (Android preset 전용)
#
# ExecWindows/CMakeLists.txt와 동일한 역할을 합니다.
# Android preset으로 빌드할 때 Windows-Vulkan preset 없이도
# 셰이더가 자동으로 컴파일되어 APK assets에 배치됩니다.
#
# glslc는 호스트(Windows) 도구입니다.
# find_program은 항상 호스트를 검색하므로 크로스컴파일 중에도
# Windows 호스트의 Vulkan SDK glslc가 실행됩니다.
# ============================================================
if(USE_VULKAN AND GLSLC_EXECUTABLE AND VULKAN_SHADER_SOURCE_DIR)
    message(STATUS "[VaEngine] Android Vulkan 셰이더 컴파일 설정 중...")

    set(SHADER_SOURCES
        "${VULKAN_SHADER_SOURCE_DIR}/mesh.vert"
        "${VULKAN_SHADER_SOURCE_DIR}/mesh.frag"
    )

    set(SHADER_INTERMEDIATE_DIR "${CMAKE_BINARY_DIR}/ShaderIntermediate/Vulkan")
    file(MAKE_DIRECTORY "${SHADER_INTERMEDIATE_DIR}")

    set(ANDROID_SHADER_ASSET_DIR
        "${CMAKE_SOURCE_DIR}/Platforms/ExecAndroid/app/src/main/assets/Shaders/Vulkan")

    foreach(SHADER_SRC ${SHADER_SOURCES})
        get_filename_component(SHADER_NAME "${SHADER_SRC}" NAME)
        set(SHADER_SPV "${SHADER_INTERMEDIATE_DIR}/${SHADER_NAME}.spv")

        # glslc로 GLSL → SPIR-V 컴파일
        add_custom_command(
            OUTPUT  "${SHADER_SPV}"
            COMMAND "${GLSLC_EXECUTABLE}" "${SHADER_SRC}" -o "${SHADER_SPV}"
            DEPENDS "${SHADER_SRC}"
            COMMENT "셰이더 컴파일: ${SHADER_NAME} → ${SHADER_NAME}.spv"
            VERBATIM
        )

        # 컴파일된 .spv를 Android APK assets 폴더로 복사
        add_custom_command(
            TARGET ExecAndroid POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${ANDROID_SHADER_ASSET_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SHADER_SPV}"
                "${ANDROID_SHADER_ASSET_DIR}/${SHADER_NAME}.spv"
            COMMENT "셰이더 복사 [Android]: ${SHADER_NAME}.spv → assets/Shaders/Vulkan/"
            VERBATIM
        )

        list(APPEND SHADER_SPV_FILES "${SHADER_SPV}")
    endforeach()

    add_custom_target(VulkanShaders ALL DEPENDS ${SHADER_SPV_FILES})
    add_dependencies(ExecAndroid VulkanShaders)
endif()
