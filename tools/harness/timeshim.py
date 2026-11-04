"""TEST-ONLY time-acceleration shim (round 39, owner 2026-11-01): the staged engine's clocks and timed waits run F times
faster than real time; the simulation per PT is unchanged (pin the PT rate: game_session --pt-rate N).

Staged artifacts only: builds tools/boot/timeshim.c into <stage>/EETMS.dll (factor patched into its kConfig block) and
renames the import "WINMM.dll" of the staged <stage>/EE-AOC.exe (the engine is inside it) to "EETMS.dll" (same length). Both exe builds
load the same staged engine, so the factor applies identically to the original and the full link.

  stage_timeshim(stage, factor)    -> manifest dict (also <stage>/EETMS.timeshim.json)
Plugin hooks for game_session.py (par's contract): add_session_arguments(ap) adds --time-factor F; prepare_session(ctx).
Options: --time-factor F, --no-render [--render-key-ms MS], --render-frame-ms MS (all test-only, identical per build).
CLI: python -I tools/boot/timeshim.py <stage dir> <factor> [null 0/1] [key ms] [frame ms]   (stage by hand)
"""
import fractions
import hashlib
import json
import os
import pathlib
import struct
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
GCC = pathlib.Path(r'C:\msys64\mingw32\bin\gcc.exe')
SHIM = 'EETMS.dll'
ENGINE = 'EE-AOC.exe'   # the engine is inside the exe
MAGIC = b'EE-TIMESHIM-CFG\0'
FORWARDED = ['mciSendCommandA', 'auxGetVolume', 'mixerGetControlDetailsA', 'auxSetVolume', 'mixerSetControlDetails',
             'mixerGetLineControlsA', 'mixerGetLineInfoA', 'mixerGetDevCapsA', 'mixerGetNumDevs', 'auxGetDevCapsA',
             'auxGetNumDevs', 'timeBeginPeriod', 'timeEndPeriod']
OWN = {'timeGetTime': 'ShimTimeGetTime@0', 'timeSetEvent': 'ShimTimeSetEvent@20', 'timeKillEvent': 'ShimTimeKillEvent@4'}
BUILD = ROOT/'build/boot/timeshim'


def build_dll():
    """Compile once per source change into build/boot/timeshim/EETMS.dll (factor 1/1)."""
    source = HERE/'timeshim.c'
    out = BUILD/SHIM
    BUILD.mkdir(parents=True, exist_ok=True)
    if out.exists() and out.stat().st_mtime > source.stat().st_mtime and out.stat().st_mtime > pathlib.Path(__file__).stat().st_mtime:
        return out
    tag = str(os.getpid())   # parallel sessions: private temporaries, atomic rename into place
    definition, temp = BUILD/f'timeshim.{tag}.def', BUILD/f'EETMS.{tag}.tmp'
    definition.write_text('LIBRARY "EETMS.dll"\nEXPORTS\n' + ''.join(f'{k}={v}\n' for k, v in OWN.items())
                          + ''.join(f'{n}=winmm.{n}\n' for n in FORWARDED), encoding='ascii')
    subprocess.run([str(GCC), '-O2', '-Wall', '-shared', '-nostdlib', '-o', str(temp), str(source), str(definition),
                    '-Wl,-e,_DllMain@12', '-Wl,--enable-stdcall-fixup', '-lkernel32', '-lwinmm', '-luser32', '-lgcc'],
                   check=True, env={'PATH': str(GCC.parent), 'SYSTEMROOT': r'C:\Windows', 'TEMP': str(BUILD)})
    os.replace(temp, out)
    definition.unlink()
    return out


def parse_factor(text):
    f = fractions.Fraction(str(text)).limit_denominator(1000)
    if not 0 < f <= 4096:
        raise ValueError('time factor must be in (0, 4096]')
    return f


def rename_import(engine):
    data = bytearray(engine.read_bytes())
    import pefile
    pe = pefile.PE(data=bytes(data))
    for d in pe.DIRECTORY_ENTRY_IMPORT:
        if d.dll.lower() == b'winmm.dll':
            off = pe.get_offset_from_rva(d.struct.Name)
            assert data[off:off+10].lower() == b'winmm.dll\0'
            data[off:off+9] = SHIM.encode()
            engine.write_bytes(bytes(data))
            return True
        if d.dll == SHIM.encode():
            return False
    raise ValueError(f'{engine}: no WINMM.dll import')


RENDER_MAIN = '?Main@RTSGraphics@@SAKPAX@Z'
RENDER_MAIN_ORIGINAL = (0x51a3d5, 0x51a4c9)   # analysis/exe/names.txt 0x11a3d5; next function 0x11a4c9


def render_main_extent(fulllink_map=None):
    if fulllink_map is None:
        return RENDER_MAIN_ORIGINAL
    sys.path.insert(0, str(HERE))
    from fulllink_locator import LinkMap
    return LinkMap(fulllink_map).extent(RENDER_MAIN)


