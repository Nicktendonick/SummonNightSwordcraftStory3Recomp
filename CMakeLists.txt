cmake_minimum_required(VERSION 3.20)
project(SummonNightSwordcraftStory3Recomp C CXX)

include(CTest)

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
    src/adaptive_widescreen.cpp
    src/swordcraft3_true_map.cpp
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

if(BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME swordcraft3_widescreen_route_audit_unit
            COMMAND ${Python3_EXECUTABLE} -m unittest discover
                -s "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                -p "test_audit_widescreen_route.py" -v)
    endif()
endif()

if(MSVC)
    target_link_options(SummonNightSwordcraftStory3Recomp PRIVATE /STACK:16777216)
elseif(MINGW OR WIN32)
    target_link_options(SummonNightSwordcraftStory3Recomp PRIVATE
        -Wl,--stack,16777216)
endif()

# Optional private translation build. The user supplies the copyrighted BPS
# and stock ROM locally; both the patched ROM and generated corpus stay below
# the ignored build tree. This is a separate executable because the beta patch
# changes ARM/Thumb instructions and therefore needs its own static corpus.
set(SWORDCRAFT3_BETA_BPS "" CACHE FILEPATH
    "Local Swordcraft Story 3 English beta BPS (empty disables beta target)")
set(SWORDCRAFT3_STOCK_ROM
    "${CMAKE_CURRENT_SOURCE_DIR}/roms/swordcraft3_jp.gba" CACHE FILEPATH
    "Verified Japanese Swordcraft Story 3 ROM used for private code generation")

if(SWORDCRAFT3_BETA_BPS)
    if(NOT EXISTS "${SWORDCRAFT3_BETA_BPS}")
        message(FATAL_ERROR "SWORDCRAFT3_BETA_BPS does not exist")
    endif()
    if(NOT EXISTS "${SWORDCRAFT3_STOCK_ROM}")
        message(FATAL_ERROR "SWORDCRAFT3_STOCK_ROM does not exist")
    endif()

    add_executable(swordcraft3_rom_patch_tool EXCLUDE_FROM_ALL
        recomp-ui/tools/rom_patch_tool.c
        recomp-ui/src/common/ips_patch.c
        recomp-ui/src/common/crc32.c
        recomp-ui/src/common/sha1.c)
    target_include_directories(swordcraft3_rom_patch_tool PRIVATE
        recomp-ui/src/common)

    set(SWORDCRAFT3_BETA_DIR "${CMAKE_BINARY_DIR}/generated-beta")
    set(SWORDCRAFT3_BETA_ROM
        "${CMAKE_BINARY_DIR}/rom-patch-cache/swordcraft3_beta.gba")
    set(SWORDCRAFT3_BETA_CONFIG
        "${CMAKE_BINARY_DIR}/swordcraft3_beta_codegen.toml")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/symbols/swordcraft3_jp.toml"
         SWORDCRAFT3_BETA_CONFIG_TEXT)
    string(REPLACE
        "sha1 = \"3f5253fcf57e07ce52472bd29a61d16b98a12376\""
        "sha1 = \"bb2eebf98deb59bb6218442c2308bb5033ae2915\""
        SWORDCRAFT3_BETA_CONFIG_TEXT "${SWORDCRAFT3_BETA_CONFIG_TEXT}")
    file(WRITE "${SWORDCRAFT3_BETA_CONFIG}"
         "${SWORDCRAFT3_BETA_CONFIG_TEXT}")

    set(SWORDCRAFT3_BETA_SOURCES
        "${SWORDCRAFT3_BETA_DIR}/dispatch_table.cpp")
    foreach(SHARD RANGE 0 31)
        if(SHARD LESS 10)
            set(SHARD_PADDED "00${SHARD}")
        else()
            set(SHARD_PADDED "0${SHARD}")
        endif()
        list(APPEND SWORDCRAFT3_BETA_SOURCES
            "${SWORDCRAFT3_BETA_DIR}/recompiled_${SHARD_PADDED}.cpp")
    endforeach()
    add_custom_command(
        OUTPUT ${SWORDCRAFT3_BETA_SOURCES}
               "${SWORDCRAFT3_BETA_DIR}/recompiled.h"
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "${CMAKE_BINARY_DIR}/rom-patch-cache"
        COMMAND $<TARGET_FILE:swordcraft3_rom_patch_tool>
                "${SWORDCRAFT3_STOCK_ROM}" "${SWORDCRAFT3_BETA_BPS}"
                "${SWORDCRAFT3_BETA_ROM}"
        COMMAND $<TARGET_FILE:gba_recompile>
                --rom "${SWORDCRAFT3_BETA_ROM}"
                --config "${SWORDCRAFT3_BETA_CONFIG}"
                --symbols "${CMAKE_CURRENT_SOURCE_DIR}/symbols/imported_symbols.tsv"
                --out "${SWORDCRAFT3_BETA_DIR}"
        DEPENDS swordcraft3_rom_patch_tool gba_recompile
                "${SWORDCRAFT3_STOCK_ROM}" "${SWORDCRAFT3_BETA_BPS}"
                "${CMAKE_CURRENT_SOURCE_DIR}/symbols/swordcraft3_jp.toml"
                "${CMAKE_CURRENT_SOURCE_DIR}/symbols/imported_symbols.tsv"
        VERBATIM)
    add_custom_target(swordcraft3_beta_codegen
        DEPENDS ${SWORDCRAFT3_BETA_SOURCES}
                "${SWORDCRAFT3_BETA_DIR}/recompiled.h")

    add_executable(SummonNightSwordcraftStory3RecompBeta
        src/main.cpp src/adaptive_widescreen.cpp src/swordcraft3_true_map.cpp
        ${SWORDCRAFT3_BETA_SOURCES})
    target_include_directories(SummonNightSwordcraftStory3RecompBeta PRIVATE
        "${GBARECOMP_ROOT}/src/armv4t" "${GBARECOMP_ROOT}/src/gba"
        "${GBARECOMP_ROOT}/src/runtime" "${GBARECOMP_ROOT}/src/debug"
        "${SWORDCRAFT3_BETA_DIR}" src)
    target_link_libraries(SummonNightSwordcraftStory3RecompBeta PRIVATE
        "-Wl,--start-group" gbarecomp_runtime gbarecomp_debug gbarecomp_gba
        gbarecomp_armv4t "-Wl,--end-group")
    target_compile_definitions(SummonNightSwordcraftStory3RecompBeta PRIVATE
        SWORDCRAFT3_BETA_BUILD=1 GBARECOMP_DEFAULT_DEBUG_PORT=19854
        GBARECOMP_WINDOW_TITLE="Summon Night: Swordcraft Story 3 - English Beta"
        GBARECOMP_DEFAULT_GAME_CONFIG="game.toml")
    if(MSVC)
        target_compile_options(SummonNightSwordcraftStory3RecompBeta PRIVATE
            $<$<CONFIG:Debug>:/O2>)
    else()
        target_compile_options(SummonNightSwordcraftStory3RecompBeta PRIVATE
            $<$<CONFIG:Debug>:-O2>)
    endif()
    if(SWORDCRAFT3_RECOMP_UI)
        target_link_libraries(SummonNightSwordcraftStory3RecompBeta PRIVATE
            swordcraft3_launcher_ui)
    endif()
    gbarecomp_stage_mingw_runtime(SummonNightSwordcraftStory3RecompBeta)
    if(MSVC)
        target_link_options(SummonNightSwordcraftStory3RecompBeta PRIVATE
            /STACK:16777216)
    elseif(MINGW OR WIN32)
        target_link_options(SummonNightSwordcraftStory3RecompBeta PRIVATE
            -Wl,--stack,16777216)
    endif()
endif()
