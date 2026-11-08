"""Play the full-link EE-AOC.exe on this VM (run_eeaoc.bat). No exercise, no milestones, no telemetry.

Stages build/boot/play/ with the windowed EXE overlay (sites located by name in the full-link map), the software DX7
renderer, the GDI presentation helper with the absolute-pointer mouse shim (TestPresentation.dll, loaded by the
game's own plug-in scan) and the game data. The install is never modified.
Default (native): the staged game runs as a plain process, no debugger, at full speed; the real Miles mixer is staged
(--silent: the silent mixer and music off, for a box without a sound device). Nothing redirects the game's own
writes: DirectDraw/DirectInput keep their usual MostRecentApplication bookkeeping and sockets are the system's.
--debugger: the old hosted run under tools/boot/exe_boot.c (writes sandboxed, sockets on loopback, silent mixer);
much slower under x86 emulation (every guarded call is a debugger round trip).
Either way the stage gets a fresh config.yaml (the game's settings file; music off with --silent).
--original: play the ORIGINAL exe (orig-bin/aoc/EE-AOC.unwrapped.exe, staged like game_session --original) the same
way, for comparing the two images on one machine; its gUIManager/gMouse come from analysis/exe/names.txt.
usage: play.py [--debugger] [--silent] [--hours H] [--original | image]   (default image build/exe_fulllink/EE-AOC.exe;
       H = upper bound, default 24)"""
import argparse, json, pathlib, subprocess, sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import exe_boot as boot
from presentation_bridge import build_presentation_bridge

ap = argparse.ArgumentParser()
ap.add_argument('--hours', type=float, default=24)
ap.add_argument('--debugger', action='store_true', help='host the game under exe_boot (sandboxed, slow)')
ap.add_argument('--silent', action='store_true', help='silent mixer and music off (no sound device)')
ap.add_argument('--original', action='store_true', help='play the original exe (no full-link map needed)')
ap.add_argument('image', nargs='?', default=str(boot.ROOT / 'build/exe_fulllink/EE-AOC.exe'))
a = ap.parse_args()
silent = a.silent or a.debugger
image = (boot.ROOT / 'orig-bin/aoc/EE-AOC.unwrapped.exe') if a.original else pathlib.Path(a.image).resolve()
harness = boot.build_harness() if a.debugger else None
stage = boot.stage('play', image, data=True, windowed=True, software_display=True, silent_audio=silent,
                   fulllink_map=None if a.original else image.with_suffix('.map'))
# the software profile renders with the staged DX7HRDisplay.dll only. Where Direct3D works (WARP on an ARM64 VM) the
# T&L plug-in's Test32BitSupport leaves TestWindowClass registered with its own window procedure; the plug-in scan
# then unloads it and the software renderer's test window crashes in that stale procedure. Not staged for play.
(stage / 'DX7HRTnLDisplay.dll').unlink()
if a.original:   # fixed original layout: the VAs from analysis/exe/names.txt (RVA column + image base)
    globals_va = {}
    for line in (boot.ROOT / 'analysis/exe/names.txt').read_text(encoding='latin-1').splitlines():
        part = line.split()
        if len(part) >= 2 and part[1] in ('?gUIManager@@3PAVEEUIManager@@A', '?gMouse@@3PAVEEMouse@@A'):
            globals_va.setdefault(part[1], 0x400000 + int(part[0], 16))
else:
    located = json.loads((stage / 'EE-AOC.windowed.json').read_text(encoding='utf-8'))
    globals_va = {s: int(located['globals'][s]['va'], 16) for s in ('?gUIManager@@3PAVEEUIManager@@A', '?gMouse@@3PAVEEMouse@@A')}
# the mouse shim: a VM's absolute tablet gives DirectInput's exclusive relative mouse nothing (presentation_bridge.c)
build_presentation_bridge(stage, None if a.original else globals_va['?gUIManager@@3PAVEEUIManager@@A'],
                          mouse_va=globals_va['?gMouse@@3PAVEEMouse@@A'], play_only=True)
reg_set = ['audio.music.enabled=false'] if silent else []
print(f'playing {stage / "EE-AOC.exe"} ({"debugger" if a.debugger else "native"}, '
      f'{"silent" if silent else "sound"}; quit the game to finish; settings in the stage config.yaml)',
      flush=True)
if a.debugger:
    boot.run(harness, stage, 'run', int(a.hours * 3600 * 1000), None, reg_set,
             windowed=True, checkpoint='session', live_log=str(boot.BOOT / 'play.log.txt'), loopback=True,
             harness_extra=() if a.original else ('--audio-entry', located['audio_entry_rva']))
else:
    (stage / 'config.yaml').write_bytes(boot.config_text(reg_set).encode('cp1252'))   # the game's settings file
    game = subprocess.Popen([str(stage / 'EE-AOC.exe')], cwd=stage)
    try:
        code = game.wait(timeout=a.hours * 3600)
    except subprocess.TimeoutExpired:
        game.kill()
        code = game.wait()
        print(f'stopped after {a.hours} h', flush=True)
    print(f'game exited with {code:#x}', flush=True)
print('game closed', flush=True)
