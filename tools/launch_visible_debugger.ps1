param(
    [ValidateSet("Beta", "Stock")]
    [string]$Edition = "Beta",
    [ValidateRange(240, 480)]
    [int]$ViewWidth = 284
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$session = Join-Path $root "validation/visible-debugger/$stamp-$($Edition.ToLowerInvariant())"
$captures = Join-Path $session "captures"
New-Item -ItemType Directory -Force -Path $captures | Out-Null

if ($Edition -eq "Beta") {
    $exe = Join-Path $root "build-beta/SummonNightSwordcraftStory3RecompBeta.exe"
    $rom = Join-Path $root "build-beta/rom-patch-cache/swordcraft3_beta.gba"
    $config = Join-Path $root "build-beta/swordcraft3_beta_codegen.toml"
} else {
    $exe = Join-Path $root "build-assist/SummonNightSwordcraftStory3Recomp.exe"
    $rom = Join-Path $root "roms/swordcraft3_jp.gba"
    $config = Join-Path $root "game.toml"
}
$bios = Join-Path $root "gbarecomp/bios/gba_bios.bin"
foreach ($required in @($exe, $rom, $config, $bios)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required debugger input is missing: $required"
    }
}

$env:GBARECOMP_VISIBLE_DEBUGGER = "1"
$env:GBARECOMP_DEBUG_CAPTURE_DIR = $captures
$env:GBARECOMP_INPUT_RECORD = Join-Path $session "session-input.trace"
$env:GBARECOMP_AUDIO_DUMP = Join-Path $session "delivered-audio.s16le"
$env:GBARECOMP_FRAME_PHASE = Join-Path $session "performance.csv"
$env:GBARECOMP_COVERAGE_JSON = Join-Path $session "coverage.json"
$env:GBARECOMP_MISS_FRAG = Join-Path $session "misses.toml.frag"
$env:SWORDCRAFT3_WS_DEBUG = "1"

Write-Host "Visible debugger session: $session"
Write-Host "F2 composite | F3-F6 BG0-BG3 | F7 OBJ | F8 pause | F9 step | F10 export"
& $exe --window --view-width $ViewWidth --bios $bios --rom $rom `
    --save (Join-Path $session "battery-save.eep") $config
if ($LASTEXITCODE -ne 0) {
    throw "Visible debugger exited with code $LASTEXITCODE"
}
