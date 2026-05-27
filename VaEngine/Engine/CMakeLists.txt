add_library(Engine STATIC)

# 공통 헤더_Public
file(GLOB_RECURSE ENGINE_HEADERS
    CONFIGURE_DEPENDS
    "Public/*.h"
)
target_sources(Engine PUBLIC
    FILE_SET nav_PUBLIC_HEADERS TYPE HEADERS # 단순 IDE 네비게이션 용도
    BASE_DIRS Public
    FILES ${ENGINE_HEADERS}
)

#공통 소스_Private
file(GLOB_RECURSE ENGINE_SOURCES
    CONFIGURE_DEPENDS
    "Private/Animation/*.cpp"
    "Private/Asset/*.cpp"
    "Private/Core/*.cpp"
    "Private/Demo/*.cpp"
    "Private/Mesh/*.cpp"
    "Private/Object/*.cpp"
    "Private/Scene/*.cpp"
    "Private/Math/*.cpp"
    "Private/Render/*.cpp"
    "Private/System/Common/*.cpp"
    "Private/Utilities/*.cpp"
)
file(GLOB ENGINE_RENDER_HEADERS
    CONFIGURE_DEPENDS
    "Private/Render/*.h"
)
target_sources(Engine PRIVATE
    Engine.cpp
    "Private/RHI/RHILoader.cpp"
    "Private/Execute.cpp"
    ${ENGINE_SOURCES}
)

include(FetchContent)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(stb)
target_include_directories(Engine PRIVATE ${stb_SOURCE_DIR})

target_sources(Engine PRIVATE
    FILE_SET nav_RENDER_HEADERS TYPE HEADERS
    BASE_DIRS Private
    FILES ${ENGINE_RENDER_HEADERS}
)

# Windows 플랫폼 입력 시스템
if(WIN32)
    file(GLOB ENGINE_WIN_SYSTEM_HEADERS
        CONFIGURE_DEPENDS
        "Private/System/Windows/*.h"
    )
    target_sources(Engine PRIVATE
        FILE_SET nav_WIN_SYSTEM_HEADERS TYPE HEADERS
        BASE_DIRS Private
        FILES ${ENGINE_WIN_SYSTEM_HEADERS}
    )
    file(GLOB ENGINE_WIN_SYSTEM_SOURCES
        CONFIGURE_DEPENDS
        "Private/System/Windows/*.cpp"
    )
    target_sources(Engine PRIVATE ${ENGINE_WIN_SYSTEM_SOURCES})
endif()

# Debug Engine 모드
if (USE_DEBUGGING)
    target_compile_definitions(Engine PRIVATE USE_DEBUGGING)

    include(FetchContent)
    FetchContent_Declare(
        DirectXHeaders
        GIT_REPOSITORY https://github.com/microsoft/DirectX-Headers.git
        GIT_TAG v1.614.1
    )
    FetchContent_MakeAvailable(DirectXHeaders) 

    file(GLOB_RECURSE ENGINE_HEADERS
        CONFIGURE_DEPENDS
        "Private/RHI/*.h"
    )
    target_sources(Engine PRIVATE
        FILE_SET nav_HEADERS TYPE HEADERS
        BASE_DIRS Private
        FILES ${ENGINE_HEADERS}
    )
    file(GLOB_RECURSE ENGINE_SOURCES
	    CONFIGURE_DEPENDS
        "Private/RHI/*.cpp"
    )
    target_sources(Engine PRIVATE ${ENGINE_SOURCES})
    target_link_libraries(Engine PRIVATE d3d12 dxgi dxguid Microsoft::DirectX-Headers)
    find_package(Vulkan REQUIRED)
        target_link_libraries(Engine PRIVATE Vulkan::Vulkan)
endif()

