param(
    [string]$BuildDir = "build-assist",
    [string]$OutputDir = "validation/adaptive-widescreen/route-audit-new-game-local",
    [int]$Start = 1200,
    [int]$End = 4400,
    [int]$Step = 12,
    [string]$Layers = "composite,bg1,bg2,obj",
    [switch]$ReuseCapture
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot

$python = Get-Command python.exe -ErrorAction SilentlyContinue
$pythonPrefix = @()
if (-not $python) {
    $python = Get-Command py.exe -ErrorAction SilentlyContinue
    if ($python) { $pythonPrefix = @("-3") }
}
if (-not $python) {
    $bundledPython = Join-Path $env:USERPROFILE ".cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe"
    if (Test-Path -LiteralPath $bundledPython) {
        $python = Get-Item -LiteralPath $bundledPython
    }
}
if (-not $python) {
    throw "Python 3 was not found. Install Python or pass the command in docs/WIDESCREEN_AUDIT.md manually."
}

$arguments = @(
    $pythonPrefix
    (Join-Path $projectRoot "tools/audit_widescreen_route.py")
    "--executable", (Join-Path $projectRoot "$BuildDir/SummonNightSwordcraftStory3Recomp.exe")
    "--rom", (Join-Path $projectRoot "roms/swordcraft3_jp.gba")
    "--bios", (Join-Path $projectRoot "gbarecomp/bios/gba_bios.bin")
    "--config", (Join-Path $projectRoot "game.toml")
    "--input-replay", (Join-Path $projectRoot "tests/input/new_game_select_male.trace")
    "--output-dir", (Join-Path $projectRoot $OutputDir)
    "--start", $Start
    "--end", $End
    "--step", $Step
    "--layers", $Layers
)
if ($ReuseCapture) { $arguments += "--reuse-capture" }

$env:SDL_VIDEODRIVER = "dummy"
$env:SDL_AUDIODRIVER = "dummy"
& $python.Source @arguments
if ($LASTEXITCODE -ne 0) {
    throw "Widescreen audit failed with exit code $LASTEXITCODE."
}
