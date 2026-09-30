param([Parameter(Mandatory=$true)][string]$Package, [string]$CaseName = '')
$ErrorActionPreference = 'Stop'
$lab = Split-Path -Parent $PSScriptRoot
$owner = [IO.Path]::GetFullPath((Join-Path $lab '../..'))
$Package = [IO.Path]::GetFullPath($Package)
$validation = [IO.Path]::GetFullPath((Join-Path $lab 'validation'))
if (!$Package.StartsWith($validation + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Use a disposable validation package, never a player installation.'
}
$stock = Join-Path $owner 'release/swordcraft3_jp - Copy.gba'
$patch = Join-Path $owner 'release/Summon_Night_Swordcraft_Story_3_Beta - Copy.bps'
$english = Join-Path $lab 'build-native/full-combat-playtest/swordcraft3_beta.gba'
$bios = Join-Path $owner 'gbarecomp/bios/gba_bios.bin'
$jpSha = '3f5253fcf57e07ce52472bd29a61d16b98a12376'
$enSha = 'bb2eebf98deb59bb6218442c2308bb5033ae2915'
if ((Get-FileHash -LiteralPath $stock -Algorithm SHA1).Hash -ne $jpSha -or
    (Get-FileHash -LiteralPath $english -Algorithm SHA1).Hash -ne $enSha) { throw 'Unexpected test ROM revision.' }
$out = Join-Path $Package ('Logs/language-tests-' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfff'))
if (Test-Path -LiteralPath $out) { throw 'Choose a fresh test package.' }
$null = New-Item -ItemType Directory -Path $out
$protected = @{}
foreach ($p in @($stock,$patch,$english,$bios)) { $protected[$p]=(Get-FileHash -LiteralPath $p).Hash }
Copy-Item -LiteralPath $bios -Destination (Join-Path $Package 'BIOS/test.bin')
[IO.File]::WriteAllText((Join-Path $Package 'Settings/bios.cfg'), '../BIOS/test.bin')
$battery = New-Object byte[] 8192
for ($i=0; $i -lt $battery.Length; ++$i) { $battery[$i]=255 }
foreach ($name in @('japanese.eep','battery.eep')) {
    $p=Join-Path $Package "Saves/$name"
    if (Test-Path -LiteralPath $p) {
        $existing=[IO.File]::ReadAllBytes($p)
        if ($existing.Length -ne 8192 -or @($existing | Where-Object { $_ -ne 255 }).Count) {
            throw 'Existing battery is not a blank test fixture; use a fresh test package.'
        }
    } else { [IO.File]::WriteAllBytes($p,$battery) }
    $protected[$p]=(Get-FileHash -LiteralPath $p).Hash
}
$wrong=Join-Path $out 'unsupported.ips'
[IO.File]::WriteAllBytes($wrong,[byte[]](80,65,84,67,72,0,0,0,0,1,88,69,79,70))
$keys=@('PATH','LNG_SCRIPT','SWORDCRAFT3_BETA_LAUNCHER','SWORDCRAFT3_PORTABLE_ROOT',
        'GBARECOMP_ASSIST_SCRIPT','GBARECOMP_ASSIST_SCRIPT_AFTER_RESET','GBARECOMP_INPUT_RECORD',
        'GBARECOMP_DEBUG_CAPTURE_DIR','SDL_VIDEODRIVER','SDL_RENDER_DRIVER','SDL_AUDIODRIVER',
        'SWORDCRAFT3_RESET_CHILD','SWORDCRAFT3_CUSTOM_HOST_WIDTH')
foreach($flag in @('CUSTOM_RENDERER','FULL_FIELD_RENDERER','FULL_COMBAT_RENDERER','CUSTOM_BATTLES','CUSTOM_GENERAL_FIELDS','CUSTOM_OBJECTS','CUSTOM_ROCKY','CUSTOM_ADDITIONAL_AREAS')) { $keys += "SWORDCRAFT3_$flag" }
$previous=@{}
foreach($key in $keys) { $previous[$key]=[Environment]::GetEnvironmentVariable($key,'Process') }
try {
    foreach($key in $keys) { [Environment]::SetEnvironmentVariable($key,$null,'Process') }
    $env:PATH="$env:SystemRoot/System32;$env:SystemRoot"
    $env:SWORDCRAFT3_BETA_LAUNCHER='1'
    $env:SWORDCRAFT3_PORTABLE_ROOT=$Package
    $env:SWORDCRAFT3_CUSTOM_HOST_WIDTH='384'
    foreach($key in $keys | Where-Object { $_ -match '^SWORDCRAFT3_(CUSTOM_RENDERER|FULL_|CUSTOM_BATTLES|CUSTOM_GENERAL_FIELDS|CUSTOM_OBJECTS|CUSTOM_ROCKY|CUSTOM_ADDITIONAL_AREAS)' }) {
        [Environment]::SetEnvironmentVariable($key,'1','Process')
    }
    $env:GBARECOMP_ASSIST_SCRIPT='600:menu_probe;610:menu_open;620:menu_close;630:menu_confirm'
    $engine=Join-Path $Package 'Runtime/Swordcraft3CustomRendererBeta.exe'
    $arguments='--window --launcher --view-width 240 "'+(Join-Path $Package 'Runtime/game.toml')+'"'
    $cases=@(
        @{Name='japanese-original'; Script="romfile:$stock;patchclear"; Sha=$jpSha; Language='Japanese (original)'; Save='japanese.eep'; Engine='Swordcraft3Japanese.exe'},
        @{Name='english-optional'; Script="patchfile:$patch"; Sha=$enSha; Language='English (translation)'; Save='battery.eep'; Engine='Swordcraft3CustomRendererBeta.exe'},
        @{Name='japanese-return'; Script='patchtoggle'; Sha=$jpSha; Language='Japanese (original)'; Save='japanese.eep'; Engine='Swordcraft3Japanese.exe'},
        @{Name='english-prepatched'; Script="patchclear;romfile:$english"; Sha=$enSha; Language='English (translation)'; Save='battery.eep'; Engine='Swordcraft3CustomRendererBeta.exe'},
        @{Name='incompatible-patch'; Script="romfile:$stock;patchfile:$wrong"; Sha=''; Language='English (translation)'; Save='battery.eep'; Engine=''}
    )
    foreach($case in $cases) {
        if ($CaseName -and $case.Name -ne $CaseName) { continue }
        $env:LNG_SCRIPT='wait:12;'+$case.Script+';romprobe;play;wait:8;quit'
        $err=Join-Path $out ($case.Name+'.err')
        $stdout=Join-Path $out ($case.Name+'.log')
        $p=Start-Process -FilePath $engine -ArgumentList $arguments -WorkingDirectory (Join-Path $Package 'Logs') -WindowStyle Hidden -PassThru -RedirectStandardError $err -RedirectStandardOutput $stdout
        if (!$p.WaitForExit(45000)) { throw "Language test timed out; test process $($p.Id) retained for inspection." }
        if ($p.ExitCode -ne 0) { throw "Language case $($case.Name) exited $($p.ExitCode). Inspect $err" }
        $log=[IO.File]::ReadAllText($err)
        $boot=[IO.File]::ReadAllText($stdout)
        if (!$log.Contains('verified=1 can_play=1') -or !$log.Contains('language='+$case.Language) -or
            !$log.Contains($case.Save)) { throw "Incorrect launcher language/save state for $($case.Name)" }
        if ($case.Sha) {
            $probe=[regex]::Match($log,'event=menu_probe pump=600 frame=(\d+) cycles=(\d+)')
            # Windowed runtime suppresses rom_loaded; its verified identity is
            # also present in the self-heal initialization's ROM-specific path.
            if (!$boot.Contains($case.Sha) -or !$log.Contains('engine='+$case.Engine) -or
                !$probe.Success -or [int]$probe.Groups[1].Value -lt 500 -or
                !$boot.Contains('self_heal_coverage=FULLY_STATIC dispatch_misses=0')) {
                throw "Wrong engine or incomplete/static-coverage boot in $($case.Name)"
            }
        } elseif ($boot -match 'self_heal_recompile=' -or $log -notmatch 'play blocked: This executable was built for a different patch release\.') {
            throw 'Incompatible patch reached runtime or was not rejected for the expected reason.'
        }
        foreach($path in $protected.Keys) {
            if ((Get-FileHash -LiteralPath $path).Hash -ne $protected[$path]) { throw "Protected input/save changed: $path" }
        }
        Write-Output "PASS: $($case.Name) - $($case.Language), matching engine/save; original inputs and both batteries unchanged."
    }
} finally {
    foreach($key in $keys) { [Environment]::SetEnvironmentVariable($key,$previous[$key],'Process') }
}
