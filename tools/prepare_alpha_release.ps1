param([string]$Python = '', [string]$Strip = 'C:\msys64\mingw64\bin\strip.exe')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $Python) {
    $localPythonRoot = Join-Path $env:LOCALAPPDATA 'Programs\Python'
    if (Test-Path -LiteralPath $localPythonRoot) {
        $Python = Get-ChildItem -LiteralPath $localPythonRoot -Directory |
            Sort-Object Name -Descending | ForEach-Object {
                $candidate = Join-Path $_.FullName 'python.exe'
                if (Test-Path -LiteralPath $candidate) { $candidate }
            } | Select-Object -First 1
    }
    if (-not $Python) {
        $command = Get-Command python.exe -ErrorAction SilentlyContinue
        if ($command -and $command.Source -notlike '*WindowsApps*') { $Python = $command.Source }
    }
}
if (-not $Python) { throw 'Python 3 is needed only to prepare this local package. Pass -Python with its executable path.' }
Write-Host 'Preparing Alpha unfinished-build v.01 (LOCAL REVIEW ONLY). No GitHub upload.'
Push-Location $projectRoot
try {
    & $Python (Join-Path $PSScriptRoot 'package_alpha.py') --strip $Strip
    if ($LASTEXITCODE -ne 0) { throw 'Packaging failed; inspect the error above. Existing builds/candidates were not removed.' }
} finally { Pop-Location }
