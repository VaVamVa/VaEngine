add_library(Engine STATIC)

# 공통 헤더_Public
file(GLOB_RECURSE ENGINE_PUBLIC_HEADERS
    CONFIGURE_DEPENDS
    "Public/*.h"
)
target_sources(Engine PUBLIC
    FILE_SET HEADERS
    BASE_DIRS Public
    FILES ${ENGINE_PUBLIC_HEADERS}
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
    file(GLOB_RECURSE ENGINE_DX12_SOURCES
	    CONFIGURE_DEPENDS
            "Private/RHI/DirectX/*.cpp"
    )
    target_sources(Engine PRIVATE ${ENGINE_DX12_SOURCES})
    target_link_libraries(Engine PRIVATE d3d12 dxgi dxguid)
    target_include_directories(Engine PRIVATE
        "${CMAKE_SOURCE_DIR}/ThirdParty/DirectX-Headers/include/directx"
    )
endif()

# Vulkan 백엔드
if(USE_VULKAN)
    target_compile_definitions(Engine PRIVATE USE_VULKAN)
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