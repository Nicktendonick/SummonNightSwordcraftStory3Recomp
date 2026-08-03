param(
    [string]$BuildDir = "build-debug",
    [int]$Frames = 4400,
    [switch]$CaptureFrame
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$exePath = Join-Path $root "$BuildDir/SummonNightSwordcraftStory3Recomp.exe"
$biosPath = Join-Path $root "gbarecomp/bios/gba_bios.bin"
$romPath = Join-Path $root "roms/swordcraft3_jp.gba"
$configPath = Join-Path $root "game.toml"
$tracePath = Join-Path $root "tests/input/new_game_select_male.trace"
$validationPath = Join-Path $root "validation"
$savePath = Join-Path $validationPath "new_game_validation.eep"
$coveragePath = Join-Path $root "recomp_coverage_B3CJ.json"

foreach ($required in @($exePath, $biosPath, $romPath, $configPath, $tracePath)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Required validation input not found: $required"
    }
}

New-Item -ItemType Directory -Force -Path $validationPath | Out-Null
$msys2Mingw = "C:\msys64\mingw64\bin"
if (Test-Path -LiteralPath $msys2Mingw) {
    $env:PATH = "$msys2Mingw;$env:PATH"
}

$previousStrict = $env:GBARECOMP_STRICT_STATIC
$previousReplay = $env:GBARECOMP_INPUT_REPLAY
$previousSelfHeal = $env:GBARECOMP_SELFHEAL_RECOMPILE

try {
    $env:GBARECOMP_STRICT_STATIC = "1"
    $env:GBARECOMP_INPUT_REPLAY = $tracePath
    Remove-Item Env:GBARECOMP_SELFHEAL_RECOMPILE -ErrorAction SilentlyContinue

    $arguments = @(
        "--no-window",
        "--frames", $Frames,
        "--save", $savePath,
        "--bios", $biosPath,
        "--rom", $romPath
    )
    if ($CaptureFrame) {
        $arguments += @("--dump-png", (Join-Path $validationPath "new_game_validation.png"))
    }
    $arguments += $configPath

    Push-Location $root
    try {
        & $exePath @arguments
        if ($LASTEXITCODE -ne 0) {
            throw "Strict-static validation failed with exit code $LASTEXITCODE"
        }
    }
    finally {
        Pop-Location
    }

    if (-not (Test-Path -LiteralPath $coveragePath)) {
        throw "Runtime did not write the expected coverage report: $coveragePath"
    }
    $coverage = Get-Content -Raw -LiteralPath $coveragePath | ConvertFrom-Json
    if ($coverage.coverage -ne "FULLY_STATIC" -or
        $coverage.distinct_misses -ne 0 -or
        $coverage.interpreted_insns -ne 0) {
        throw "Coverage report did not pass strict-static acceptance"
    }

    Write-Host "PASS: $Frames-frame new-game trace is FULLY_STATIC"
}
finally {
    if ($null -eq $previousStrict) {
        Remove-Item Env:GBARECOMP_STRICT_STATIC -ErrorAction SilentlyContinue
    } else {
        $env:GBARECOMP_STRICT_STATIC = $previousStrict
    }
    if ($null -eq $previousReplay) {
        Remove-Item Env:GBARECOMP_INPUT_REPLAY -ErrorAction SilentlyContinue
    } else {
        $env:GBARECOMP_INPUT_REPLAY = $previousReplay
    }
    if ($null -eq $previousSelfHeal) {
        Remove-Item Env:GBARECOMP_SELFHEAL_RECOMPILE -ErrorAction SilentlyContinue
    } else {
        $env:GBARECOMP_SELFHEAL_RECOMPILE = $previousSelfHeal
    }
}
