param(
    [string]$BuildDir = "build-assist",
    [int]$PacingPresents = 60,
    [switch]$SkipPacing,
    [switch]$SkipStateRoundTrip,
    [switch]$SkipCallDepthGuard
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$exePath = Join-Path $root "$BuildDir/SummonNightSwordcraftStory3Recomp.exe"
$biosPath = Join-Path $root "gbarecomp/bios/gba_bios.bin"
$sourceRomPath = Join-Path $root "roms/swordcraft3_jp.gba"
$configPath = Join-Path $root "game.toml"
$tracePath = Join-Path $root "tests/input/new_game_select_male.trace"
$outputPath = Join-Path $root "validation/assist-runtime"
$testRomPath = Join-Path $outputPath "assist_runtime_test.gba"
$testSavePath = Join-Path $outputPath "assist_runtime_test.eep"
$launcherConfigPath = Join-Path $root "$BuildDir/config.ini"
$launcherConfigBackup = Join-Path $outputPath "config.ini.backup"
$statePath = [IO.Path]::ChangeExtension($testRomPath, ".state1")

foreach ($required in @($exePath, $biosPath, $sourceRomPath,
                         $configPath, $tracePath)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Required validation input not found: $required"
    }
}

New-Item -ItemType Directory -Force -Path $outputPath | Out-Null
Copy-Item -LiteralPath $sourceRomPath -Destination $testRomPath -Force
$hadLauncherConfig = Test-Path -LiteralPath $launcherConfigPath
if ($hadLauncherConfig) {
    Copy-Item -LiteralPath $launcherConfigPath `
        -Destination $launcherConfigBackup -Force
}

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class AssistValidationKeys {
    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")]
    public static extern bool PostMessage(IntPtr hWnd, uint message,
                                           UIntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")]
    public static extern uint MapVirtualKey(uint code, uint mapType);
    [DllImport("user32.dll")]
    public static extern void keybd_event(byte key, byte scan,
                                           uint flags, UIntPtr extra);
}
"@

$KeyUp = 0x0002
$WmKeyDown = 0x0100
$WmKeyUp = 0x0101
$VkShift = 0x10
$VkF1 = 0x70
$VkRewind = 0x31
$VkFastForward = 0x32

function Send-KeyDown([byte]$key) {
    if ($script:TargetWindow -ne [IntPtr]::Zero) {
        $scan = [AssistValidationKeys]::MapVirtualKey($key, 0)
        $lParam = [IntPtr](1 -bor ($scan -shl 16))
        [AssistValidationKeys]::PostMessage(
            $script:TargetWindow, $script:WmKeyDown,
            [UIntPtr]$key, $lParam) | Out-Null
        return
    }
    [AssistValidationKeys]::keybd_event($key, 0, 0, [UIntPtr]::Zero)
}

function Send-KeyUp([byte]$key) {
    if ($script:TargetWindow -ne [IntPtr]::Zero) {
        $scan = [AssistValidationKeys]::MapVirtualKey($key, 0)
        $lParam = [IntPtr](1 -bor ($scan -shl 16) -bor 0xC0000000)
        [AssistValidationKeys]::PostMessage(
            $script:TargetWindow, $script:WmKeyUp,
            [UIntPtr]$key, $lParam) | Out-Null
        return
    }
    [AssistValidationKeys]::keybd_event($key, 0, $script:KeyUp,
                                        [UIntPtr]::Zero)
}

function Send-KeyTap([byte]$key) {
    Send-KeyDown $key
    Start-Sleep -Milliseconds 200
    Send-KeyUp $key
}

function Send-SaveSlotOne {
    Send-KeyDown $script:VkShift
    Send-KeyTap $script:VkF1
    Send-KeyUp $script:VkShift
}

function Set-AssistConfig([int]$multiplier) {
    $text = @"
[Launcher]
assist_tools = 1
assist_fast_forward_multiplier = $multiplier
assist_rewind_key = 30
assist_fast_key = 31
"@
    [IO.File]::WriteAllText($script:launcherConfigPath, $text,
                            [Text.UTF8Encoding]::new($false))
}

function Wait-ForWindow([Diagnostics.Process]$process) {
    $deadline = [DateTime]::UtcNow.AddSeconds(20)
    do {
        if ($process.HasExited) {
            throw "Runtime exited before opening its game window"
        }
        $process.Refresh()
        if ($process.MainWindowHandle -ne [IntPtr]::Zero) {
            [AssistValidationKeys]::SetForegroundWindow(
                $process.MainWindowHandle) | Out-Null
            $script:TargetWindow = $process.MainWindowHandle
            Start-Sleep -Milliseconds 250
            return
        }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Timed out waiting for the runtime game window"
}

function Start-ValidationRun(
    [string]$name,
    [int]$presents,
    [int]$multiplier,
    [bool]$holdFastForward,
    [string]$assistScript = "",
    [scriptblock]$interaction = $null,
    [int]$timeoutSeconds = 180,
    [int]$presentCallDepthLimit = 0
) {
    Set-AssistConfig $multiplier
    $stdoutPath = Join-Path $script:outputPath "$name.stdout.log"
    $stderrPath = Join-Path $script:outputPath "$name.stderr.log"
    $phasePath = Join-Path $script:outputPath "$name.frame-phase.csv"
    $cadencePath = Join-Path $script:outputPath "$name.present-cadence.csv"
    foreach ($old in @($stdoutPath, $stderrPath, $phasePath, $cadencePath)) {
        Remove-Item -LiteralPath $old -Force -ErrorAction SilentlyContinue
    }

    $arguments = @(
        "--window", "--frames", $presents,
        "--save", $script:testSavePath,
        "--bios", $script:biosPath,
        "--rom", $script:testRomPath,
        $script:configPath
    )
    $quoted = $arguments | ForEach-Object {
        '"' + ([string]$_).Replace('"', '\"') + '"'
    }
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $script:exePath
    $start.WorkingDirectory = $script:root
    $start.Arguments = $quoted -join " "
    $start.UseShellExecute = $false
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $start.EnvironmentVariables["PATH"] =
        "C:\msys64\mingw64\bin;" + $env:PATH
    $start.EnvironmentVariables["GBARECOMP_NO_LAUNCHER"] = "1"
    $start.EnvironmentVariables["GBARECOMP_INPUT_REPLAY"] = $script:tracePath
    $start.EnvironmentVariables["GBARECOMP_FRAME_PHASE"] = $phasePath
    $start.EnvironmentVariables["GBARECOMP_PRESENT_CADENCE"] = "1"
    $start.EnvironmentVariables["GBARECOMP_PRESENT_CADENCE_DUMP"] =
        $cadencePath
    if ($assistScript) {
        $start.EnvironmentVariables["GBARECOMP_ASSIST_SCRIPT"] =
            $assistScript
    }
    if ($presentCallDepthLimit -gt 0) {
        $start.EnvironmentVariables[
            "GBARECOMP_PRESENT_IN_PLACE_CALL_DEPTH"] =
                [string]$presentCallDepthLimit
    }

    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $start
    $wall = [Diagnostics.Stopwatch]::StartNew()
    if (-not $process.Start()) { throw "Failed to start $name" }
    $script:TargetWindow = [IntPtr]::Zero
    try {
        Wait-ForWindow $process
        if ($holdFastForward) { Send-KeyDown $script:VkFastForward }
        if ($interaction) { & $interaction $process }
        if (-not $process.WaitForExit($timeoutSeconds * 1000)) {
            $process.Kill()
            throw "$name exceeded its $timeoutSeconds-second timeout"
        }
    }
    finally {
        if ($holdFastForward) { Send-KeyUp $script:VkFastForward }
        $script:TargetWindow = [IntPtr]::Zero
        $wall.Stop()
    }

    $stdout = $process.StandardOutput.ReadToEnd()
    $stderr = $process.StandardError.ReadToEnd()
    [IO.File]::WriteAllText($stdoutPath, $stdout)
    [IO.File]::WriteAllText($stderrPath, $stderr)
    if ($process.ExitCode -ne 0) {
        throw "$name failed with exit code $($process.ExitCode); see $stderrPath"
    }
    [pscustomobject]@{
        Name = $name
        Multiplier = $multiplier
        FastForward = $holdFastForward
        WallSeconds = [Math]::Round($wall.Elapsed.TotalSeconds, 3)
        Stdout = $stdout
        Stderr = $stderr
        PhasePath = $phasePath
        CadencePath = $cadencePath
    }
}

function Measure-Pacing($run) {
    $rows = @(Import-Csv -LiteralPath $run.PhasePath)
    if ($rows.Count -lt 4) { throw "$($run.Name) produced too few timing rows" }
    $frames = @($rows | ForEach-Object { [long]$_.frame })
    $cadenceRows = @(Import-Csv -LiteralPath $run.CadencePath)
    if ($cadenceRows.Count -lt 2) {
        throw "$($run.Name) produced too few presentation rows"
    }
    $guestFrames = $frames[-1] - $frames[0]
    [pscustomobject]@{
        Mode = if ($run.FastForward) { "$($run.Multiplier)x" } else { "1x" }
        Presents = $cadenceRows.Count
        GuestFrames = $guestFrames
        GuestFramesPerPresent = [Math]::Round(
            $rows.Count / $cadenceRows.Count, 2)
        WallSeconds = $run.WallSeconds
        ActualGuestFps = [Math]::Round($guestFrames / $run.WallSeconds, 2)
        ActualSpeed = [Math]::Round(
            $guestFrames / $run.WallSeconds / 59.7275, 2)
    }
}

$pacingResults = @()
try {
    if (-not $SkipPacing) {
        $baseline = Start-ValidationRun "pacing_1x" $PacingPresents 4 $false
        $pacingResults += Measure-Pacing $baseline
        foreach ($multiplier in @(2, 4, 10)) {
            $run = Start-ValidationRun "pacing_${multiplier}x" `
                $PacingPresents $multiplier $true
            $pacingResults += Measure-Pacing $run
        }
        $pacingResults | Export-Csv -LiteralPath `
            (Join-Path $outputPath "pacing-summary.csv") -NoTypeInformation
        $pacingResults | Format-Table -AutoSize
    }

    if (-not $SkipCallDepthGuard) {
        $guardRun = Start-ValidationRun `
            -name "present_call_depth_guard" -presents 120 `
            -multiplier 4 -holdFastForward $false `
            -assistScript "1:fast_on" -timeoutSeconds 60 `
            -presentCallDepthLimit 1
        if ($guardRun.Stderr -notmatch
            "present-in-place call-depth unwind") {
            throw "Present-in-place call-depth guard did not activate"
        }
        Write-Host "PASS: present-in-place call-depth guard unwound safely"
    }

    if (-not $SkipStateRoundTrip) {
        Remove-Item -LiteralPath $statePath -Force `
            -ErrorAction SilentlyContinue
        $stateScript =
            "1:fast_on;6100:fast_off;6150:save1;6300:load1;6500:rewind"
        $stateRun = Start-ValidationRun `
            -name "state_rewind_roundtrip" -presents 1300 `
            -multiplier 10 -holdFastForward $false `
            -assistScript $stateScript -timeoutSeconds 240
        if ($stateRun.Stdout -notmatch "savestate_saved slot=1") {
            throw "State round-trip did not report a slot-1 save"
        }
        if ($stateRun.Stdout -notmatch "savestate_loaded slot=1") {
            throw "State round-trip did not report a slot-1 load"
        }
        if ($stateRun.Stdout -notmatch "rewind_loaded frame=") {
            throw "State round-trip did not report a rewind load"
        }
        if (-not (Test-Path -LiteralPath $statePath)) {
            throw "State round-trip did not create $statePath"
        }
        Write-Host "PASS: slot-1 save/load and rewind hotkeys completed"
    }
}
finally {
    Send-KeyUp $VkFastForward
    Send-KeyUp $VkRewind
    Send-KeyUp $VkF1
    Send-KeyUp $VkShift
    if ($hadLauncherConfig) {
        Copy-Item -LiteralPath $launcherConfigBackup `
            -Destination $launcherConfigPath -Force
    } else {
        Remove-Item -LiteralPath $launcherConfigPath -Force `
            -ErrorAction SilentlyContinue
    }
    Remove-Item -LiteralPath $launcherConfigBackup -Force `
        -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $testRomPath -Force -ErrorAction SilentlyContinue
}
