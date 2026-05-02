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
    target_compile_definitions(Engine PRIVATE USE_DX12)

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