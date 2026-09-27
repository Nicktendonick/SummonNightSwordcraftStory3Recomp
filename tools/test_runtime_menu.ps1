param([Parameter(Mandatory=$true)][string]$Package, [string]$State = '')
$ErrorActionPreference = 'Stop'
$lab = Split-Path -Parent $PSScriptRoot
$Package = [IO.Path]::GetFullPath($Package)
$validation = [IO.Path]::GetFullPath((Join-Path $lab 'validation'))
if (!$Package.StartsWith($validation + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Use an isolated test package under validation, never player data.'
}
$out = Join-Path $Package 'Logs/menu-tests'
if ($State) { $out = Join-Path $Package 'Logs/menu-tests-state' }
New-Item -ItemType Directory -Path $out -Force | Out-Null
$save = Join-Path $out 'menu-test.eep'
if (!(Test-Path -LiteralPath $save)) {
    $bytes = New-Object byte[] 8192
    for ($i=0; $i -lt $bytes.Length; ++$i) { $bytes[$i] = 255 }
    [IO.File]::WriteAllBytes($save, $bytes)
}
$saveHash = (Get-FileHash -LiteralPath $save).Hash
$env:SWORDCRAFT3_BETA_LAUNCHER = '1'
$env:SWORDCRAFT3_PORTABLE_ROOT = $Package
$env:SWORDCRAFT3_RESET_CHILD = $null
$env:SDL_VIDEODRIVER = 'dummy'
$env:SDL_RENDER_DRIVER = 'software'
$env:SDL_AUDIODRIVER = 'dummy'
$env:GBARECOMP_INPUT_RECORD = $null
$env:GBARECOMP_DEBUG_CAPTURE_DIR = $null
foreach ($flag in @('CUSTOM_RENDERER','FULL_FIELD_RENDERER','FULL_COMBAT_RENDERER','CUSTOM_BATTLES','CUSTOM_GENERAL_FIELDS','CUSTOM_OBJECTS','CUSTOM_ROCKY','CUSTOM_ADDITIONAL_AREAS')) {
    [Environment]::SetEnvironmentVariable("SWORDCRAFT3_$flag", '1', 'Process')
}
$env:SWORDCRAFT3_CUSTOM_HOST_WIDTH = '384'
$runtime = Join-Path $Package 'Runtime/Swordcraft3CustomRendererBeta.exe'
$args = '--window --no-launcher --frames 240 --rom "' + (Join-Path $Package 'ROMs/test.gba') + '" --bios "' + (Join-Path $Package 'BIOS/test.bin') + '" --save "' + $save + '" "' + (Join-Path $Package 'Runtime/game.toml') + '"'
if ($State) {
    Copy-Item -LiteralPath $State -Destination (Join-Path $out 'resume.state') -Force
    $args += ' --load-state "' + (Join-Path $out 'resume.state') + '"'
}
foreach ($pip in @('1','0')) {
    $env:GBARECOMP_PRESENT_IN_PLACE = $pip
    $env:GBARECOMP_ASSIST_SCRIPT = '1:menu_auto_on;10:menu_open;20:menu_probe;30:menu_pause;40:menu_probe;50:menu_open;60:menu_resume;90:menu_probe;100:menu_open;110:menu_reset;120:menu_cancel;130:menu_close;140:menu_cancel;150:menu_back;180:menu_open;190:menu_close;200:menu_confirm'
    $logPath = Join-Path $out "pause-$pip.err"
    $p = Start-Process -FilePath $runtime -ArgumentList $args -WorkingDirectory $out -WindowStyle Hidden -PassThru -RedirectStandardError $logPath -RedirectStandardOutput (Join-Path $out "pause-$pip.log")
    if (!$p.WaitForExit(45000)) { throw "Pause test timed out; test process $($p.Id) retained for inspection." }
    if ($p.ExitCode -ne 0) { throw "Pause test exit: $($p.ExitCode)" }
    $log = Get-Content -LiteralPath $logPath -Raw
    $states = @([regex]::Matches($log, 'event=(menu_\w+) pump=(\d+) frame=(\d+) cycles=(\d+) pc=(\w+) manual_pause=(\d) open=(\d) confirm=(\d) audio=(\d)') | ForEach-Object {
        [pscustomobject]@{ Event=$_.Groups[1].Value; Pump=[int]$_.Groups[2].Value; Frame=[long]$_.Groups[3].Value; Cycles=$_.Groups[4].Value; PC=$_.Groups[5].Value; Pause=$_.Groups[6].Value; Open=$_.Groups[7].Value; Confirm=$_.Groups[8].Value; Audio=$_.Groups[9].Value }
    })
    if ($states.Count -ne 17) { throw 'Missing state events.' }
    $held = @($states | Where-Object { $_.Pump -ge 10 -and $_.Pump -le 60 })
    if (@($held.Frame | Select-Object -Unique).Count -ne 1 -or @($held.PC | Select-Object -Unique).Count -ne 1 -or @($held.Cycles | Select-Object -Unique).Count -ne 1) { throw 'Guest advanced while paused.' }
    if (($states | Where-Object Pump -EQ 40).Pause -ne '1') { throw 'Manual pause did not latch.' }
    if (($states | Where-Object Pump -EQ 90).Frame -le $held[0].Frame) { throw 'Guest failed to resume.' }
    foreach ($pump in @(110,130,190)) {
        if (($states | Where-Object Pump -EQ $pump).Confirm -ne '1') { throw 'Destructive action bypassed confirmation.' }
    }
    foreach ($pump in @(120,140)) {
        if (($states | Where-Object Pump -EQ $pump).Confirm -ne '0') { throw 'Cancel did not dismiss confirmation.' }
    }
    if ($log.Contains('Clean reset:')) { throw 'Cancel accidentally reset the game.' }
    Write-Output "PASS PIP=$pip : automatic/manual pause freezes PC+frame, Resume advances, Reset/Close cancel safely, confirmed Close exits cleanly."
}
$env:GBARECOMP_PRESENT_IN_PLACE = '1'
$env:GBARECOMP_ASSIST_SCRIPT = '10:menu_open;20:menu_reset;30:menu_confirm'
$env:GBARECOMP_ASSIST_SCRIPT_AFTER_RESET = '20:menu_probe;30:menu_open;40:menu_close;50:menu_confirm'
$p = Start-Process -FilePath $runtime -ArgumentList $args -WorkingDirectory $out -WindowStyle Hidden -PassThru -RedirectStandardError (Join-Path $out 'reset.err') -RedirectStandardOutput (Join-Path $out 'reset.log')
if (!$p.WaitForExit(45000)) { throw "Reset test timed out; test process $($p.Id) retained for inspection." }
if ($p.ExitCode -ne 0) { throw "Reset test exit $($p.ExitCode)" }
$log = Get-Content -LiteralPath (Join-Path $out 'reset.err') -Raw
if ([regex]::Matches($log, 'Clean reset:').Count -ne 1 -or !$log.Contains('event=menu_probe pump=20 frame=19')) {
    throw 'Fresh process reset did not boot and service the post-reset menu.'
}
if ((Get-FileHash -LiteralPath $save).Hash -ne $saveHash) { throw 'Reset/close altered the test battery unexpectedly.' }
Write-Output 'PASS: clean-process Reset cold-boots same ROM/BIOS/save; child Close exits supervisor normally; battery hash unchanged.'

function Read-PolicyStates([string]$Log) {
    $result = @{}
    foreach ($m in [regex]::Matches($Log, 'event=(menu_\w+) pump=(\d+) frame=(\d+) cycles=(\d+) pc=(\w+) manual_pause=(\d) open=(\d) confirm=(\d) audio=(\d) auto_pause=(\d)')) {
        $result[[int]$m.Groups[2].Value] = [pscustomobject]@{
            Frame=[long]$m.Groups[3].Value; Cycles=$m.Groups[4].Value; PC=$m.Groups[5].Value;
            Manual=$m.Groups[6].Value; Open=$m.Groups[7].Value; Auto=$m.Groups[10].Value
        }
    }
    return $result
}
foreach ($pip in @('1','0')) {
    $env:GBARECOMP_PRESENT_IN_PLACE = $pip
    $env:GBARECOMP_ASSIST_SCRIPT = '1:menu_auto_on;10:menu_open;20:menu_probe;30:menu_auto_off;50:menu_probe;60:menu_pause;70:menu_probe;80:menu_auto_on;90:menu_auto_off;100:menu_probe;110:menu_resume;130:menu_open;150:menu_probe;160:menu_close;170:menu_confirm'
    $err = Join-Path $out "policy-$pip.err"
    $p = Start-Process -FilePath $runtime -ArgumentList $args -WorkingDirectory $out -WindowStyle Hidden -PassThru -RedirectStandardError $err -RedirectStandardOutput (Join-Path $out "policy-$pip.log")
    if (!$p.WaitForExit(45000) -or $p.ExitCode -ne 0) { throw 'Menu policy runtime test failed/timed out.' }
    $states = Read-PolicyStates (Get-Content -LiteralPath $err -Raw)
    if ($states.Count -ne 15) { throw 'Missing policy state events.' }
    if ($states[10].Frame -ne $states[30].Frame -or $states[10].Cycles -ne $states[30].Cycles) { throw 'Auto-pause On failed.' }
    if ($states[50].Frame -le $states[30].Frame -or $states[50].Open -ne '1' -or $states[50].Auto -ne '0') { throw 'Turning auto-pause Off did not let the open menu run.' }
    foreach ($pump in @(70,80,90,100,110)) {
        if ($states[$pump].Frame -ne $states[60].Frame -or $states[$pump].PC -ne $states[60].PC -or $states[$pump].Cycles -ne $states[60].Cycles -or $states[$pump].Manual -ne '1') {
            throw 'Manual Pause did not stay latched across auto-pause preference changes.'
        }
    }
    if ($states[70].Open -ne '1') { throw 'Pause hid the Resume button.' }
    if ($states[130].Frame -le $states[110].Frame -or $states[150].Frame -le $states[130].Frame -or $states[150].Open -ne '1') { throw 'Resume or live reopened menu failed.' }
    $env:GBARECOMP_ASSIST_SCRIPT = '10:menu_open;20:menu_probe;30:menu_close;40:menu_confirm'
    $err = Join-Path $out "policy-reload-$pip.err"
    $p = Start-Process -FilePath $runtime -ArgumentList $args -WorkingDirectory $out -WindowStyle Hidden -PassThru -RedirectStandardError $err -RedirectStandardOutput (Join-Path $out "policy-reload-$pip.log")
    if (!$p.WaitForExit(45000) -or $p.ExitCode -ne 0) { throw 'Menu preference reload failed/timed out.' }
    $states = Read-PolicyStates (Get-Content -LiteralPath $err -Raw)
    if ($states.Count -ne 4 -or $states[20].Auto -ne '0' -or $states[20].Open -ne '1' -or $states[20].Frame -le $states[10].Frame) { throw 'Menu preference was not retained on the next startup.' }
    Write-Output "PASS PIP=$pip : live/paused menu option, manual Pause stays open, Resume works, manual hold survives option changes, preference persists on fresh startup."
}
if ((Get-FileHash -LiteralPath $save).Hash -ne $saveHash) { throw 'Menu policy tests altered the test battery unexpectedly.' }
