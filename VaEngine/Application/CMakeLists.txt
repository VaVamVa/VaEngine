add_library(Application STATIC)

file(GLOB_RECURSE APPLICATION_HEADERS
    CONFIGURE_DEPENDS
    "Source/Public/*.h"
)

target_sources(Application PUBLIC
    FILE_SET nav_APPLICATION_HEADERS TYPE HEADERS
    BASE_DIRS Source/Public
    FILES ${APPLICATION_HEADERS}
)

file(GLOB_RECURSE APPLICATION_SOURCES
    CONFIGURE_DEPENDS
    "Source/Private/*.cpp"
)

target_sources(Application PRIVATE
    Application.cpp
    ${APPLICATION_SOURCES}
)

target_include_directories(Application
    PUBLIC  Source/Public
    PRIVATE Source/Private
)

target_link_libraries(Application PUBLIC Engine)

target_precompile_headers(Application PRIVATE pch.h)