def stage_timeshim(stage, factor=1, null_render=False, key_ms=1000, frame_ms=0, fulllink_map=None):
    """null_render: skip the 3D device's rasterising calls (one keyframe scene every key_ms real ms; 0 = none);
    frame_ms: RTSGraphics::Main renders at most one frame per frame_ms real ms (its Sleep(1), exe IAT, caller-checked)."""
    stage = pathlib.Path(stage).resolve()
    if stage.parent != (ROOT/'build/boot').resolve():
        raise ValueError('time shim stages only into build/boot/<stage>')
    f = parse_factor(factor)
    dll = bytearray(build_dll().read_bytes())
    at = dll.find(MAGIC)
    assert at > 0 and dll.find(MAGIC, at+1) < 0
    lo, hi = render_main_extent(fulllink_map) if frame_ms else (0, 0)
    dll[at+16:at+44] = struct.pack('<iiIIIII', f.numerator, f.denominator, lo, hi, frame_ms, int(bool(null_render)), key_ms)
    (stage/SHIM).write_bytes(bytes(dll))
    engine = stage/ENGINE
    before = hashlib.sha256(engine.read_bytes()).hexdigest()
    renamed = rename_import(engine)
    manifest = dict(test_only=True, kind='time-shim-v1', factor=str(f), numerator=f.numerator, denominator=f.denominator,
                    source_sha256=hashlib.sha256((HERE/'timeshim.c').read_bytes()).hexdigest(),
                    output_sha256=hashlib.sha256(bytes(dll)).hexdigest(), engine_sha256_before=before,
                    engine_sha256_after=hashlib.sha256(engine.read_bytes()).hexdigest(), engine_import_renamed=renamed,
                    null_render=bool(null_render), key_ms=key_ms if null_render else None, frame_ms=frame_ms,
                    render_main=[hex(lo), hex(hi)] if frame_ms else None,
                    owner='2026-11-01 round 39: test-only timer shim, identical factor in both builds',
                    scope='staged engine only: timeGetTime/timeSetEvent/timeKillEvent (EETMS.dll) and KERNEL32 '
                          'QueryPerformanceCounter/Sleep/WaitForSingleObject/WaitForMultipleObjects (IAT, in memory); '
                          'null render: the renderer IDirect3DDevice7 Clear/Draw*Primitive* (vtable, in memory); '
                          'frame cap: the exe Sleep IAT slot, acting only for calls from RTSGraphics::Main')
    (stage/'EETMS.timeshim.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
    return manifest


# ---- game_session.py plugin contract (par) ----
def add_session_arguments(ap):
    ap.add_argument('--time-factor', type=parse_factor, default=None, metavar='F',
                    help='TEST ONLY (round 39): engine clocks and timed waits run F x real time (staged EETMS.dll shim); '
                         'pin --pt-rate so the simulation per PT is unchanged')
    ap.add_argument('--no-render', action='store_true',
                    help='TEST ONLY (round 39): null 3D device: the renderer builds every scene but its Clear/Draw*Primitive* '
                         'calls draw nothing, except one keyframe scene per --render-key-ms real ms (UI evidence)')
    ap.add_argument('--render-key-ms', type=int, default=1000, metavar='MS',
                    help='with --no-render: draw one scene every MS real ms (0 = never; UI captures then see no new pixels)')
    ap.add_argument('--render-frame-ms', type=int, default=0, metavar='MS',
                    help='TEST ONLY (round 39): the graphics thread renders at most one frame per MS real ms')


def prepare_session(ctx):
    a = ctx['args']
    f = getattr(a, 'time_factor', None)
    if f is None and not a.no_render and not a.render_frame_ms:
        return None
    m = stage_timeshim(ctx['stage'], f or 1, a.no_render, a.render_key_ms, a.render_frame_ms, ctx.get('map'))
    print(f'TEST time shim factor {m["factor"]} null-render {m["null_render"]} key-ms {m["key_ms"]} frame-ms '
          f'{m["frame_ms"]} staged ({SHIM}; engine import WINMM.dll renamed)', flush=True)
    return dict(interventions=[dict(kind='time-shim', factor=m['factor'], manifest=m)], manifest=m)


def finish_session(ctx, lines):
    a = ctx['args']
    if getattr(a, 'time_factor', None) is None and not a.no_render and not a.render_frame_ms:
        return None
    return dict(shim_lines=[l for l in lines if 'EE-TEST-TIMESHIM' in l])


if __name__ == '__main__':
    print(json.dumps(stage_timeshim(sys.argv[1], sys.argv[2], *(int(x) for x in sys.argv[3:5]),
                                    frame_ms=int(sys.argv[5]) if len(sys.argv) > 5 else 0), indent=2))
