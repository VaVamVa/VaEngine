if(NOT ANDROID)
    return()
endif()

file(GLOB_RECURSE EXECANDROID_SOURCES
    CONFIGURE_DEPENDS
    "Source/*.cpp"
)

add_library(ExecAndroid SHARED)

target_sources(ExecAndroid PRIVATE ${EXECANDROID_SOURCES})

target_link_libraries(ExecAndroid PRIVATE
    Application
    android
    log
)