param([Parameter(Mandatory=$true)][string]$Package)
$ErrorActionPreference='Stop'
$lab=Split-Path -Parent $PSScriptRoot
$owner=[IO.Path]::GetFullPath((Join-Path $lab '../..'))
$Package=(Resolve-Path -LiteralPath $Package).Path
$destination=(Resolve-Path -LiteralPath (Join-Path $owner 'release/Portable Beta')).Path
$candidateRoot=(Resolve-Path -LiteralPath (Join-Path $owner 'release/Portable Release Candidates')).Path
if (!$Package.StartsWith($candidateRoot+'\',[StringComparison]::OrdinalIgnoreCase)) {
    throw 'Install from a clean release candidate, not a played test package.'
}
$manifest=Get-Content -LiteralPath (Join-Path $Package 'Runtime/package-manifest.json') -Raw | ConvertFrom-Json
# Japanese first, then the English frontend, then the top-level starter: a
# half-updated frontend must never offer an engine which is not present yet.
$names=@('Runtime/Swordcraft3Japanese.exe','Runtime/Swordcraft3CustomRendererBeta.exe','Swordcraft Story 3 Beta.exe')
foreach($name in $names) {
    $row=@($manifest.files | Where-Object path -EQ $name)
    if ($row.Count -ne 1 -or (Get-FileHash -LiteralPath (Join-Path $Package $name)).Hash -ne $row[0].sha256) {
        throw "Candidate binary does not match manifest: $name"
    }
}
foreach($process in @(Get-Process -Name 'Swordcraft*' -ErrorAction SilentlyContinue)) {
    if (!$process.Path -or $process.Path.StartsWith($destination+'\',[StringComparison]::OrdinalIgnoreCase)) {
        throw 'Close the installed launcher/game before applying this update. No process was stopped.'
    }
}
$protected=@{}
foreach($folder in @('ROMs','BIOS','Mods','Saves','Save States','Settings','Credits')) {
    foreach($file in Get-ChildItem -LiteralPath (Join-Path $destination $folder) -Recurse -File) {
        $protected[$file.FullName]=(Get-FileHash -LiteralPath $file.FullName).Hash
    }
}
$backup=Join-Path $owner ('release/Language Support Rollback '+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfff'))
$null=New-Item -ItemType Directory -Path $backup
$replacements=@()
foreach($name in $names + @('README.md')) {
    $target=Join-Path $destination $name
    if (Test-Path -LiteralPath $target) {
        $bak=Join-Path $backup $name
        $null=New-Item -ItemType Directory -Path (Split-Path -Parent $bak) -Force
        Copy-Item -LiteralPath $target -Destination $bak
        if ((Get-FileHash -LiteralPath $target).Hash -ne (Get-FileHash -LiteralPath $bak).Hash) { throw 'Backup verification failed.' }
    }
}
foreach($name in $names) {
    $source=Join-Path $Package $name
    $target=Join-Path $destination $name
    Copy-Item -LiteralPath $source -Destination $target -Force
    $hash=(Get-FileHash -LiteralPath $target).Hash
    if ($hash -ne (Get-FileHash -LiteralPath $source).Hash) { throw "Installed hash mismatch: $name" }
    $replacements += @{path=$name; sha256=$hash}
}
Copy-Item -LiteralPath (Join-Path $lab 'docs/PORTABLE_BETA.md') -Destination (Join-Path $destination 'README.md') -Force
foreach($path in $protected.Keys) {
    if ((Get-FileHash -LiteralPath $path).Hash -ne $protected[$path]) { throw "Protected file changed: $path" }
}
$launcher=Start-Process -FilePath (Join-Path $destination 'Swordcraft Story 3 Beta.exe') -ArgumentList '--check' -WorkingDirectory $env:SystemRoot -WindowStyle Hidden -PassThru
if (!$launcher.WaitForExit(10000) -or $launcher.ExitCode -ne 0) { throw 'Updated launcher preflight failed.' }
$report=@{installed=$destination; backup=$backup; source=$Package; binaries=$replacements; protected_files=$protected.Count; unchanged_player_data=$true; preflight_passed=$true}
$report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $backup 'UPDATE-REPORT.json') -Encoding UTF8
$report | ConvertTo-Json -Depth 5
