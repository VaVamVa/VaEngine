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

        # 컴파일된 .spv를 실행파일 옆 Shaders/Vulkan/ 디렉토리로 복사
        # $<TARGET_FILE_DIR:ExecWindows>: Debug/ 또는 Release/ 경로를 자동으로 해결
        add_custom_command(
            TARGET ExecWindows POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:ExecWindows>/Shaders/Vulkan"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SHADER_SPV}"
                "$<TARGET_FILE_DIR:ExecWindows>/Shaders/Vulkan/${SHADER_NAME}.spv"
            COMMENT "셰이더 복사: ${SHADER_NAME}.spv → 실행파일 디렉토리"
            VERBATIM
        )

        list(APPEND SHADER_SPV_FILES "${SHADER_SPV}")
    endforeach()

    add_custom_target(VulkanShaders ALL DEPENDS ${SHADER_SPV_FILES})
    add_dependencies(ExecWindows VulkanShaders)

else()
    if(USE_VULKAN AND NOT GLSLC_EXECUTABLE)
        message(WARNING "[VaEngine] glslc를 찾을 수 없습니다. Vulkan SDK($ENV{VULKAN_SDK})가 설치되었는지 확인하세요. SPIR-V 셰이더를 수동으로 빌드해야 합니다.")
    endif()
endif()