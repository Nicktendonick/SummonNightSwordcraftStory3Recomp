param(
    [string]$Bios = "gbarecomp/bios/gba_bios.bin",
    [string]$BuildDir = "build-debug",
    [string]$Recompiler = "build/gbarecomp_build/gba_recompile.exe"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$biosPath = Join-Path $root $Bios
$toolPath = Join-Path $root $Recompiler
$configPath = Join-Path $root "gbarecomp/bios/gba_bios.toml"
$outputPath = Join-Path $root "$BuildDir/generated_bios"

if (-not (Test-Path -LiteralPath $biosPath)) {
    throw "GBA BIOS not found: $biosPath"
}
if (-not (Test-Path -LiteralPath $toolPath)) {
    throw "gba_recompile not found: $toolPath"
}

$msys2Mingw = "C:\msys64\mingw64\bin"
if (Test-Path -LiteralPath $msys2Mingw) {
    $env:PATH = "$msys2Mingw;$env:PATH"
}

& $toolPath --bios $biosPath --config $configPath --out $outputPath
if ($LASTEXITCODE -ne 0) {
    throw "BIOS recompilation failed with exit code $LASTEXITCODE"
}
