param()
$ErrorActionPreference = 'Stop'
$labRoot = Split-Path -Parent $PSScriptRoot
$ownerRoot = [IO.Path]::GetFullPath((Join-Path $labRoot '../..'))
$pin = Get-Content -LiteralPath (Join-Path $labRoot 'packaging/sdl2.json') -Raw | ConvertFrom-Json
$folder = Join-Path $ownerRoot ('.tooling/sdl2-' + $pin.version)
$archive = Join-Path $folder $pin.archive
New-Item -ItemType Directory -Path $folder -Force | Out-Null
if (-not (Test-Path -LiteralPath $archive)) {
    Invoke-WebRequest -Uri $pin.url -OutFile $archive
}
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $pin.archive_sha256) {
    throw 'SDL2 archive checksum mismatch; no extraction performed.'
}
$sdk = Join-Path $folder $pin.directory
if (-not (Test-Path -LiteralPath $sdk)) {
    Expand-Archive -LiteralPath $archive -DestinationPath $folder
}
$dll = Join-Path $sdk 'x86_64-w64-mingw32/bin/SDL2.dll'
if ((Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash -ne $pin.dll_sha256) {
    throw 'SDL2 SDK DLL checksum mismatch; existing files were not overwritten.'
}
Write-Output ('Verified SDL2 ' + $pin.version + ': ' + $sdk)
