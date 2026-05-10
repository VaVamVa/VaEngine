add_library(Application STATIC)

# Core 공통 유틸리티 헤더
file(GLOB_RECURSE APPLICATION_CORE_HEADERS
    CONFIGURE_DEPENDS
    "Core/Public/*.h"
)
target_sources(Application PUBLIC
    FILE_SET nav_CORE_HEADERS TYPE HEADERS
    BASE_DIRS Core/Public
    FILES ${APPLICATION_CORE_HEADERS}
)

# Core 공통 유틸리티 소스
file(GLOB_RECURSE APPLICATION_CORE_SOURCES
    CONFIGURE_DEPENDS
    "Core/Private/**/*.cpp"
)

# Source 헤더
file(GLOB_RECURSE APPLICATION_HEADERS
    CONFIGURE_DEPENDS
    "Source/Public/*.h"
)
target_sources(Application PUBLIC
    FILE_SET nav_APPLICATION_HEADERS TYPE HEADERS
    BASE_DIRS Source/Public
    FILES ${APPLICATION_HEADERS}
)

# Source 소스
file(GLOB_RECURSE APPLICATION_SOURCES
    CONFIGURE_DEPENDS
    "Source/Private/**/*.cpp"
    "Source/Private/VaProgramName.cpp"
)

target_sources(Application PRIVATE
    Application.cpp
    ${APPLICATION_CORE_SOURCES}
    ${APPLICATION_SOURCES}
)

target_include_directories(Application
    PUBLIC  Core/Public Source/Public
    PRIVATE Core/Private Source/Private
)

target_link_libraries(Application PUBLIC Engine)

target_compile_definitions(Application PRIVATE
    ASSETS_DIR="${CMAKE_SOURCE_DIR}/_Assets/"
)

target_precompile_headers(Application PRIVATE pch.h)