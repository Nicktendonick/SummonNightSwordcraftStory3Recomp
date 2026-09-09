param(
    [ValidateSet("Beta", "Stock")]
    [string]$Edition = "Beta",
    [ValidateRange(240, 384)]
    [int]$ViewWidth = 284,
    [switch]$ShowLauncher,
    # Return the launch plan without opening windows, making directories,
    # or changing environment variables (used by regression tests).
    [switch]$PlanOnly
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$stamp = Get-Date -Format "yyyyMMdd-HHmmss-fff"
$session = Join-Path $root "validation/visible-debugger/$stamp-$($Edition.ToLowerInvariant())"
$captures = Join-Path $session "captures"

if ($Edition -eq "Beta") {
    $exe = Join-Path $root "build-beta/SummonNightSwordcraftStory3RecompBeta.exe"
    $rom = Join-Path $root "build-beta/rom-patch-cache/swordcraft3_beta.gba"
    $config = Join-Path $root "build-beta/swordcraft3_beta_codegen.toml"
} else {
    $exe = Join-Path $root "build-beta/SummonNightSwordcraftStory3Recomp.exe"
    $rom = Join-Path $root "roms/swordcraft3_jp.gba"
    $config = Join-Path $root "game.toml"
}
$bios = Join-Path $root "gbarecomp/bios/gba_bios.bin"
$requiredInputs = @($exe, $config, $bios)
if (-not $ShowLauncher) { $requiredInputs += $rom }
foreach ($required in $requiredInputs) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required debugger input is missing: $required"
    }
}

$captureEnvironment = [ordered]@{
    GBARECOMP_VISIBLE_DEBUGGER = "1"
    GBARECOMP_DEBUG_CAPTURE_DIR = $captures
    GBARECOMP_INPUT_RECORD = Join-Path $session "session-input.trace"
    GBARECOMP_AUDIO_DUMP = Join-Path $session "delivered-audio.s16le"
    GBARECOMP_FRAME_PHASE = Join-Path $session "performance.csv"
    GBARECOMP_COVERAGE_JSON = Join-Path $session "coverage.json"
    GBARECOMP_MISS_FRAG = Join-Path $session "misses.toml.frag"
    SWORDCRAFT3_WS_DEBUG = "1"
}
$gameArguments = @("--window", "--bios", $bios,
                   "--save", (Join-Path $session "battery-save.eep"))
$clearEnvironment = @()
if ($ShowLauncher) {
    # Explicit --rom is a deliberate headless/automation bypass in the shared
    # launcher seam. Let the launcher select/verify the ROM and translation,
    # and append its chosen Display/Assist settings without a fixed width.
    $gameArguments += "--launcher"
    $clearEnvironment = @("GBARECOMP_VIEW_WIDTH", "GBARECOMP_WIDESCREEN", "GBARECOMP_RESIZE_VIEW")
} else {
    $gameArguments += @("--no-launcher", "--view-width", "$ViewWidth", "--rom", $rom)
}
$gameArguments += $config
$plan = [ordered]@{
    executable = $exe
    arguments = $gameArguments
    session = $session
    environment = $captureEnvironment
    clear_environment = $clearEnvironment
    show_launcher = [bool]$ShowLauncher
}
if ($PlanOnly) { $plan | ConvertTo-Json -Depth 4; return }

if ($ShowLauncher -and $PSBoundParameters.ContainsKey("ViewWidth")) {
    Write-Host "ViewWidth is for direct launch; choose the width in Display for this session."
}
New-Item -ItemType Directory -Path $captures -Force | Out-Null
$plan | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $session "launch-plan.json") -Encoding UTF8

Write-Host "Visible debugger session: $session"
Write-Host "F2 composite | F3-F6 BG0-BG3 | F7 OBJ | F8 pause | F9 step | F10 export"
if ($ShowLauncher) {
    Write-Host "Choose Display / aspect ratio and other options in the launcher, then press PLAY."
    Write-Host "Adaptive follows window resizing; Full Arena is the fixed 384-pixel option."
}
$savedEnvironment = @{}
try {
    foreach ($key in @($captureEnvironment.Keys) + $clearEnvironment) {
        $savedEnvironment[$key] = [Environment]::GetEnvironmentVariable($key, "Process")
    }
    foreach ($key in $clearEnvironment) {
        [Environment]::SetEnvironmentVariable($key, $null, "Process")
    }
    foreach ($key in $captureEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($key, $captureEnvironment[$key], "Process")
    }
    Push-Location -LiteralPath $root
    try {
        & $exe @gameArguments
        if ($LASTEXITCODE -ne 0) { throw "Visible debugger exited with code $LASTEXITCODE" }
    } finally { Pop-Location }
} finally {
    foreach ($key in $savedEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($key, $savedEnvironment[$key], "Process")
    }
}
