$ErrorActionPreference = 'Stop'
$lab = Split-Path -Parent $PSScriptRoot
$owner = [IO.Path]::GetFullPath((Join-Path $lab '../..'))
$build = Join-Path $lab 'build-native'
$cmake = Join-Path $owner '.tooling/cmake/data/bin/cmake.exe'
if (!(Test-Path -LiteralPath (Join-Path $build 'CMakeCache.txt'))) {
    throw 'Prepare the custom-renderer build first with tools/build_custom_renderer.ps1.'
}
$destination = Join-Path $owner 'Swordcraft Story 3 Beta.exe'
if (Test-Path -LiteralPath $destination) {
    if ((Get-Item -LiteralPath $destination).VersionInfo.ProductName -ne 'Summon Night: Swordcraft Story 3 PC Beta') {
        throw 'Destination is not our beta launcher; refusing to replace it.'
    }
}
$priorPath = $env:PATH
try {
    $env:PATH = 'C:/msys64/mingw64/bin;' + $priorPath
    & $cmake -S $lab -B $build -DCMAKE_RC_COMPILER=C:/msys64/mingw64/bin/windres.exe
    if ($LASTEXITCODE -ne 0) { throw 'Beta launcher configuration failed.' }
    & $cmake --build $build --target Swordcraft3BetaLauncher Swordcraft3CustomRendererBeta --parallel 1
    if ($LASTEXITCODE -ne 0) { throw 'Beta launcher build failed.' }
    Copy-Item -LiteralPath (Join-Path $build 'Swordcraft Story 3 Beta.exe') -Destination $destination
    $credits = Join-Path $owner 'Credits'
    $null = New-Item -ItemType Directory -Path $credits -Force
    foreach ($name in @('original-game.txt','pc-port.txt','tools-and-projects.txt')) {
        $target = Join-Path $credits $name
        if (!(Test-Path -LiteralPath $target)) {
            Copy-Item -LiteralPath (Join-Path $lab "assets/credits/$name") -Destination $target
        }
    }
    Write-Host "Ready: $destination"
} finally { $env:PATH = $priorPath }
