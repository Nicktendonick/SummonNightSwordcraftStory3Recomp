$ErrorActionPreference = 'Stop'
$lab = Split-Path -Parent $PSScriptRoot
$project = [IO.Path]::GetFullPath((Join-Path $lab '../..'))
$package = Join-Path $project 'release/Portable Beta'
$runtime = Join-Path $lab 'build-native/Swordcraft3CustomRendererBeta.exe'
$target = Join-Path $package 'Runtime/Swordcraft3CustomRendererBeta.exe'
$backup = Join-Path $project 'release/Pause Controls Rollback 20260926'
if (Get-Process | Where-Object { $_.ProcessName -match 'Swordcraft|gba_recomp' }) {
    throw 'A game/launcher is running. Close it normally before deployment.'
}
if (Test-Path -LiteralPath $backup) { throw 'Rollback already exists; inspect it before another deployment.' }
if ((Get-FileHash -LiteralPath $target).Hash -ne '4542E47D58533E475BA1EC7909740D2C06364DAC0C2035E3C694E5E013D25088') {
    throw 'Installed runtime changed since this update began; inspect before replacing.'
}
$protected = @{}
foreach ($owner in @($project,$package)) {
    foreach ($folder in @('Settings','Saves','Save States','ROMs','BIOS','Mods','Credits')) {
        $dir = Join-Path $owner $folder
        if (Test-Path -LiteralPath $dir) {
            Get-ChildItem -LiteralPath $dir -Recurse -File | ForEach-Object {
                $protected[$_.FullName] = (Get-FileHash -LiteralPath $_.FullName).Hash
            }
        }
    }
}
New-Item -ItemType Directory -Path $backup | Out-Null
Copy-Item -LiteralPath $target -Destination (Join-Path $backup 'Swordcraft3CustomRendererBeta.exe')
# The root starter already uses the newly built worktree runtime. Only the
# portable package has a second runtime executable to update. No data copying.
Copy-Item -LiteralPath $runtime -Destination $target -Force
if ((Get-FileHash -LiteralPath $target).Hash -ne (Get-FileHash -LiteralPath $runtime).Hash) {
    throw 'Runtime copy verification failed.'
}
foreach ($file in $protected.Keys) {
    if ((Get-FileHash -LiteralPath $file).Hash -ne $protected[$file]) { throw "Protected data changed: $file" }
}
foreach ($owner in @($project,$package)) {
    $p = Start-Process -FilePath (Join-Path $owner 'Swordcraft Story 3 Beta.exe') -ArgumentList '--check' -WorkingDirectory $env:SystemRoot -WindowStyle Hidden -PassThru
    if (!$p.WaitForExit(10000) -or $p.ExitCode -ne 0) { throw "Launcher preflight failed: $owner" }
}
$report = [ordered]@{
    runtime_sha256 = (Get-FileHash -LiteralPath $target).Hash
    rollback_sha256 = (Get-FileHash -LiteralPath (Join-Path $backup 'Swordcraft3CustomRendererBeta.exe')).Hash
    protected_files = $protected.Count
    protected_hashes = $protected
    root_launcher_preflight = 'passed'
    portable_launcher_preflight = 'passed'
}
$report | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $backup 'deployment-verification.json') -Encoding UTF8
Write-Output "PASS: runtime deployed, both launchers pass read-only preflight, $($protected.Count) protected files unchanged."
Write-Output "Rollback: $backup"
