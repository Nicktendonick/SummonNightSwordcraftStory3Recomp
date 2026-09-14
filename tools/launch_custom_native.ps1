param([switch]$CheckOnly, [ValidateSet(240,284,320,384)][int]$HostWidth=240)
$ErrorActionPreference = 'Stop'
$labRoot = Split-Path -Parent $PSScriptRoot
$projectRoot = [IO.Path]::GetFullPath((Join-Path $labRoot '../..'))
$exe = Join-Path $labRoot 'build-native/Swordcraft3CustomRendererBeta.exe'
$rom = Join-Path $projectRoot 'build-beta/rom-patch-cache/swordcraft3_beta.gba'
$bios = Join-Path $projectRoot 'gbarecomp/bios/gba_bios.bin'
$config = Join-Path $labRoot 'native-test.toml'
foreach ($file in @($exe,$rom,$bios,$config)) {
    if (!(Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing lab input: $file" }
}
if ($CheckOnly) { Write-Host "Renderer lab paths verified ($HostWidth pixel host); nothing launched."; exit 0 }
$playtest = Join-Path $labRoot 'build-native/playtest'
New-Item -ItemType Directory -Path $playtest -Force | Out-Null
$saved = @{}
foreach ($key in @('PATH','SWORDCRAFT3_CUSTOM_RENDERER','SWORDCRAFT3_CUSTOM_HOST_WIDTH','SWORDCRAFT3_LAKE_EDGE_DATA','SWORDCRAFT3_LAKE_CAMERA_LIMITS',
                  'GBARECOMP_VISIBLE_DEBUGGER','GBARECOMP_DEBUG_CAPTURE_DIR','GBARECOMP_INPUT_RECORD',
                  'GBARECOMP_WS_WIP','GBARECOMP_VIEW_WIDTH','GBARECOMP_WIDESCREEN','GBARECOMP_RESIZE_VIEW')) {
    $saved[$key] = [Environment]::GetEnvironmentVariable($key,'Process')
}
try {
    $env:PATH = 'C:\msys64\mingw64\bin;' + $env:PATH
    $env:SWORDCRAFT3_CUSTOM_RENDERER = '1'
    $env:SWORDCRAFT3_CUSTOM_HOST_WIDTH = "$HostWidth"
    $env:SWORDCRAFT3_LAKE_EDGE_DATA = $null
    $env:SWORDCRAFT3_LAKE_CAMERA_LIMITS = $null
    # Do not inherit a previous renderer/debug experiment into this lab run.
    $env:GBARECOMP_WS_WIP = $null
    $env:GBARECOMP_VIEW_WIDTH = $null
    $env:GBARECOMP_WIDESCREEN = $null
    $env:GBARECOMP_RESIZE_VIEW = $null
    $env:GBARECOMP_VISIBLE_DEBUGGER = $null
    $env:GBARECOMP_DEBUG_CAPTURE_DIR = $null
    $env:GBARECOMP_INPUT_RECORD = $null
    if ($HostWidth -gt 240) {
        $captures = Join-Path $labRoot ('validation/playtest-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
        New-Item -ItemType Directory -Path $captures | Out-Null
        $env:GBARECOMP_VISIBLE_DEBUGGER = '1'
        $env:GBARECOMP_DEBUG_CAPTURE_DIR = $captures
        $env:GBARECOMP_INPUT_RECORD = Join-Path $captures 'session-input.trace'
        Write-Host "Field scenery test: $HostWidth pixel display; the game still renders at 240x160."
        Write-Host 'Lake, village-chief outdoors and village outdoors include extended regular sprites. Dialogue/battles use black margins.'
        Write-Host "Press F10 to capture an issue. Captures: $captures"
    } else {
        Write-Host 'Native renderer lab: intentional 240x160 correctness test.'
    }
    Write-Host 'Uses a separate battery save. Close it and launch normally to return to the existing build.'
    Push-Location -LiteralPath $playtest
    try {
        & $exe --window --launcher --bios $bios --rom $rom --view-width 240 --save (Join-Path $playtest 'native-renderer.eep') $config
        if ($LASTEXITCODE -ne 0) { throw "Native test exited with $LASTEXITCODE" }
    } finally { Pop-Location }
} finally {
    foreach ($key in $saved.Keys) { [Environment]::SetEnvironmentVariable($key,$saved[$key],'Process') }
}
