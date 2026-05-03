add_library(Application STATIC)

# Public 헤더 (IDE 네비게이션용)
file(GLOB_RECURSE APPLICATION_PUBLIC_HEADERS
    CONFIGURE_DEPENDS
    "Public/*.h"
)
target_sources(Application PUBLIC
    FILE_SET nav_APPLICATION_PUBLIC_HEADERS TYPE HEADERS
    BASE_DIRS Public
    FILES ${APPLICATION_PUBLIC_HEADERS}
)

# Private 소스 (Execute 등 앱 구현체)
file(GLOB_RECURSE APPLICATION_SOURCES
    CONFIGURE_DEPENDS
    "Private/*.cpp"
)
target_sources(Application PRIVATE
    Application.cpp
    ${APPLICATION_SOURCES}
)

target_include_directories(Application
    PUBLIC  Public
    PRIVATE Private
)

target_link_libraries(Application PUBLIC Engine)

target_precompile_headers(Application PRIVATE pch.h)