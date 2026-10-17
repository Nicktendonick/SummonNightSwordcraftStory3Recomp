param(
    [Parameter(Mandatory=$true)][string]$JapaneseRom,
    [Parameter(Mandatory=$true)][string]$EnglishRom,
    [Parameter(Mandatory=$true)][string]$Bios,
    [string]$BuildDir = 'build-portable',
    [string]$CompilerPrefix = 'C:/msys64/mingw64',
    [string]$CMake = 'cmake',
    [string]$Ninja = 'ninja',
    [string]$Python = 'python',
    [string]$Git = 'git',
    [ValidateRange(1,64)][int]$Jobs = 2
)

# Fresh private generation, never a cached-guest-object or copied-BIOS shortcut.
# No source input or existing portable is modified. The output is not published.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$target = Get-Content -LiteralPath (Join-Path $root 'packaging/portable-beta/target.json') -Raw | ConvertFrom-Json
if (-not [IO.Path]::IsPathRooted($BuildDir)) { $BuildDir = Join-Path $root $BuildDir }
$build = [IO.Path]::GetFullPath($BuildDir)
if ($build -eq [IO.Path]::GetFullPath($root)) { throw 'Use a separate build directory, not the source root.' }

function Tool([string]$Name) {
    (Get-Command $Name -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
}
function Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Command failed ($LASTEXITCODE): $Program" }
}
function VerifiedInput([string]$Name, [string]$Path, [string]$Sha1) {
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    if ((Get-FileHash -LiteralPath $resolved -Algorithm SHA1).Hash.ToLowerInvariant() -ne $Sha1) {
        throw "$Name has the wrong SHA-1; no build or patching was started."
    }
    $resolved
}
function Revisions {
    $result = [ordered]@{}
    foreach ($entry in @(@('game',$root), @('engine',(Join-Path $root 'gbarecomp')), @('ui',(Join-Path $root 'recomp-ui')))) {
        $status = & $script:gitTool -C $entry[1] status --porcelain --untracked-files=normal
        if ($LASTEXITCODE -ne 0) { throw "Cannot read source status: $($entry[0])" }
        if ($status) { throw "Commit or isolate changes before a reproducible build: $($entry[0])" }
        $head = & $script:gitTool -C $entry[1] rev-parse HEAD
        if ($LASTEXITCODE -ne 0) { throw "Cannot read source revision: $($entry[0])" }
        $result[$entry[0]] = $head.Trim()
    }
    $result
}

$oldPath = $env:PATH
try {
    $env:PATH = (Join-Path $CompilerPrefix 'bin') + ';' + $oldPath
    $cmakeTool = Tool $CMake
    $ninjaTool = Tool $Ninja
    $pythonTool = Tool $Python
    $script:gitTool = Tool $Git
    $gcc = Tool (Join-Path $CompilerPrefix 'bin/gcc.exe')
    $gxx = Tool (Join-Path $CompilerPrefix 'bin/g++.exe')
    $jp = VerifiedInput 'Japanese ROM' $JapaneseRom $target.japanese_sha1
    $en = VerifiedInput 'English ROM' $EnglishRom $target.english_sha1
    $biosPath = VerifiedInput 'GBA BIOS' $Bios $target.bios_sha1
    $before = Revisions
    $cache = Join-Path $build 'CMakeCache.txt'
    if (Test-Path -LiteralPath $cache) {
        $homeLine = Get-Content -LiteralPath $cache | Where-Object { $_.StartsWith('CMAKE_HOME_DIRECTORY:INTERNAL=') }
        if ($homeLine.Count -ne 1 -or [IO.Path]::GetFullPath($homeLine.Split('=',2)[1]) -ne [IO.Path]::GetFullPath($root)) {
            throw 'This build directory belongs to a different checkout. Choose a new directory.'
        }
    }
    $biosOut = Join-Path $build 'generated_bios'
    $configure = @('-S',$root,'-B',$build,'-G','Ninja',
        '-DCMAKE_BUILD_TYPE=Release',"-DCMAKE_C_COMPILER=$gcc", "-DCMAKE_CXX_COMPILER=$gxx",
        "-DCMAKE_MAKE_PROGRAM=$ninjaTool", "-DPython3_EXECUTABLE=$pythonTool",
        "-DCMAKE_PREFIX_PATH=$CompilerPrefix", "-DGBARECOMP_MINGW_RUNTIME_BIN=$CompilerPrefix/bin",
        '-DGBARECOMP_COMPILER_CACHE=OFF', '-DGBARECOMP_STATIC_RELEASE=OFF',
        '-DSC3_LAB_GUEST_OBJECT_DIR=', '-DSWORDCRAFT3_BETA_BPS=',
        '-DSWORDCRAFT3_RELEASE105_ROM=', '-DSWORDCRAFT3_RELEASE106_ROM=',
        '-DSWORDCRAFT3_RECOMP_UI=ON', '-DSWORDCRAFT3_PORTABLE_JAPANESE=ON',
        "-DSWORDCRAFT3_STOCK_ROM=$jp", "-$('D' + $target.cmake_rom_option)=$en",
        "-DGBARECOMP_GENERATED_BIOS_DIR=$biosOut")
    Checked $cmakeTool $configure
    Checked $cmakeTool @('--build',$build,'--target','gba_recompile','--parallel',"$Jobs")
    $recompiler = Join-Path $build 'gbarecomp_build/gba_recompile.exe'
    Checked $recompiler @('--bios',$biosPath,'--config',(Join-Path $root 'gbarecomp/bios/gba_bios.toml'),'--out',$biosOut)
    # The first configure used the intentional placeholder while building the
    # recompiler. Reconfigure now so the runtime includes the freshly generated BIOS.
    Checked $cmakeTool $configure
    Checked $cmakeTool @('--build',$build,'--target',$target.english_target,$target.japanese_target,
        $target.launcher_target,'--parallel',"$Jobs")
    $after = Revisions
    if (($before | ConvertTo-Json -Compress) -ne ($after | ConvertTo-Json -Compress)) {
        throw 'Source revision changed during the build; no provenance record was written.'
    }
    $artifacts = [ordered]@{}
    foreach ($name in @('Swordcraft Story 3 Beta.exe', "$($target.english_target).exe",
            "$($target.japanese_target).exe", 'SDL2.dll','libstdc++-6.dll','libgcc_s_seh-1.dll','libwinpthread-1.dll')) {
        $artifacts[$name] = (Get-FileHash -LiteralPath (Join-Path $build $name) -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    $record = [ordered]@{
        schema = 1
        target = $target
        revisions = $after
        build_type = 'Release'
        generated_bios = $true
        artifacts = $artifacts
        compiler = ((& $gxx --version) | Select-Object -First 1)
        cmake = ((& $cmakeTool --version) | Select-Object -First 1)
        created_utc = [DateTime]::UtcNow.ToString('o')
    }
    $record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $build 'portable-build.json') -Encoding UTF8
    Write-Output "Built verified Japanese + English $($target.english_revision) engines in $build"
    Write-Output 'Packaging and runtime acceptance are separate steps. Nothing was published.'
} finally {
    $env:PATH = $oldPath
}
