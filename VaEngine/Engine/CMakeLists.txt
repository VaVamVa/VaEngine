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

file(GLOB_RECURSE ENGINE_CORE_SOURCES
    CONFIGURE_DEPENDS
    "Private/Core/*.cpp"
)
target_sources(Engine PRIVATE
    Engine.cpp
    "Private/RHI/RHILoader.cpp"
    ${ENGINE_CORE_SOURCES}
)

# DX12 백엔드
if(USE_DX12)
    target_compile_definitions(Engine PUBLIC USE_DX12)

    include(FetchContent) # CMake 3.11 이상에서 사용할 수 있는 모듈로, 외부 라이브러리를 쉽게 가져올 수 있도록 도와줍니다.
    FetchContent_Declare(
        DirectXHeaders
        GIT_REPOSITORY https://github.com/microsoft/DirectX-Headers.git
        GIT_TAG v1.614.1
    )
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
    target_link_libraries(Engine PRIVATE d3d12 dxgi dxguid Microsoft::DirectX-Headers)
endif()

# Vulkan 백엔드
if(USE_VULKAN)
    # PUBLIC: ExecWindows·Application 같은 하위 타겟에도 USE_VULKAN 매크로가 전달됩니다.
    # Execute.cpp 에서 #ifdef USE_VULKAN 분기를 사용할 수 있게 하기 위함입니다.
    target_compile_definitions(Engine PUBLIC USE_VULKAN)

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

        # Vulkan 헤더 경로와 Vulkan RHI 구현 헤더 경로를 하위 타겟에 노출합니다.
        # ExecWindows가 Pipeline_Vulkan, Buffer_Vulkan 등을 include할 수 있습니다.
        target_include_directories(Engine PUBLIC
            ${Vulkan_INCLUDE_DIRS}
            Private/RHI/Vulkan
        )

        # glslc 경로를 CACHE에 노출하여 ExecWindows CMakeLists에서 재사용합니다.
        find_program(GLSLC_EXECUTABLE
            NAMES glslc
            HINTS "${Vulkan_INCLUDE_DIRS}/../bin" ENV VULKAN_SDK
            PATH_SUFFIXES "bin"
            CACHE STRING "glslc SPIR-V 컴파일러 경로"
        )
        # 셰이더 소스 디렉토리도 CACHE로 노출합니다.
        set(VULKAN_SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Vulkan"
            CACHE PATH "Vulkan GLSL 셰이더 소스 디렉토리" FORCE)
    endif()
endif()

target_include_directories(Engine
    PUBLIC  Public
    PRIVATE Private
)

target_precompile_headers(Engine PRIVATE pch.h)