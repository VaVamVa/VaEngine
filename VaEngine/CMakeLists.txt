cmake_minimum_required(VERSION 3.20)
project(VaEngine)
set(CMAKE_CXX_STANDARD 20)

if(MSVC)
    add_compile_options(/utf-8)
endif()


option(USE_DIRECTX	"Enable DirectX backend"	OFF)
option(USE_VULKAN	"Enable Vulkan backend"		OFF)
#이곳에 Graphics API 옵션 추가

add_subdirectory(Engine)
add_subdirectory(Application)
add_subdirectory(Platforms/ExecWindows)
add_subdirectory(Platforms/ExecAndroid)