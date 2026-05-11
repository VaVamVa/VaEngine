cmake_minimum_required(VERSION 3.20)

include(FetchContent)
FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG        v5.3.1
)
set(BUILD_SHARED_LIBS                     OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS                    OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL                        OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_FBX_IMPORTER             ON  CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_OBJ_IMPORTER             ON  CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT                      ON  CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(assimp)

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(stb)

add_executable(VaImportTool
    Main.cpp
    Converter.cpp
    Exporter.cpp
    SkmExporter.cpp
    ClipExporter.cpp
)

target_include_directories(VaImportTool PRIVATE
    ${CMAKE_SOURCE_DIR}/Engine/Public
    ${stb_SOURCE_DIR}
)

target_link_libraries(VaImportTool PRIVATE assimp)

set(_asset_input  "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Input")
set(_asset_output "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Output")
file(MAKE_DIRECTORY "${_asset_input}")
file(MAKE_DIRECTORY "${_asset_output}")

target_compile_definitions(VaImportTool PRIVATE
    ASSET_INPUT_DIR="${_asset_input}"
    ASSET_OUTPUT_DIR="${_asset_output}"
)

set_target_properties(VaImportTool PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Tools"
)
