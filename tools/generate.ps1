param(
    [string]$Rom = "roms/swordcraft3_jp.gba",
    [string]$Recompiler = "build/gbarecomp_build/gba_recompile.exe"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$romPath = Join-Path $root $Rom
$toolPath = Join-Path $root $Recompiler
$configPath = Join-Path $root "symbols/swordcraft3_jp.toml"
$symbolsPath = Join-Path $root "symbols/imported_symbols.tsv"
$outPath = Join-Path $root "generated"

# A locally built MinGW executable needs its runtime DLLs on PATH. Prefer the
# caller's configured toolchain, with the conventional MSYS2 location as a
# Windows fallback.
if (-not (Get-Command "libstdc++-6.dll" -ErrorAction SilentlyContinue)) {
    $msys2Mingw = "C:\msys64\mingw64\bin"
    if (Test-Path -LiteralPath $msys2Mingw) {
        $env:PATH = "$msys2Mingw;$env:PATH"
    }
}

if (-not (Test-Path -LiteralPath $romPath)) {
    throw "ROM not found: $romPath (see baserom.md)"
}
if (-not (Test-Path -LiteralPath $toolPath)) {
    throw "gba_recompile not found: $toolPath; build the gbarecomp submodule first"
}

& $toolPath --rom $romPath --config $configPath --symbols $symbolsPath --out $outPath
if ($LASTEXITCODE -ne 0) {
    throw "gba_recompile failed with exit code $LASTEXITCODE"
}
