[CmdletBinding()]
param(
    [switch]$FailOnDirty
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

$repositories = @(
    [pscustomobject]@{ Name = 'game'; Path = $root },
    [pscustomobject]@{ Name = 'gbarecomp'; Path = (Join-Path $root 'gbarecomp') },
    [pscustomobject]@{ Name = 'recomp-ui'; Path = (Join-Path $root 'recomp-ui') }
)

function Invoke-GitCapture {
    param(
        [Parameter(Mandatory)] [string]$Repository,
        [Parameter(Mandatory)] [string[]]$Arguments,
        [switch]$AllowFailure
    )

    $gitArguments = @(
        '-c', "safe.directory=$($Repository.Replace('\', '/'))",
        '-C', $Repository
    ) + $Arguments
    $result = & git @gitArguments 2>&1
    $exitCode = $LASTEXITCODE
    if (-not $AllowFailure -and $exitCode -ne 0) {
        throw "git $($Arguments -join ' ') failed in $Repository`n$result"
    }
    return [pscustomobject]@{ Output = @($result); ExitCode = $exitCode }
}

$problems = [System.Collections.Generic.List[string]]::new()
$dirtyRepositories = [System.Collections.Generic.List[string]]::new()
$forbiddenTracked = [regex]'(?i)(^|/)(roms|build[^/]*)/|gba_bios\.bin$|\.(gba|sav|srm|eep|state[0-9]*)$'
$privateGamePatch = [regex]'(?i)\.(ips|ips32|bps)$'

Write-Host 'Summon Night Swordcraft Story 3 handoff audit'
Write-Host "Root: $root"

foreach ($repository in $repositories) {
    if (-not (Test-Path (Join-Path $repository.Path '.git'))) {
        $problems.Add("Missing Git worktree: $($repository.Path)")
        continue
    }

    $branch = Invoke-GitCapture $repository.Path @('branch', '--show-current')
    $head = Invoke-GitCapture $repository.Path @('rev-parse', '--short=12', 'HEAD')
    $status = Invoke-GitCapture $repository.Path @('status', '--short')
    $tracked = Invoke-GitCapture $repository.Path @('ls-files')

    Write-Host "`n[$($repository.Name)] branch=$($branch.Output -join '') head=$($head.Output -join '')"
    if ($status.Output.Count -eq 0 -or
        ($status.Output.Count -eq 1 -and [string]::IsNullOrWhiteSpace($status.Output[0]))) {
        Write-Host '  worktree: clean'
    } else {
        $dirtyRepositories.Add($repository.Name)
        Write-Host '  worktree: DIRTY'
        $status.Output | ForEach-Object { Write-Host "    $_" }
    }

    foreach ($path in $tracked.Output) {
        $normalized = "$path".Replace('\', '/')
        if ($forbiddenTracked.IsMatch($normalized) -or
            ($repository.Name -eq 'game' -and $privateGamePatch.IsMatch($normalized))) {
            $message = "$($repository.Name) tracks private/generated-looking path: $normalized"
            $problems.Add($message)
            Write-Host "  ERROR: $message" -ForegroundColor Red
        }
    }
}

$ignoreChecks = @(
    [pscustomobject]@{ Repository = $root; Path = 'roms/handoff_probe.gba' },
    [pscustomobject]@{ Repository = $root; Path = 'handoff_probe.bps' },
    [pscustomobject]@{ Repository = $root; Path = 'build-handoff/handoff_probe.state1' },
    [pscustomobject]@{ Repository = (Join-Path $root 'gbarecomp'); Path = 'bios/gba_bios.bin' }
)

Write-Host "`n[ignore rules]"
foreach ($check in $ignoreChecks) {
    $ignored = Invoke-GitCapture $check.Repository @('check-ignore', '-q', '--', $check.Path) -AllowFailure
    if ($ignored.ExitCode -eq 0) {
        Write-Host "  ignored: $($check.Path)"
    } else {
        $message = "Expected private path is not ignored: $($check.Path)"
        $problems.Add($message)
        Write-Host "  ERROR: $message" -ForegroundColor Red
    }
}

Write-Host "`n[summary]"
if ($problems.Count -gt 0) {
    $problems | ForEach-Object { Write-Host "  ERROR: $_" -ForegroundColor Red }
    exit 1
}

if ($dirtyRepositories.Count -gt 0) {
    Write-Host "  Dirty repositories: $($dirtyRepositories -join ', ')"
    if ($FailOnDirty) {
        Write-Host '  ERROR: -FailOnDirty was requested.' -ForegroundColor Red
        exit 2
    }
    Write-Host '  Safe-file checks passed, but commit/push and submodule pinning remain.' -ForegroundColor Yellow
} else {
    Write-Host '  All repositories are clean and no forbidden tracked paths were found.' -ForegroundColor Green
}
