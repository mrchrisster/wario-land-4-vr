cmake_minimum_required(VERSION 3.20)
project(WarioLand4Recomp C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Shares the Quest-VR gbarecomp checkout (layer view, STOCK 3D) with Mother3Recomp.
if(ANDROID)
    set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)
endif()
# Submodules in external/ (branch wl4-vr) when present, else the sibling Mother3Recomp checkout.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/gbarecomp/CMakeLists.txt")
    set(_wl4_gba_default "${CMAKE_CURRENT_SOURCE_DIR}/external/gbarecomp")
    set(_wl4_ui_default "${CMAKE_CURRENT_SOURCE_DIR}/external/recomp-ui")
else()
    set(_wl4_gba_default "${CMAKE_CURRENT_SOURCE_DIR}/../Mother3Recomp/gbarecomp")
    set(_wl4_ui_default "${CMAKE_CURRENT_SOURCE_DIR}/../Mother3Recomp/recomp-ui")
endif()
set(GBARECOMP_ROOT "${_wl4_gba_default}" CACHE PATH "Path to the gbarecomp checkout")
set(RECOMP_UI_ROOT "${_wl4_ui_default}" CACHE PATH "recomp-ui checkout")
set(GBARECOMP_RUNTIME_UI_ROOT "${RECOMP_UI_ROOT}" CACHE PATH "recomp-ui root for the runtime")
add_subdirectory("${GBARECOMP_ROOT}" gbarecomp_build EXCLUDE_FROM_ALL)
set(VARIANT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/variants/wario_land_4")
file(GLOB VARIANT_GENERATED CONFIGURE_DEPENDS "${VARIANT_DIR}/generated/*.cpp")
if(ANDROID)
    set(WL4_TARGET main)
    add_library(main SHARED src/main.cpp ${VARIANT_GENERATED})
else()
    set(WL4_TARGET WarioLand4Recomp)
    add_executable(WarioLand4Recomp src/main.cpp ${VARIANT_GENERATED})
endif()
set(SHARDS ${VARIANT_GENERATED})
list(FILTER SHARDS INCLUDE REGEX "/recompiled_[0-9]+\\.cpp$")
set_source_files_properties(${SHARDS} PROPERTIES COMPILE_OPTIONS "-O2;-g0")
target_include_directories(${WL4_TARGET} PRIVATE
    "${GBARECOMP_ROOT}/src/armv4t" "${GBARECOMP_ROOT}/src/gba"
    "${GBARECOMP_ROOT}/src/runtime" "${GBARECOMP_ROOT}/src/debug" src)
target_link_libraries(${WL4_TARGET} PRIVATE
    gbarecomp_runtime gbarecomp_debug gbarecomp_gba gbarecomp_armv4t)
target_compile_definitions(${WL4_TARGET} PRIVATE
    GBARECOMP_DEFAULT_DEBUG_PORT=19880
    GBARECOMP_WINDOW_TITLE="WarioLand4Recomp"
    GBARECOMP_DEFAULT_GAME_CONFIG="variants/wario_land_4/game.toml"
    GBARECOMP_BUILTIN_NAME="Wario Land 4"
    GBARECOMP_BUILTIN_SHA1="b9fe05a8080e124b67bce6a623234ee3b518a2c1"
    GBARECOMP_BUILTIN_CRC32=0xd6141609
    GBARECOMP_BUILTIN_REGION="USA")

include("${RECOMP_UI_ROOT}/recomp_ui.cmake")
add_library(game_launcher_ui STATIC src/game_launcher_boot.cpp)
target_include_directories(game_launcher_ui PRIVATE
    "${GBARECOMP_ROOT}/src/runtime" "${GBARECOMP_ROOT}/src/gba" src)
recomp_target_launcher_ui(game_launcher_ui CONSOLE gba)
if(ANDROID)
    set_target_properties(game_launcher_ui PROPERTIES POSITION_INDEPENDENT_CODE ON)
else()
    find_package(SDL2 REQUIRED)
endif()
target_link_libraries(game_launcher_ui PUBLIC SDL2::SDL2)
target_link_libraries(${WL4_TARGET} PRIVATE game_launcher_ui)
set_property(SOURCE src/main.cpp APPEND PROPERTY COMPILE_DEFINITIONS GBAGAME_RECOMP_UI)
