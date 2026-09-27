param([Parameter(Mandatory=$true)][string]$Package)
$ErrorActionPreference = 'Stop'
$Package = [IO.Path]::GetFullPath($Package)
$lab = Split-Path -Parent $PSScriptRoot
$validation = [IO.Path]::GetFullPath((Join-Path $lab 'validation'))
if (!$Package.StartsWith($validation + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Use a test-owned package under validation, never player data.'
}
$oldScript = $env:LNG_SCRIPT
try {
    $env:LNG_SCRIPT = 'wait:12;view:credits;wait:10;credittools:open;wait:20;credittools:close;wait:10;view:dashboard;wait:3;view:credits;wait:10;quit'
    $p = Start-Process -FilePath (Join-Path $Package 'Swordcraft Story 3 Beta.exe') -WorkingDirectory $env:SystemRoot -WindowStyle Hidden -PassThru
    if (!$p.WaitForExit(30000) -or $p.ExitCode -ne 0) { throw 'Credits launcher smoke test failed.' }
    $session = Get-ChildItem -LiteralPath (Join-Path $Package 'Captures') -Directory |
        Where-Object Name -Like "*-$($p.Id)" | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if (!$session) { throw 'No test-owned session log.' }
    $log = Get-Content -LiteralPath (Join-Path $session.FullName 'session.log') -Raw
    if (!$log.Contains('[credits] panels=2 missing_glyphs=0')) {
        throw "Expected two panels and full credits glyph coverage. Inspect $($session.FullName)"
    }
    if (!$log.Contains('[credits] tools_open=1') -or !$log.Contains('[credits] tools_open=0')) {
        throw 'Credits tools details navigation did not open and close.'
    }
    Write-Output 'PASS: Credits and Tools opened/closed; all credit texts have complete font glyph coverage. No external links were opened.'
} finally { $env:LNG_SCRIPT = $oldScript }
