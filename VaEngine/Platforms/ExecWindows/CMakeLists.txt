if(NOT WIN32)
    return()
endif()

add_executable(ExecWindows)

file(GLOB_RECURSE EXECWINDOWS_SOURCES
    CONFIGURE_DEPENDS
    "*.cpp"
    "*.rc"
)

target_sources(ExecWindows PRIVATE ${EXECWINDOWS_SOURCES})

target_link_libraries(ExecWindows PRIVATE Application)

target_precompile_headers(ExecWindows PRIVATE pch.h)

set_property(TARGET ExecWindows PROPERTY WIN32_EXECUTABLE TRUE)

# ============================================================
# Vulkan GLSL → SPIR-V 셰이더 자동 컴파일
#
# Engine CMakeLists에서 설정한 GLSLC_EXECUTABLE과
# VULKAN_SHADER_SOURCE_DIR 변수를 사용합니다.
#
# 셰이더 출력 경로: $<TARGET_FILE_DIR:ExecWindows>/Shaders/Vulkan/
# Visual Studio 멀티 설정(Debug/Release)에서도 올바른 위치에 복사됩니다.
# ============================================================
if(USE_VULKAN AND GLSLC_EXECUTABLE AND VULKAN_SHADER_SOURCE_DIR)
    message(STATUS "[VaEngine] Vulkan 셰이더 컴파일 설정 중...")

    set(SHADER_SOURCES
        "${VULKAN_SHADER_SOURCE_DIR}/mesh.vert"
        "${VULKAN_SHADER_SOURCE_DIR}/mesh.frag"
    )

    # 임시 SPIR-V 출력 경로 (빌드 디렉토리 내부)
    set(SHADER_INTERMEDIATE_DIR "${CMAKE_BINARY_DIR}/ShaderIntermediate/Vulkan")
    file(MAKE_DIRECTORY "${SHADER_INTERMEDIATE_DIR}")

    # Android assets 출력 경로
    # ExecWindows 빌드 시 .spv를 Android APK용 assets 폴더에도 자동 배포합니다.
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

        # ---- Windows 실행파일 옆으로 복사 ----
        # $<TARGET_FILE_DIR:ExecWindows>: Debug/ 또는 Release/ 경로를 자동으로 해결
        add_custom_command(
            TARGET ExecWindows POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:ExecWindows>/Shaders/Vulkan"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SHADER_SPV}"
                "$<TARGET_FILE_DIR:ExecWindows>/Shaders/Vulkan/${SHADER_NAME}.spv"
            COMMENT "셰이더 복사 [Windows]: ${SHADER_NAME}.spv → 실행파일 디렉토리"
            VERBATIM
        )

        # ---- Android assets 폴더로 복사 ----
        # Gradle이 이 경로를 APK에 자동으로 패키징합니다.
        # ExecWindows를 빌드하면 Android APK용 셰이더도 함께 갱신됩니다.
        add_custom_command(
            TARGET ExecWindows POST_BUILD
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
    add_dependencies(ExecWindows VulkanShaders)

endif()