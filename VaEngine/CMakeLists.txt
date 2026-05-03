cmake_minimum_required(VERSION 3.20)
project(VaEngine)
set(CMAKE_CXX_STANDARD 20)

if(MSVC)
    add_compile_options(/utf-8)
endif()


option(USE_DX12		"Enable DirectX12 backend"	OFF)
option(USE_VULKAN	"Enable Vulkan backend"		OFF)

if(USE_DX12 AND NOT WIN32)
	message(FATAL_ERROR "DX12 is only supported on Windows")
endif()

add_subdirectory(Engine)
add_subdirectory(Application)
add_subdirectory(Platforms/ExecWindows)
add_subdirectory(Platforms/ExecAndroid)

# ============================================================
# android-apk 커스텀 타겟
#
# Windows 호스트에서 Gradle을 직접 호출하여 APK를 빌드합니다.
# VS 솔루션 탐색기 / Rider CMake 타겟 목록에 표시되므로
# Android Studio 없이 IDE 안에서 APK를 빌드할 수 있습니다.
#
# 사용법:
#   Visual Studio : 솔루션 탐색기 → android-apk 우클릭 → 빌드
#   Rider         : CMake 탭 → android-apk → Run
#   CLI           : cmake --build Build/windows-vulkan --target android-apk
#
# 주의: gradlew.bat이 있는 ExecAndroid 폴더를 WORKING_DIRECTORY로 사용합니다.
#       NDK가 Android SDK에 설치되어 있어야 합니다.
# ============================================================
if(WIN32)
    set(GRADLEW_CMD "${CMAKE_SOURCE_DIR}/Platforms/ExecAndroid/gradlew.bat")

    # Debug APK (arm8, x86-64 각각)
    add_custom_target(android-apk-arm8-debug
        COMMAND "${GRADLEW_CMD}" assembleArm8Debug
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/Platforms/ExecAndroid"
        COMMENT "Android APK 빌드 중 (arm8 Debug)..."
        VERBATIM
    )
    add_custom_target(android-apk-x86-64-debug
        COMMAND "${GRADLEW_CMD}" assembleX86-64Debug
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/Platforms/ExecAndroid"
        COMMENT "Android APK 빌드 중 (x86-64 Debug, 에뮬레이터용)..."
        VERBATIM
    )

    # 둘 다 빌드하는 통합 타겟
    add_custom_target(android-apk)
    add_dependencies(android-apk android-apk-arm8-debug android-apk-x86-64-debug)
endif()