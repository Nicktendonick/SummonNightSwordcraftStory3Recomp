param([string]$Build = 'build-native')
$ErrorActionPreference = 'Stop'
$lab = Split-Path -Parent $PSScriptRoot
$owner = [IO.Path]::GetFullPath((Join-Path $lab '../..'))
$Build = (Resolve-Path -LiteralPath $Build).Path
$root = Join-Path $lab ('validation/filter-menus-' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfff'))
$null = New-Item -ItemType Directory -Path $root
$bios = Join-Path $owner 'gbarecomp/bios/gba_bios.bin'
$english = Join-Path $owner 'build-beta/rom-patch-cache/swordcraft3_beta.gba'
$japanese = Join-Path $owner 'release/swordcraft3_jp - Copy.gba'
$protected = @{}
foreach ($file in @($bios,$english,$japanese)) { $protected[$file] = (Get-FileHash -LiteralPath $file).Hash }
$savedEnv = @{}
foreach ($entry in Get-ChildItem Env: | Where-Object Name -Match '^(SDL_|SWORDCRAFT3_|GBARECOMP_|LNG_)') {
    $savedEnv[$entry.Name] = $entry.Value
    [Environment]::SetEnvironmentVariable($entry.Name, $null, 'Process')
}
function Invoke-TestProcess([string]$Exe, [string]$LaunchArguments, [string]$LogPrefix) {
    $p = Start-Process -FilePath $Exe -ArgumentList $LaunchArguments -WorkingDirectory $root -WindowStyle Hidden -PassThru -RedirectStandardOutput ($LogPrefix+'.out') -RedirectStandardError ($LogPrefix+'.err')
    if (!$p.WaitForExit(30000)) { throw "Test-owned process $($p.Id) timed out; retained for inspection." }
    if ($p.ExitCode -ne 0) { throw "Test process failed: $LogPrefix ($($p.ExitCode))" }
    return Get-Content -LiteralPath ($LogPrefix+'.err') -Raw
}
try {
    $env:SWORDCRAFT3_BETA_LAUNCHER = '1'
    $env:GBARECOMP_SELFHEAL_RECOMPILE = '0'
    foreach ($language in @('English','Japanese')) {
        $session = Join-Path $root $language
        $null = New-Item -ItemType Directory -Path (Join-Path $session 'Settings')
        $env:SWORDCRAFT3_PORTABLE_ROOT = $session
        $ini = Join-Path $session 'Settings/launcher.ini'
        '[Launcher]','scale = 3','screen = raw','[KeyMap]','Pause = Shift+P' | Set-Content -LiteralPath $ini
        $exe = Join-Path $Build $(if ($language -eq 'English') { 'Swordcraft3CustomRendererBeta.exe' } else { 'Swordcraft3Japanese.exe' })
        $rom = if ($language -eq 'English') { $english } else { $japanese }
        $env:SDL_VIDEODRIVER = 'dummy'; $env:SDL_RENDER_DRIVER = 'software'; $env:SDL_AUDIODRIVER = 'dummy'
        $env:GBARECOMP_ASSIST_SCRIPT = '1:menu_auto_on;5:menu_open;10:menu_scaling=3;15:menu_effect=2;20:menu_strength=65;25:menu_probe;30:menu_resume;50:menu_open;60:menu_reset;70:menu_confirm'
        $env:GBARECOMP_ASSIST_SCRIPT_AFTER_RESET = '10:menu_open;20:menu_probe;30:menu_close;40:menu_confirm'
        $launch = '--window --no-launcher --frames 180 --bios "'+$bios+'" --rom "'+$rom+'" --save "'+(Join-Path $session 'synthetic.eep')+'" "'+(Join-Path $lab 'native-test.toml')+'"'
        $log = Invoke-TestProcess $exe $launch (Join-Path $session 'live-reset')
        if ($log -notmatch '\[runtime-filters\] scaling=3 effect=2 strength=65') { throw 'Live filter changes failed.' }
        $events = @([regex]::Matches($log,'event=menu_[^ ]+ pump=(\d+) frame=(\d+) cycles=(\d+) pc=(\w+)'))
        $held = @($events | Where-Object { [int]$_.Groups[1].Value -ge 5 -and [int]$_.Groups[1].Value -le 30 } | Select-Object -First 6)
        if ($held.Count -ne 6 -or @($held | ForEach-Object { $_.Groups[2].Value+':'+$_.Groups[3].Value+':'+$_.Groups[4].Value } | Select-Object -Unique).Count -ne 1) { throw 'Guest advanced while changing filters in paused menu.' }
        $reset = $log.IndexOf('Clean reset:')
        if ($reset -lt 0 -or !$log.Substring($reset).Contains('[runtime-filters] scaling=3 effect=2 strength=65')) { throw 'Reset lost live filter preferences.' }
        $settings = Get-Content -LiteralPath $ini -Raw
        foreach ($line in @('smooth_filter = 1','screen_effect = 2','screen_effect_strength = 65','Pause = Shift+P')) {
            if (!$settings.Contains($line)) { throw "Missing saved preference: $line" }
        }
        # The actual launcher reloads runtime-written settings through its own seam.
        $env:SDL_VIDEODRIVER=$null; $env:SDL_RENDER_DRIVER=$null; $env:SDL_AUDIODRIVER=$null
        $env:GBARECOMP_ASSIST_SCRIPT=$null; $env:GBARECOMP_ASSIST_SCRIPT_AFTER_RESET=$null
        $env:LNG_SCRIPT = 'wait:12;view:settings;filterprobe;scaling:sharp;effect:lcd;effectstrength:45;filterprobe;wait:4;quit'
        $launch = '--window --launcher "'+(Join-Path $lab 'native-test.toml')+'"'
        $log = Invoke-TestProcess $exe $launch (Join-Path $session 'launcher')
        if (!$log.Contains('scaler=Smooth 2x linear=0 sharp=0 smooth=1 effect=2 strength=65') -or
            !$log.Contains('scaler=Sharp fractional linear=0 sharp=1 smooth=0 effect=1 strength=45')) { throw 'Launcher did not reload or change filter choices.' }
        $env:LNG_SCRIPT = 'wait:12;view:settings;filterprobe;wait:4;quit'
        $log = Invoke-TestProcess $exe $launch (Join-Path $session 'reopen')
        if (!$log.Contains('scaler=Sharp fractional linear=0 sharp=1 smooth=0 effect=1 strength=45')) { throw 'Launcher settings did not survive reopen.' }
        $env:LNG_SCRIPT = $null
        Write-Output "PASS $language : live changes, paused guest PC/cycles, cold Reset, launcher reload/edit/reopen, unrelated settings preserved."
    }
    foreach ($file in $protected.Keys) {
        if ((Get-FileHash -LiteralPath $file).Hash -ne $protected[$file]) { throw 'A private source input changed.' }
    }
    Write-Output "Evidence: $root"
} finally {
    foreach ($entry in Get-ChildItem Env: | Where-Object Name -Match '^(SDL_|SWORDCRAFT3_|GBARECOMP_|LNG_)') {
        [Environment]::SetEnvironmentVariable($entry.Name, $null, 'Process')
    }
    foreach ($name in $savedEnv.Keys) { [Environment]::SetEnvironmentVariable($name, $savedEnv[$name], 'Process') }
}
