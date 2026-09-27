param([switch]$CheckOnly, [switch]$IsolateFullCombat, [switch]$AcceptedFullCombat, [switch]$PrePerformance, [ValidateSet(240,284,320,384)][int]$HostWidth=240)
$ErrorActionPreference = 'Stop'
$labRoot = Split-Path -Parent $PSScriptRoot
$projectRoot = [IO.Path]::GetFullPath((Join-Path $labRoot '../..'))
$exe = Join-Path $labRoot 'build-native/Swordcraft3CustomRendererBeta.exe'
if ($AcceptedFullCombat) { $exe = Join-Path $labRoot 'validation/full-field-20260924/accepted/Swordcraft3CustomRendererBeta.exe' }
if ($PrePerformance) { $exe = Join-Path $labRoot 'validation/upstream-performance-20260924/before/Swordcraft3CustomRendererBeta.exe' }
$rom = Join-Path $projectRoot 'build-beta/rom-patch-cache/swordcraft3_beta.gba'
$bios = Join-Path $projectRoot 'gbarecomp/bios/gba_bios.bin'
$config = Join-Path $labRoot 'native-test.toml'
foreach ($file in @($exe,$rom,$bios,$config)) {
    if (!(Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing lab input: $file" }
}
$identity = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
Write-Host "Executable: $exe"
Write-Host "Build SHA256: $identity"
if ($CheckOnly) { Write-Host "Renderer lab paths verified ($HostWidth pixel host); nothing launched."; exit 0 }
$playtest = Join-Path $labRoot 'build-native/playtest'
if ($IsolateFullCombat) { $playtest = Join-Path $labRoot 'build-native/full-combat-playtest' }
New-Item -ItemType Directory -Path $playtest -Force | Out-Null
if ($IsolateFullCombat) {
    # The runtime locates state slots beside the ROM, not the battery file.
    # A private ROM copy therefore isolates both slot writes and battery saves.
    $privateRom = Join-Path $playtest 'swordcraft3_beta.gba'
    if (!(Test-Path -LiteralPath $privateRom)) {
        Copy-Item -LiteralPath $rom -Destination $privateRom
        for ($slot = 1; $slot -le 10; $slot++) {
            $sourceSlot = [IO.Path]::ChangeExtension($rom, ".state$slot")
            $privateSlot = [IO.Path]::ChangeExtension($privateRom, ".state$slot")
            if ((Test-Path -LiteralPath $sourceSlot) -and !(Test-Path -LiteralPath $privateSlot)) {
                Copy-Item -LiteralPath $sourceSlot -Destination $privateSlot
            }
        }
        $acceptedSave = Join-Path $projectRoot 'experiments/custom-renderer/build-native/playtest/native-renderer.eep'
        $privateSave = Join-Path $playtest 'native-renderer.eep'
        if ((Test-Path -LiteralPath $acceptedSave) -and !(Test-Path -LiteralPath $privateSave)) {
            Copy-Item -LiteralPath $acceptedSave -Destination $privateSave
        }
    }
    $rom = $privateRom
    Write-Host 'Full-frame test: battery save and ten save-state slots are isolated copies.'
}
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
        Write-Host "Custom renderer test: $HostWidth x 160 display; guest coordinates remain 240 x 160."
        if ($env:SWORDCRAFT3_FULL_FIELD_RENDERER -eq '1') {
            Write-Host 'OVERWORLD: complete-frame custom composition; no native center pasted over it.'
        } else { Write-Host 'OVERWORLD: accepted hybrid composition (native center plus custom margins).' }
        if ($env:SWORDCRAFT3_FULL_COMBAT_RENDERER -eq '1') {
            Write-Host 'COMBAT: complete-frame custom composition for supported arenas.'
        }
        Write-Host 'Supported fields: lake, village-chief outdoors, village outdoors and the captured 888x312 and 632x616 maps. Dialogue keeps native framing.'
        if ($env:SWORDCRAFT3_CUSTOM_GENERAL_FIELDS -eq '1') {
            Write-Host 'General fields ON: compatible ROM-backed maps and regular source-backed animations can widen without a room allowlist.'
            Write-Host 'Unsupported scripted animations/layer modes retain native fallback.'
        }
        if ($env:SWORDCRAFT3_CUSTOM_ADDITIONAL_AREAS -eq '0') {
            Write-Host 'Previous-area comparison: the new 888x312 and 632x616 maps and battle arenas 2 and 7 are disabled.'
        }
        if ($env:SWORDCRAFT3_CUSTOM_BATTLES -eq '1') {
            if ($env:SWORDCRAFT3_CUSTOM_ROCKY -eq '0') {
                Write-Host 'Forest combat widescreen enabled; rocky arena disabled for this comparison.'
            } else {
                Write-Host 'Reviewed combat arenas 0, 2, 3 and 7 enabled, subject to the area switches above. Unverified arenas remain native.'
            }
        } else {
            Write-Host 'Field-only test: battles use black margins.'
        }
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
