add_library(Application STATIC)

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