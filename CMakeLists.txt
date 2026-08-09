cmake_minimum_required(VERSION 3.20)
project(SummonNightSwordcraftStory3Recomp C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GBARECOMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/gbarecomp"
    CACHE PATH "Path to the gbarecomp platform core")
set(RECOMP_UI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/recomp-ui"
    CACHE PATH "Path to recomp-ui")
set(GBARECOMP_RUNTIME_UI_ROOT "${RECOMP_UI_ROOT}"
    CACHE PATH "recomp-ui root used by the live runtime adapter")
set(GBARECOMP_GENERATED_BIOS_DIR "${CMAKE_BINARY_DIR}/generated_bios"
    CACHE PATH "Build-local private BIOS recompilation output")

if(NOT EXISTS "${GBARECOMP_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR "Initialize submodules: git submodule update --init --recursive")
endif()
if(NOT EXISTS "${RECOMP_UI_ROOT}/recomp_ui.cmake")
    message(FATAL_ERROR "recomp-ui submodule is missing")
endif()

add_subdirectory("${GBARECOMP_ROOT}" gbarecomp_build EXCLUDE_FROM_ALL)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/generated/recompiled.cpp")
    message(FATAL_ERROR
        "Stale monolithic generated/recompiled.cpp found; regenerate with current gbarecomp")
endif()

file(GLOB SWORDCRAFT3_GENERATED_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/generated/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/generated/*.cpp")

add_executable(SummonNightSwordcraftStory3Recomp
    src/main.cpp
    ${SWORDCRAFT3_GENERATED_SOURCES})

target_include_directories(SummonNightSwordcraftStory3Recomp PRIVATE
    "${GBARECOMP_ROOT}/src/armv4t"
    "${GBARECOMP_ROOT}/src/gba"
    "${GBARECOMP_ROOT}/src/runtime"
    "${GBARECOMP_ROOT}/src/debug"
    src)

target_link_libraries(SummonNightSwordcraftStory3Recomp PRIVATE
    "-Wl,--start-group"
    gbarecomp_runtime
    gbarecomp_debug
    gbarecomp_gba
    gbarecomp_armv4t
    "-Wl,--end-group")

target_compile_definitions(SummonNightSwordcraftStory3Recomp PRIVATE
    GBARECOMP_DEFAULT_DEBUG_PORT=19853
    GBARECOMP_WINDOW_TITLE="Summon Night: Swordcraft Story 3"
    GBARECOMP_DEFAULT_GAME_CONFIG="game.toml")

# Generated guest code is the dominant runtime cost. An unoptimized Debug
# build fell well below real-time even though the runtime was functioning
# correctly, so keep the game corpus optimized during developer builds while
# leaving the reusable framework's own debugging policy untouched.
if(MSVC)
    target_compile_options(SummonNightSwordcraftStory3Recomp PRIVATE
        $<$<CONFIG:Debug>:/O2>)
else()
    target_compile_options(SummonNightSwordcraftStory3Recomp PRIVATE
        $<$<CONFIG:Debug>:-O2>)
endif()

option(SWORDCRAFT3_RECOMP_UI "Build the recomp-ui pre-boot launcher" ON)
if(SWORDCRAFT3_RECOMP_UI)
    include("${RECOMP_UI_ROOT}/recomp_ui.cmake")
    add_library(swordcraft3_launcher_ui STATIC src/game_launcher_boot.cpp)
    target_include_directories(swordcraft3_launcher_ui PRIVATE
        "${GBARECOMP_ROOT}/src/runtime"
        "${GBARECOMP_ROOT}/src/gba"
        src)
    recomp_target_launcher_ui(swordcraft3_launcher_ui CONSOLE gba)
    find_package(SDL2 REQUIRED)
    target_link_libraries(swordcraft3_launcher_ui PUBLIC SDL2::SDL2)
    target_link_libraries(SummonNightSwordcraftStory3Recomp PRIVATE
        swordcraft3_launcher_ui)
    set_property(SOURCE src/main.cpp APPEND PROPERTY
        COMPILE_DEFINITIONS SWORDCRAFT3_RECOMP_UI)
endif()

gbarecomp_stage_mingw_runtime(SummonNightSwordcraftStory3Recomp)

if(MSVC)
    target_link_options(SummonNightSwordcraftStory3Recomp PRIVATE /STACK:16777216)
elseif(MINGW OR WIN32)
    target_link_options(SummonNightSwordcraftStory3Recomp PRIVATE
        -Wl,--stack,16777216)
endif()
