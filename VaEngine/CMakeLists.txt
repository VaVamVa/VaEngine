cmake_minimum_required(VERSION 3.20)
project(VaEngine)
set(CMAKE_CXX_STANDARD 20)

if(MSVC)
    add_compile_options(/utf-8)
endif()


option(USE_DX12		"Enable DirectX12 backend"	OFF)
option(USE_VULKAN	"Enable Vulkan backend"		OFF)

if(USE_DX12 AND NOT WIN32)
	message(FATAL_ERROR "DX12 is only supported on Windows")
endif()

add_subdirectory(Engine)
add_subdirectory(Application)
add_subdirectory(Platforms/ExecWindows)
add_subdirectory(Platforms/ExecAndroid)