# DX12 백엔드
if(USE_DIRECTX)
    target_compile_definitions(Engine PRIVATE USE_DIRECTX)

    include(FetchContent) # CMake 3.11 이상에서 사용할 수 있는 모듈로, 외부 라이브러리를 쉽게 가져올 수 있도록 도와줍니다.
    FetchContent_Declare(
        DirectXHeaders
        GIT_REPOSITORY https://github.com/microsoft/DirectX-Headers.git
        GIT_TAG v1.614.1
    ) # "${SolutionRoot}\Build\windows-dx12\_deps"에 DirectX-Headers 라이브러리를 다운로드
    FetchContent_MakeAvailable(DirectXHeaders) # FetchContent로 선언된 라이브러리를 프로젝트에 추가합니다.

    file(GLOB_RECURSE ENGINE_DX12_HEADERS
        CONFIGURE_DEPENDS
        "Private/RHI/DirectX/*.h"
    )
    target_sources(Engine PRIVATE
        FILE_SET nav_DX12_HEADERS TYPE HEADERS
        BASE_DIRS Private
        FILES ${ENGINE_DX12_HEADERS}
    )
    file(GLOB_RECURSE ENGINE_DX12_SOURCES
	    CONFIGURE_DEPENDS
        "Private/RHI/DirectX/*.cpp"
    )
    target_sources(Engine PRIVATE ${ENGINE_DX12_SOURCES})

    target_link_libraries(Engine PRIVATE d3d12 dxgi dxguid d3dcompiler Microsoft::DirectX-Headers)

    # 셰이더 오프라인 컴파일 (VaShaderCompiler → .cso)
    set(_dx_shader_src "${CMAKE_CURRENT_SOURCE_DIR}/_Shaders/DirectX")
    set(_dx_shader_common "${CMAKE_CURRENT_SOURCE_DIR}/_Shaders/Common")
    set(_dx_shader_bin "${CMAKE_CURRENT_BINARY_DIR}/_Shaders")
    file(MAKE_DIRECTORY "${_dx_shader_bin}")

    # 공용 셰이더 헤더 — 모든 .hlsl 컴파일이 의존
    set(_dx_shader_common_files
        "${_dx_shader_common}/Sampler.hlsli"
        "${_dx_shader_common}/Lighting.hlsli"
        "${_dx_shader_common}/Global.hlsli"
    )

    add_custom_command(
        OUTPUT
            "${_dx_shader_bin}/ForwardOpaque_VS.cso"
            "${_dx_shader_bin}/ForwardOpaque_PS.cso"
        COMMAND $<TARGET_FILE:VaShaderCompiler>
                --target dx
                --in "${_dx_shader_src}/ForwardOpaque.hlsl"
                --stage vs --entry VSMain
                --out "${_dx_shader_bin}/ForwardOpaque_VS.cso"
        COMMAND $<TARGET_FILE:VaShaderCompiler>
                --target dx
                --in "${_dx_shader_src}/ForwardOpaque.hlsl"
                --stage ps --entry PSMain
                --out "${_dx_shader_bin}/ForwardOpaque_PS.cso"
        DEPENDS
            VaShaderCompiler
            "${_dx_shader_src}/ForwardOpaque.hlsl"
            ${_dx_shader_common_files}
        COMMENT "셰이더 컴파일: ForwardOpaque"
        VERBATIM
    )

    add_custom_command(
        OUTPUT
            "${_dx_shader_bin}/AnimationDemo_VS.cso"
            "${_dx_shader_bin}/AnimationDemo_PS.cso"
        COMMAND $<TARGET_FILE:VaShaderCompiler>
                --target dx
                --in "${_dx_shader_src}/AnimationDemo.hlsl"
                --stage vs --entry VSMain
                --out "${_dx_shader_bin}/AnimationDemo_VS.cso"
        COMMAND $<TARGET_FILE:VaShaderCompiler>
                --target dx
                --in "${_dx_shader_src}/AnimationDemo.hlsl"
                --stage ps --entry PSMain
                --out "${_dx_shader_bin}/AnimationDemo_PS.cso"
        DEPENDS
            VaShaderCompiler
            "${_dx_shader_src}/AnimationDemo.hlsl"
            ${_dx_shader_common_files}
        COMMENT "셰이더 컴파일: AnimationDemo"
        VERBATIM
    )

    add_custom_command(
        OUTPUT
            "${_dx_shader_bin}/Sky_VS.cso"
            "${_dx_shader_bin}/Sky_PS.cso"
        COMMAND $<TARGET_FILE:VaShaderCompiler>
                --target dx
                --in "${_dx_shader_src}/Sky.hlsl"
                --stage vs --entry VSMain
                --out "${_dx_shader_bin}/Sky_VS.cso"
        COMMAND $<TARGET_FILE:VaShaderCompiler>
                --target dx
                --in "${_dx_shader_src}/Sky.hlsl"
                --stage ps --entry PSMain
                --out "${_dx_shader_bin}/Sky_PS.cso"
        DEPENDS
            VaShaderCompiler
            "${_dx_shader_src}/Sky.hlsl"
            ${_dx_shader_common_files}
        COMMENT "셰이더 컴파일: Sky"
        VERBATIM
    )

    add_custom_command(
        OUTPUT
            "${_dx_shader_bin}/Glyph_VS.cso"
            "${_dx_shader_bin}/Glyph_PS.cso"
        COMMAND $<TARGET_FILE:VaShaderCompiler>
                --target dx
                --in "${_dx_shader_src}/Glyph.hlsl"
                --stage vs --entry VSMain
                --out "${_dx_shader_bin}/Glyph_VS.cso"
        COMMAND $<TARGET_FILE:VaShaderCompiler>
                --target dx
                --in "${_dx_shader_src}/Glyph.hlsl"
                --stage ps --entry PSMain
                --out "${_dx_shader_bin}/Glyph_PS.cso"
        DEPENDS
            VaShaderCompiler
            "${_dx_shader_src}/Glyph.hlsl"
            ${_dx_shader_common_files}
        COMMENT "셰이더 컴파일: Glyph"
        VERBATIM
    )

    add_custom_command(
        OUTPUT
            "${_dx_shader_bin}/DebugLine_VS.cso"
            "${_dx_shader_bin}/DebugLine_PS.cso"
        COMMAND $<TARGET_FILE:VaShaderCompiler>
                --target dx
                --in "${_dx_shader_src}/DebugLine.hlsl"
                --stage vs --entry VSMain
                --out "${_dx_shader_bin}/DebugLine_VS.cso"
        COMMAND $<TARGET_FILE:VaShaderCompiler>
                --target dx
                --in "${_dx_shader_src}/DebugLine.hlsl"
                --stage ps --entry PSMain
                --out "${_dx_shader_bin}/DebugLine_PS.cso"
        DEPENDS
            VaShaderCompiler
            "${_dx_shader_src}/DebugLine.hlsl"
            ${_dx_shader_common_files}
        COMMENT "셰이더 컴파일: DebugLine"
        VERBATIM
    )

    add_custom_command(
        OUTPUT
            "${_dx_shader_bin}/HelloCompute_CS.cso"
        COMMAND $<TARGET_FILE:VaShaderCompiler>
                --target dx
                --in "${_dx_shader_src}/HelloCompute.hlsl"
                --stage cs --entry CSMain
                --out "${_dx_shader_bin}/HelloCompute_CS.cso"
        DEPENDS
            VaShaderCompiler
            "${_dx_shader_src}/HelloCompute.hlsl"
            ${_dx_shader_common_files}
        COMMENT "셰이더 컴파일: HelloCompute (CS)"
        VERBATIM
    )

    add_custom_command(
        OUTPUT
            "${_dx_shader_bin}/BonePaletteCompute_CS.cso"
        COMMAND $<TARGET_FILE:VaShaderCompiler>
                --target dx
                --in "${_dx_shader_src}/BonePaletteCompute.hlsl"
                --stage cs --entry CSMain
                --out "${_dx_shader_bin}/BonePaletteCompute_CS.cso"
        DEPENDS
            VaShaderCompiler
            "${_dx_shader_src}/BonePaletteCompute.hlsl"
            ${_dx_shader_common_files}
        COMMENT "셰이더 컴파일: BonePaletteCompute (CS)"
        VERBATIM
    )

    add_custom_command(
        OUTPUT
            "${_dx_shader_bin}/GBuffer_VS.cso"
            "${_dx_shader_bin}/GBuffer_PS.cso"
        COMMAND $<TARGET_FILE:VaShaderCompiler>
                --target dx
                --in "${_dx_shader_src}/GBuffer.hlsl"
                --stage vs --entry VSMain
                --out "${_dx_shader_bin}/GBuffer_VS.cso"
        COMMAND $<TARGET_FILE:VaShaderCompiler>
                --target dx
                --in "${_dx_shader_src}/GBuffer.hlsl"
                --stage ps --entry PSMain
                --out "${_dx_shader_bin}/GBuffer_PS.cso"
        DEPENDS
            VaShaderCompiler
            "${_dx_shader_src}/GBuffer.hlsl"
            ${_dx_shader_common_files}
        COMMENT "셰이더 컴파일: GBuffer"
        VERBATIM
    )

    add_custom_command(
        OUTPUT
            "${_dx_shader_bin}/DeferredLighting_CS.cso"
        COMMAND $<TARGET_FILE:VaShaderCompiler>
                --target dx
                --in "${_dx_shader_src}/DeferredLighting.hlsl"
                --stage cs --entry CSMain
                --out "${_dx_shader_bin}/DeferredLighting_CS.cso"
        DEPENDS
            VaShaderCompiler
            "${_dx_shader_src}/DeferredLighting.hlsl"
            ${_dx_shader_common_files}
        COMMENT "셰이더 컴파일: DeferredLighting (CS)"
        VERBATIM
    )

    add_custom_command(
        OUTPUT
            "${_dx_shader_bin}/Blit_VS.cso"
            "${_dx_shader_bin}/Blit_PS.cso"
        COMMAND $<TARGET_FILE:VaShaderCompiler>
                --target dx
                --in "${_dx_shader_src}/Blit.hlsl"
                --stage vs --entry VSMain
                --out "${_dx_shader_bin}/Blit_VS.cso"
        COMMAND $<TARGET_FILE:VaShaderCompiler>
                --target dx
                --in "${_dx_shader_src}/Blit.hlsl"
                --stage ps --entry PSMain
                --out "${_dx_shader_bin}/Blit_PS.cso"
        DEPENDS
            VaShaderCompiler
            "${_dx_shader_src}/Blit.hlsl"
            ${_dx_shader_common_files}
        COMMENT "셰이더 컴파일: Blit"
        VERBATIM
    )

    add_custom_command(
        OUTPUT
            "${_dx_shader_bin}/GBufferSkinned_VS.cso"
            "${_dx_shader_bin}/GBufferSkinned_PS.cso"
        COMMAND $<TARGET_FILE:VaShaderCompiler>
                --target dx
                --in "${_dx_shader_src}/GBufferSkinned.hlsl"
                --stage vs --entry VSMain
                --out "${_dx_shader_bin}/GBufferSkinned_VS.cso"
        COMMAND $<TARGET_FILE:VaShaderCompiler>
                --target dx
                --in "${_dx_shader_src}/GBufferSkinned.hlsl"
                --stage ps --entry PSMain
                --out "${_dx_shader_bin}/GBufferSkinned_PS.cso"
        DEPENDS
            VaShaderCompiler
            "${_dx_shader_src}/GBufferSkinned.hlsl"
            ${_dx_shader_common_files}
        COMMENT "셰이더 컴파일: GBufferSkinned"
        VERBATIM
    )

    add_custom_target(CompileShaders DEPENDS
        "${_dx_shader_bin}/ForwardOpaque_VS.cso"
        "${_dx_shader_bin}/ForwardOpaque_PS.cso"
        "${_dx_shader_bin}/AnimationDemo_VS.cso"
        "${_dx_shader_bin}/AnimationDemo_PS.cso"
        "${_dx_shader_bin}/Sky_VS.cso"
        "${_dx_shader_bin}/Sky_PS.cso"
        "${_dx_shader_bin}/Glyph_VS.cso"
        "${_dx_shader_bin}/Glyph_PS.cso"
        "${_dx_shader_bin}/DebugLine_VS.cso"
        "${_dx_shader_bin}/DebugLine_PS.cso"
        "${_dx_shader_bin}/HelloCompute_CS.cso"
        "${_dx_shader_bin}/BonePaletteCompute_CS.cso"
        "${_dx_shader_bin}/GBuffer_VS.cso"
        "${_dx_shader_bin}/GBuffer_PS.cso"
        "${_dx_shader_bin}/DeferredLighting_CS.cso"
        "${_dx_shader_bin}/Blit_VS.cso"
        "${_dx_shader_bin}/Blit_PS.cso"
        "${_dx_shader_bin}/GBufferSkinned_VS.cso"
        "${_dx_shader_bin}/GBufferSkinned_PS.cso"
    )
    add_dependencies(Engine CompileShaders)

    # .cso 출력 경로를 컴파일 타임 매크로로 주입
    target_compile_definitions(Engine PRIVATE "SHADER_DIR=L\"${_dx_shader_bin}\"")
    
    get_target_property(_dx_includes Microsoft::DirectX-Headers INTERFACE_INCLUDE_DIRECTORIES)
    target_include_directories(Engine SYSTEM PRIVATE ${_dx_includes})
