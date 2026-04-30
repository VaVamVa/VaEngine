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