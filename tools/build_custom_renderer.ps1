param([string]$JapaneseRom = '')
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
        '-DGBARECOMP_ENABLE_MODS=OFF')
    $arguments += '-DSWORDCRAFT3_PORTABLE_JAPANESE=ON'
    if ($JapaneseRom) {
        $JapaneseRom = (Resolve-Path -LiteralPath $JapaneseRom).Path
        $arguments += "-DSWORDCRAFT3_STOCK_ROM=$JapaneseRom"
    }
    & $cmake @arguments
    if ($LASTEXITCODE -ne 0) { throw 'Experimental configure failed' }
    & $cmake --build $build --target Swordcraft3CustomRendererBeta Swordcraft3Japanese Swordcraft3BetaLauncher swordcraft3_field_frame_tests swordcraft3_combat_frame_tests gba_native_capture_tests gba_host_presentation_tests swordcraft3_lake_control_tests swordcraft3_lake_animation_tests swordcraft3_presentation_tests swordcraft3_battle_identity_tests --parallel 1
    if ($LASTEXITCODE -ne 0) { throw 'Experimental build failed' }
} finally { $env:PATH = $oldPath }
