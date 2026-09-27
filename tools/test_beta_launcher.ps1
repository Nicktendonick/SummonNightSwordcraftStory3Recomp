$ErrorActionPreference = 'Stop'
$lab = Split-Path -Parent $PSScriptRoot
$owner = [IO.Path]::GetFullPath((Join-Path $lab '../..'))
$launcher = Join-Path $owner 'Swordcraft Story 3 Beta.exe'
$play = Join-Path $lab 'build-native/full-combat-playtest'
$artState = Join-Path $owner 'Settings/boxart.txt'
$legacyArt = Join-Path $lab 'build-native/beta-boxart-state.txt'
if (!(Test-Path -LiteralPath $artState) -and (Test-Path -LiteralPath $legacyArt)) {
    $previousArt = (Get-Content -LiteralPath $legacyArt -Raw).Trim()
} else {
$previousArt = if (Test-Path -LiteralPath $artState) { (Get-Content -LiteralPath $artState -Raw).Trim() } else { '' }
}
$artworkCycle = @('clean', 'archer', 'winter', 'parchment', 'crystal', 'glasses', 'ensemble', 'horizon', 'beginnings', 'swordsmith', 'original')
$previousIndex = [Array]::IndexOf($artworkCycle, $previousArt)
$expectedArt = $artworkCycle[($previousIndex + 1) % $artworkCycle.Count]
$saveHashes = @{}
Get-ChildItem -LiteralPath $play -File | Where-Object { $_.Name -match '\.(eep|state\d+)$' } | ForEach-Object {
    $saveHashes[$_.FullName] = (Get-FileHash -LiteralPath $_.FullName).Hash
}
$previousPath = $env:PATH
try {
    # The starter must not rely on the development toolchain PATH.
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
    $check = Start-Process -FilePath $launcher -ArgumentList '--check' -WindowStyle Hidden -PassThru
    if (!$check.WaitForExit(10000)) { throw 'Launcher preflight timed out' }
    if ($check.ExitCode -ne 0) { throw "Launcher preflight failed: $($check.ExitCode)" }
    $afterCheckArt = if (Test-Path -LiteralPath $artState) { (Get-Content -LiteralPath $artState -Raw).Trim() } else { '' }
    if ((Test-Path -LiteralPath $artState) -and $afterCheckArt -ne $previousArt) { throw 'Preflight advanced the cover selection' }
    $started = Start-Process -FilePath $launcher -WorkingDirectory $env:SystemRoot -WindowStyle Hidden -PassThru
    $child = $null
    try {
        $deadline = [DateTime]::UtcNow.AddSeconds(20)
        do {
            $info = Get-CimInstance Win32_Process -Filter "ParentProcessId=$($started.Id)" |
                Where-Object Name -eq 'Swordcraft3CustomRendererBeta.exe' | Select-Object -First 1
            if ($info) {
                $child = Get-Process -Id $info.ProcessId
                if ($child.MainWindowTitle -like '*PC Beta*Launcher*') { break }
            }
            if ($started.HasExited) { throw 'Starter exited before launcher UI appeared' }
            Start-Sleep -Milliseconds 200
        } while ([DateTime]::UtcNow -lt $deadline)
        if (!$child -or $child.MainWindowTitle -notlike '*PC Beta*Launcher*') {
            throw 'Expected recomp-ui launcher window did not appear'
        }
        Write-Output "PASS: recomp-ui opened: $($child.MainWindowTitle)"
        Write-Output "Child command: $($info.CommandLine)"
        if (!$child.CloseMainWindow()) { throw 'Could not request normal launcher cancellation' }
        if (!$started.WaitForExit(10000)) { throw 'Launcher cancellation did not finish' }
        if ($started.ExitCode -ne 0) { throw "Launcher cancellation returned $($started.ExitCode)" }
        if ((Get-Content -LiteralPath $artState -Raw).Trim() -ne $expectedArt) { throw 'Cover did not alternate' }
        $session = Get-ChildItem -LiteralPath (Join-Path $owner 'Captures') -Directory |
            Where-Object Name -Like "*-$($started.Id)" | Sort-Object LastWriteTime -Descending | Select-Object -First 1
        if (!$session) { throw 'No log directory for test-owned launcher' }
        $sessionLog = Get-Content -LiteralPath (Join-Path $session.FullName 'session.log') -Raw
        $artPrefix = if ($expectedArt -in @('clean', 'original')) { 'boxart' } else { 'art' }
        if (!$sessionLog.Contains("[sc3:launcher] boxart=assets/beta/$artPrefix-$expectedArt.png")) {
            throw 'Game did not select the expected cover for recomp-ui'
        }
        Write-Output "PASS: $previousArt -> $expectedArt cover selected; preflight did not advance rotation"
    } finally {
        # Only handles of processes created by this test; never user's game.
        if ($child -and !$child.HasExited) { $null=$child.CloseMainWindow() }
    }
    foreach ($file in $saveHashes.Keys) {
        if ((Get-FileHash -LiteralPath $file).Hash -ne $saveHashes[$file]) { throw "Save changed: $file" }
    }
    foreach ($file in $saveHashes.Keys) {
        $name = [IO.Path]::GetFileName($file)
        $target = if ($name -eq 'native-renderer.eep') { Join-Path $owner 'Saves/battery.eep' }
                  else { Join-Path $owner ('Save States/beta' + [IO.Path]::GetExtension($name)) }
        if ((Get-FileHash -LiteralPath $target).Hash -ne $saveHashes[$file]) { throw "Migration mismatch: $target" }
    }
    Write-Output "PASS: clean system PATH, unrelated working directory, normal cancellation, $($saveHashes.Count) save files unchanged"
} finally { $env:PATH=$previousPath }
