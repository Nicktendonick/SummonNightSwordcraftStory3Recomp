"""State-based live host-aspect checks; isolated settings and read-only media."""
import json
import re
import subprocess
import sys
import time
from pathlib import Path
from benchmark_presentation_filters import ROOT, OWNER, DEFAULT_STATE, environment, digest

OUT = ROOT / ('validation/host-aspects-' + str(time.time_ns()))
OUT.mkdir()
neutral = OUT / 'neutral.trace'
neutral.write_text('# gbarecomp-keyinput-v1\n0,0x03ff\n')
bios = OWNER / 'gbarecomp/bios/gba_bios.bin'
languages = (
    ('English', 'Swordcraft3CustomRendererBeta.exe', OWNER / 'build-beta/rom-patch-cache/swordcraft3_beta.gba'),
    ('Japanese', 'Swordcraft3Japanese.exe', OWNER / 'release/swordcraft3_jp - Copy.gba'),
)
protected = {str(p): digest(p) for p in (bios, DEFAULT_STATE, *(item[2] for item in languages))}


def run(exe, args, env, directory, label):
    with (directory / (label+'.out')).open('wb') as out, (directory / (label+'.err')).open('wb') as err:
        child = subprocess.Popen([str(exe), *args], cwd=ROOT, env=env, stdout=out, stderr=err,
                                 creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            child.wait(timeout=55)
            assert child.returncode == 0, (label, child.returncode, directory)
        finally:
            if child.poll() is None:
                child.terminate()
                child.wait(timeout=10)
    return (directory / (label+'.err')).read_text(errors='replace')


try:
    for language, binary, rom in languages:
        directory = OUT / language
        (directory / 'Settings').mkdir(parents=True)
        ini = directory / 'Settings/launcher.ini'
        ini.write_text('[Launcher]\nhost_aspect_index = 2\nscale = 3\nscreen = raw\n'
                       'linear_filter = 0\nsharp_filter = 0\nsmooth_filter = 1\n'
                       'screen_effect = 1\nscreen_effect_strength = 35\n'
                       '[KeyMap]\nPause = Shift+P\n')
        env = environment(directory, 384, neutral, audit=True)
        if '--real-window' in sys.argv:
            env.pop('SDL_VIDEODRIVER', None)
            env.pop('SDL_RENDER_DRIVER', None)
        env['GBARECOMP_ASSIST_SCRIPT'] = (
            '1:menu_auto_on;5:menu_open;10:menu_aspect=0;15:menu_probe;20:menu_resume;'
            '60:menu_open;65:menu_aspect=1;70:menu_resume;110:menu_open;115:menu_aspect=2;'
            '120:menu_resume;160:menu_open;165:menu_aspect=0;170:menu_resume;210:menu_open;'
            '215:menu_aspect=2;220:menu_resume;260:menu_open;265:menu_aspect=1;270:menu_resume;'
            '310:menu_open;315:menu_aspect=3;320:menu_probe;325:menu_reset;330:menu_confirm')
        env['GBARECOMP_ASSIST_SCRIPT_AFTER_RESET'] = '10:menu_open;20:menu_probe;30:menu_close;40:menu_confirm'
        exe = ROOT / 'build-native' / binary
        args = ['--window', '--no-launcher', '--frames', '600', '--bios', str(bios), '--rom', str(rom),
                '--save', str(directory/'private.eep'), '--view-width', '240',
                '--smooth-filter', '1', '--screen-effect', 'lcd', str(ROOT/'native-test.toml')]
        if language == 'English': args += ['--load-state', str(DEFAULT_STATE)]
        log = run(exe, args, env, directory, 'live-reset')
        assert '[host-aspect] initial index=2 host=384 guest=240' in log
        transitions = re.findall(r'\[host-aspect\] index=(\d+) host=(\d+) guest=(\d+)', log)
        assert transitions == [(str(i), str((240,284,384)[i]), '240') for i in (0,1,2,0,2,1)], transitions
        assert '[runtime-aspect] selected=0 applied=2 host=384 guest=240' in log, 'Paused selection should wait for Resume'
        assert 'Clean reset:' in log
        after = log.split('Clean reset:', 1)[1]
        assert '[host-aspect] initial index=1 host=284 guest=240' in after, after[-2000:]
        assert '[runtime-aspect] selected=1 applied=1 host=284 guest=240' in after
        held = re.findall(r'event=menu_(?:open|aspect=0|probe|resume) pump=(\d+) frame=(\d+) cycles=(\d+) pc=(\w+)', log.split('Clean reset:', 1)[0])
        held = [row[1:] for row in held if 5 <= int(row[0]) <= 20]
        assert len(held) >= 3 and len(set(held)) == 1, ('Paused guest advanced', held)
        assert 'Pause = Shift+P' in ini.read_text()
        assert 'host_aspect_index = 1' in ini.read_text()
        if language == 'English':
            owned = re.findall(r'\[sc3:composition\][^\n]*complete_owner=1[^\n]*', log)
            assert owned, 'No widened combat frames engaged'
        for key in ('SDL_VIDEODRIVER', 'SDL_RENDER_DRIVER', 'SDL_AUDIODRIVER',
                    'GBARECOMP_ASSIST_SCRIPT', 'GBARECOMP_ASSIST_SCRIPT_AFTER_RESET'):
            env.pop(key, None)
        env['LNG_SCRIPT'] = ('wait:12;view:settings;aspectprobe;wait:4;shot:' +
            str(directory/'launcher-aspect.png') + ';aspectcycle;aspectprobe;wait:4;quit')
        launch = ['--window', '--launcher', str(ROOT/'native-test.toml')]
        log = run(exe, launch, env, directory, 'launcher')
        assert '[launcher:aspect] index=1 label=Widescreen (16:9)' in log, log[-3000:]
        assert '[launcher:aspect] index=2 label=Ultrawide (12:5)' in log
        env['LNG_SCRIPT'] = 'wait:12;view:settings;aspectprobe;wait:4;quit'
        log = run(exe, launch, env, directory, 'reopen')
        assert '[launcher:aspect] index=2 label=Ultrawide (12:5)' in log
        print('PASS '+language+': six live transitions, paused guest, invalid index, reset, launcher edit/reopen', flush=True)
finally:
    unchanged = all(digest(Path(path)) == value for path, value in protected.items())
    (OUT/'inputs.json').write_text(json.dumps(dict(unchanged=unchanged, sha256=protected), indent=2))
    print('Evidence: '+str(OUT), flush=True)
    assert unchanged
