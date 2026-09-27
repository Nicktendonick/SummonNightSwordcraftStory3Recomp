param([string]$Destination)
$ErrorActionPreference = 'Stop'
$lab = Split-Path -Parent $PSScriptRoot
$owner = [IO.Path]::GetFullPath((Join-Path $lab '../..'))
if (!$Destination) { $Destination = Join-Path $owner 'release/Portable Beta' }
$Destination = [IO.Path]::GetFullPath($Destination)
if (!$Destination.StartsWith($owner + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Keep package output inside the project folder.'
}
if (Test-Path -LiteralPath $Destination) {
    throw 'Choose a new destination: packaging never overwrites a player installation.'
}
$build = Join-Path $lab 'build-native'
$files = @('Swordcraft3CustomRendererBeta.exe','SDL2.dll','libstdc++-6.dll','libgcc_s_seh-1.dll','libwinpthread-1.dll')
foreach ($name in $files + @('Swordcraft Story 3 Beta.exe')) {
    if (!(Test-Path -LiteralPath (Join-Path $build $name))) { throw "Missing build file: $name" }
}
$runtime = Join-Path $Destination 'Runtime'
$null = New-Item -ItemType Directory -Path $runtime -Force
Copy-Item -LiteralPath (Join-Path $build 'Swordcraft Story 3 Beta.exe') -Destination $Destination
foreach ($name in $files) { Copy-Item -LiteralPath (Join-Path $build $name) -Destination $runtime }
Copy-Item -LiteralPath (Join-Path $build 'assets') -Destination $runtime -Recurse
Copy-Item -LiteralPath (Join-Path $lab 'assets/credits') -Destination (Join-Path $Destination 'Credits') -Recurse
Copy-Item -LiteralPath (Join-Path $lab 'native-test.toml') -Destination (Join-Path $runtime 'game.toml')
Copy-Item -LiteralPath (Join-Path $lab 'docs/PORTABLE_BETA.md') -Destination (Join-Path $Destination 'README.md')
Copy-Item -LiteralPath (Join-Path $lab 'docs/PORTABLE_LAUNCHER_VALIDATION_20260924.md') -Destination $Destination
foreach ($folder in @('Settings','Saves','Save States','ROMs','BIOS','Mods','Captures','Logs')) {
    $null = New-Item -ItemType Directory -Path (Join-Path $Destination $folder)
}
# Allowlist above intentionally excludes every ROM, BIOS, patch and player save.
Write-Output "Portable package (no private game inputs or saves): $Destination"
