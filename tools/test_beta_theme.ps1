param([Parameter(Mandatory=$true)][string]$Package)
$ErrorActionPreference = 'Stop'
$lab = Split-Path -Parent $PSScriptRoot
$Package = [IO.Path]::GetFullPath($Package)
$validation = [IO.Path]::GetFullPath((Join-Path $lab 'validation'))
if (!$Package.StartsWith($validation + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Use a test-owned package under validation, never player data.'
}
$cycle = @('archer','winter','parchment','crystal','glasses','ensemble','horizon','beginnings','swordsmith','original','clean')
$artState = Join-Path $Package 'Settings/boxart.txt'
[IO.File]::WriteAllText($artState, "clean`n")
$protected = @{}
foreach ($folder in @('Saves','Save States','Credits','ROMs','BIOS')) {
    Get-ChildItem -LiteralPath (Join-Path $Package $folder) -Recurse -File | ForEach-Object {
        $protected[$_.FullName] = (Get-FileHash -LiteralPath $_.FullName).Hash
    }
}
$oldScript = $env:LNG_SCRIPT
$oldPath = $env:PATH
try {
    $env:PATH = "$env:SystemRoot/System32;$env:SystemRoot"
    for ($i = 0; $i -lt $cycle.Count; ++$i) {
        $env:LNG_SCRIPT = 'wait:12;quit'
        if ($i -eq 0) {
            # State-driven views and real keyboard capture; no pixel assertions.
            $env:LNG_SCRIPT = 'wait:12;view:settings;wait:8;view:controller;player:0;wait:8;capbtn:4;key:H;wait:4;view:assist_tools;wait:8;view:mods;wait:8;view:credits;wait:8;credittools:open;wait:8;credittools:close;wait:3;size:960x720;view:dashboard;wait:8;view:settings;wait:8;view:controller;wait:8;view:credits;wait:8;size:800x600;view:dashboard;wait:8;view:settings;wait:8;view:controller;wait:8;view:assist_tools;wait:8;view:mods;wait:8;view:credits;wait:8;quit'
        }
        $p = Start-Process -FilePath (Join-Path $Package 'Swordcraft Story 3 Beta.exe') -WorkingDirectory $env:SystemRoot -WindowStyle Hidden -PassThru
        if (!$p.WaitForExit(30000)) { throw 'Theme smoke test timed out; test-owned process left for inspection.' }
        if ($p.ExitCode -ne 0) { throw "Launcher exit $($p.ExitCode) for $($cycle[$i])" }
        $session = Get-ChildItem -LiteralPath (Join-Path $Package 'Captures') -Directory |
            Where-Object Name -Like "*-$($p.Id)" | Sort-Object LastWriteTime -Descending | Select-Object -First 1
        if (!$session) { throw 'No test session log.' }
        $log = Get-Content -LiteralPath (Join-Path $session.FullName 'session.log') -Raw
        $token = $cycle[$i]
        $prefix = if ($token -in @('original','clean')) { 'boxart' } else { 'art' }
        if ((Get-Content -LiteralPath $artState -Raw).Trim() -ne $token -or
            !$log.Contains("[sc3:launcher] boxart=assets/beta/$prefix-$token.png")) {
            throw "Wrong artwork token/path: $token"
        }
        if ($log -notmatch '\[appearance\] sidebar=1 art=[1-9]\d*x[1-9]\d* logo=1448x1086 backdrop=[1-9]\d*x[1-9]\d*') {
            throw "Missing or unloaded theme assets: $token. Inspect $($session.FullName)"
        }
        if ($i -eq 0) {
            foreach ($view in @(0,1,2,4,5,6)) {
                if (!$log.Contains("[appearance] view=$view content=")) { throw "View $view was not drawn." }
            }
            if (!$log.Contains('[credits] panels=2 missing_glyphs=0') -or
                !$log.Contains('[credits] tools_open=1') -or !$log.Contains('[credits] tools_open=0')) {
                throw 'Credits text or tools navigation regression.'
            }
        }
        $ini = Get-Content -LiteralPath (Join-Path $Package 'Settings/launcher.ini') -Raw
        if ($ini -notmatch '(?m)^player_key_0 = 11\r?$') { throw 'Keyboard capture did not persist across startup.' }
        Write-Output "PASS: $token loaded, startup rotation persisted, launcher closed normally."
    }
    foreach ($file in $protected.Keys) {
        if ((Get-FileHash -LiteralPath $file).Hash -ne $protected[$file]) { throw "Protected file changed: $file" }
    }
    Write-Output 'PASS: six pages, three window sizes, credits/tools, keyboard capture, 11 artworks; saves, credits, ROM and BIOS unchanged. No game was booted.'
} finally { $env:LNG_SCRIPT = $oldScript; $env:PATH = $oldPath }
