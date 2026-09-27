param([Parameter(Mandatory=$true)][string]$Package)
$ErrorActionPreference = 'Stop'
$lab = Split-Path -Parent $PSScriptRoot
$owner = [IO.Path]::GetFullPath((Join-Path $lab '../..'))
$Package = [IO.Path]::GetFullPath($Package)
if (!$Package.StartsWith((Join-Path $lab 'validation') + [IO.Path]::DirectorySeparatorChar,
                         [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Use a disposable package under this worktree validation folder, never a player installation.'
}
$originalRom = Join-Path $lab 'build-native/full-combat-playtest/swordcraft3_beta.gba'
$originalBios = Join-Path $owner 'gbarecomp/bios/gba_bios.bin'
$romHash = (Get-FileHash -LiteralPath $originalRom).Hash
$biosHash = (Get-FileHash -LiteralPath $originalBios).Hash
# Test-owned copies only. The launcher must not depend on the developer's tree.
Copy-Item -LiteralPath $originalRom -Destination (Join-Path $Package 'ROMs/test.gba')
Copy-Item -LiteralPath $originalBios -Destination (Join-Path $Package 'BIOS/test.bin')
[IO.File]::WriteAllText((Join-Path $Package 'Settings/rom.cfg'), "../ROMs/test.gba")
[IO.File]::WriteAllText((Join-Path $Package 'Settings/bios.cfg'), "../BIOS/test.bin")
$oldScript=$env:LNG_SCRIPT
$oldPath=$env:PATH
try {
    $env:PATH="$env:SystemRoot/System32;$env:SystemRoot"
    # Exercise actual capture and close/persistence, without screenshots.
    $env:LNG_SCRIPT='wait:12;view:controller;player:0;wait:3;capbtn:4;key:G;wait:4;view:assist_tools;wait:3;quit'
    $p = Start-Process -FilePath (Join-Path $Package 'Swordcraft Story 3 Beta.exe') -WorkingDirectory $env:SystemRoot -WindowStyle Hidden -PassThru
    if (!$p.WaitForExit(30000)) { throw 'Portable launcher test timed out; test-owned window left for inspection.' }
    if ($p.ExitCode -ne 0) { throw "Portable launcher exit: $($p.ExitCode)" }
    $ini = Get-Content -LiteralPath (Join-Path $Package 'Settings/launcher.ini') -Raw
    if ($ini -notmatch '(?m)^player_key_0 = 10\r?$') { throw 'Keyboard remap did not persist on normal close.' }
    $relativeRom=(Get-Content -LiteralPath (Join-Path $Package 'Settings/rom.cfg') -Raw).Trim()
    if ([IO.Path]::IsPathRooted($relativeRom)) { throw 'ROM sidecar is not portable.' }
    # Move only this resolved test package within validation, never player data.
    $Moved = $Package + '-moved'
    if (Test-Path -LiteralPath $Moved) { throw 'Relocation destination already exists.' }
    Move-Item -LiteralPath $Package -Destination $Moved
    $Package=$Moved
    $env:LNG_SCRIPT='wait:12;view:controller;wait:3;quit'
    $p = Start-Process -FilePath (Join-Path $Package 'Swordcraft Story 3 Beta.exe') -WorkingDirectory $env:SystemRoot -WindowStyle Hidden -PassThru
    if (!$p.WaitForExit(30000) -or $p.ExitCode -ne 0) { throw 'Relocated launcher failed.' }
    $ini = Get-Content -LiteralPath (Join-Path $Package 'Settings/launcher.ini') -Raw
    if ($ini -notmatch '(?m)^player_key_0 = 10\r?$') { throw 'Remap did not survive relocation.' }
    if ($romHash -ne (Get-FileHash -LiteralPath $originalRom).Hash) { throw 'Original ROM changed.' }
    if ($biosHash -ne (Get-FileHash -LiteralPath $originalBios).Hash) { throw 'Original BIOS changed.' }
    Write-Output "PASS: actual keyboard remap, close persistence, system-only PATH, different cwd, folder relocation. Original ROM/BIOS unchanged. Test folder: $Package"
} finally { $env:LNG_SCRIPT=$oldScript; $env:PATH=$oldPath }
