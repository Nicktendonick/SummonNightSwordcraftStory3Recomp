param([Parameter(Mandatory=$true)][string]$Archive,
      [Parameter(Mandatory=$true)][string]$Destination,
      [string]$Python = 'python')
$ErrorActionPreference = 'Stop'
$lab = Split-Path -Parent $PSScriptRoot
$Destination = [IO.Path]::GetFullPath($Destination)
$validation = [IO.Path]::GetFullPath((Join-Path $lab 'validation'))
if (!$Destination.StartsWith($validation + [IO.Path]::DirectorySeparatorChar,
                            [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Use a new disposable directory inside worktree validation.'
}
if (Test-Path -LiteralPath $Destination) { throw 'First-run test never reuses a directory.' }
& $Python (Join-Path $PSScriptRoot 'package_portable_release.py') --verify $Archive
if ($LASTEXITCODE -ne 0) { throw 'Archive validation failed.' }
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::ExtractToDirectory($Archive, $Destination)
$package = Join-Path $Destination 'Swordcraft Story 3 Portable Beta'
$launcher = Join-Path $package 'Swordcraft Story 3 Beta.exe'
$before = @{}
Get-ChildItem -LiteralPath $package -Recurse -File | ForEach-Object {
    $before[$_.FullName] = (Get-FileHash -LiteralPath $_.FullName).Hash
}
$oldPath = $env:PATH
$oldScript = $env:LNG_SCRIPT
try {
    $env:PATH = "$env:SystemRoot/System32;$env:SystemRoot"
    $preflight = Start-Process -FilePath $launcher -ArgumentList '--check' -WorkingDirectory $env:SystemRoot -WindowStyle Hidden -PassThru
    if (!$preflight.WaitForExit(10000) -or $preflight.ExitCode -ne 0) { throw 'Clean preflight failed.' }
    $after = @(Get-ChildItem -LiteralPath $package -Recurse -File)
    if ($after.Count -ne $before.Count) { throw 'Read-only preflight created files.' }
    foreach ($file in $before.Keys) {
        if ((Get-FileHash -LiteralPath $file).Hash -ne $before[$file]) { throw 'Preflight modified payload.' }
    }
    $env:LNG_SCRIPT = 'wait:20;quit'
    $opened = Start-Process -FilePath $launcher -WorkingDirectory $env:SystemRoot -WindowStyle Hidden -PassThru
    if (!$opened.WaitForExit(30000)) { throw 'Fresh launcher timed out; test-owned process retained for inspection.' }
    if ($opened.ExitCode -ne 0) { throw 'Fresh launcher did not close cleanly.' }
    foreach ($folder in @('ROMs','BIOS','Saves','Save States')) {
        if (@(Get-ChildItem -LiteralPath (Join-Path $package $folder) -Recurse -File).Count) {
            throw "Fresh launcher unexpectedly populated $folder."
        }
    }
    foreach ($name in @('rom.cfg','bios.cfg')) {
        $sidecar = Join-Path $package "Settings/$name"
        if ((Test-Path -LiteralPath $sidecar) -and ([string](Get-Content -LiteralPath $sidecar -Raw)).Trim()) {
            throw "Fresh launcher borrowed a developer input: $name"
        }
    }
    foreach ($file in $before.Keys) {
        if ((Get-FileHash -LiteralPath $file).Hash -ne $before[$file]) { throw 'Startup modified package content.' }
    }
    Write-Output 'PASS: empty-input first launch, normal close, no developer ROM/BIOS/save discovery, read-only preflight, original payload unchanged.'
} finally { $env:PATH = $oldPath; $env:LNG_SCRIPT = $oldScript }
