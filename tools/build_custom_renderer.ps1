$ErrorActionPreference = 'Stop'
$labRoot = Split-Path -Parent $PSScriptRoot
$projectRoot = [IO.Path]::GetFullPath((Join-Path $labRoot '../..'))
$cmake = Join-Path $projectRoot '.tooling/cmake/data/bin/cmake.exe'
$build = Join-Path $labRoot 'build-native'
$oldPath = $env:PATH
try {
    $env:PATH = 'C:\msys64\mingw64\bin;' + $oldPath
    $arguments = @('-S', $labRoot, '-B', $build, '-G', 'Ninja',
        '-DCMAKE_BUILD_TYPE=RelWithDebInfo',
        '-DCMAKE_C_COMPILER=C:/msys64/mingw64/bin/gcc.exe',
        '-DCMAKE_CXX_COMPILER=C:/msys64/mingw64/bin/g++.exe',
        "-DCMAKE_MAKE_PROGRAM=$projectRoot/.tooling/bin/ninja.exe",
        "-DGBARECOMP_GENERATED_BIOS_DIR=$projectRoot/build-assist/generated_bios",
        "-DSC3_LAB_GUEST_OBJECT_DIR=$projectRoot/build-beta/CMakeFiles/SummonNightSwordcraftStory3RecompBeta.dir/generated-beta",
        '-DSDL2_DIR=C:/msys64/mingw64/lib/cmake/SDL2',
        '-DSDL2_INCLUDE_DIR=C:/msys64/mingw64/include/SDL2',
        '-DSDL2_LIBRARY=C:/msys64/mingw64/lib/libSDL2.dll.a',
        '-DGBARECOMP_ENABLE_MODS=OFF')
    & $cmake @arguments
    if ($LASTEXITCODE -ne 0) { throw 'Experimental configure failed' }
    & $cmake --build $build --target Swordcraft3CustomRendererBeta gba_native_capture_tests gba_host_presentation_tests swordcraft3_lake_control_tests swordcraft3_lake_animation_tests --parallel 1
    if ($LASTEXITCODE -ne 0) { throw 'Experimental build failed' }
} finally { $env:PATH = $oldPath }