endif()

# Vulkan 백엔드
if(USE_VULKAN)
    target_compile_definitions(Engine PRIVATE USE_VULKAN)

    file(GLOB_RECURSE ENGINE_VULKAN_HEADERS
        CONFIGURE_DEPENDS
        "Private/RHI/Vulkan/*.h"
    )
    target_sources(Engine PRIVATE
        FILE_SET nav_VULKAN_HEADERS TYPE HEADERS
        BASE_DIRS Private
        FILES ${ENGINE_VULKAN_HEADERS}
    )

    file(GLOB_RECURSE ENGINE_VULKAN_SOURCES
        CONFIGURE_DEPENDS
        "Private/RHI/Vulkan/*.cpp"
    )
    target_sources(Engine PRIVATE ${ENGINE_VULKAN_SOURCES})

    if(ANDROID)
        target_link_libraries(Engine PRIVATE vulkan)
    else()
        find_package(Vulkan REQUIRED)
        target_link_libraries(Engine PRIVATE Vulkan::Vulkan)
    endif()
endif()

target_include_directories(Engine
    PUBLIC  Public
    PRIVATE Private
)

target_compile_definitions(Engine PRIVATE
    "_FILES_DIR=\"${CMAKE_SOURCE_DIR}/_Files/\""
)

target_precompile_headers(Engine PRIVATE pch.h)