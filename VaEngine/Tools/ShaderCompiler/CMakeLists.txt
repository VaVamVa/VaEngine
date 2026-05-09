add_executable(VaShaderCompiler Main.cpp)

if(USE_DIRECTX)
    target_compile_definitions(VaShaderCompiler PRIVATE USE_DIRECTX)
    target_link_libraries(VaShaderCompiler PRIVATE d3dcompiler)
endif()

if(USE_VULKAN)
    target_compile_definitions(VaShaderCompiler PRIVATE USE_VULKAN)
    
    include(FetchContent)
    FetchContent_Declare(
        shaderc
        GIT_REPOSITORY https://github.com/google/shaderc.git
        GIT_TAG        main
    )
    FetchContent_MakeAvailable(shaderc)
    target_link_libraries(VaShaderCompiler PRIVATE shaderc)
endif()

set_target_properties(VaShaderCompiler PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Tools"
)
