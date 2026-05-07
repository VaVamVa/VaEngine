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
    "Private/Core/*.cpp"
    "Private/Mesh/*.cpp"
    "Private/System/Time.cpp"
    "Private/System/InputSystem.cpp"
)
target_sources(Engine PRIVATE
    Engine.cpp
    "Private/RHI/RHILoader.cpp"
    ${ENGINE_SOURCES}
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

    file(GLOB_RECURSE ENGINE_AGENT_DX12_HEADERS
        CONFIGURE_DEPENDS
        "Private/RHI/Agent/DirectX/*.h"
    )
    target_sources(Engine PRIVATE
        FILE_SET nav_AGENT_DX12_HEADERS TYPE HEADERS
        BASE_DIRS Private
        FILES ${ENGINE_AGENT_DX12_HEADERS}
    )
    file(GLOB_RECURSE ENGINE_AGENT_DX12_SOURCES
        CONFIGURE_DEPENDS
        "Private/RHI/Agent/DirectX/*.cpp"
    )
    target_sources(Engine PRIVATE ${ENGINE_AGENT_DX12_SOURCES})

    FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG master
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(stb)
    target_include_directories(Engine PRIVATE ${stb_SOURCE_DIR})

    target_link_libraries(Engine PRIVATE d3d12 dxgi dxguid d3dcompiler Microsoft::DirectX-Headers)

    # 셰이더 절대 경로를 컴파일 타임 매크로로 주입 (런타임 작업 디렉토리 의존성 제거)
    set(_dx_shader_dir "${CMAKE_CURRENT_SOURCE_DIR}/Private/RHI/DirectX/_Shaders")
    target_compile_definitions(Engine PRIVATE "DX_SHADER_DIR=L\"${_dx_shader_dir}\"")
    
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

target_precompile_headers(Engine PRIVATE pch.h)