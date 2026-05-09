cmake_minimum_required(VERSION 3.20)
project(VaEngine)
set(CMAKE_CXX_STANDARD 20)

if(MSVC)
    add_compile_options(/utf-8)
endif()


option(USE_DIRECTX     "Enable DirectX backend"  OFF)
option(USE_VULKAN      "Enable Vulkan backend"   OFF)
option(USE_IMPORT_TOOL "Build asset import tool" OFF)
#이곳에 Graphics API 옵션 추가

if(USE_IMPORT_TOOL)
    add_subdirectory(Tools/ImportTool)
else()
    if(USE_DIRECTX OR USE_VULKAN)
        add_subdirectory(Tools/ShaderCompiler)
    endif()
    add_subdirectory(Engine)
    add_subdirectory(Application)
    add_subdirectory(Platforms/ExecWindows)
    add_subdirectory(Platforms/ExecAndroid)
endif()