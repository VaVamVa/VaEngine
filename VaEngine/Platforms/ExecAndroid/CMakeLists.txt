if(NOT ANDROID)
    return()
endif()

add_library(ExecAndroid SHARED)

file(GLOB_RECURSE EXECANDROID_SOURCES
    CONFIGURE_DEPENDS
    "*.cpp"
)

target_sources(ExecAndroid PRIVATE ${EXECANDROID_SOURCES})

target_link_libraries(ExecAndroid PRIVATE
    Application
    android
    log
)