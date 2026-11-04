"""Build a staging-only GDI presentation bridge for this display-only VM (TestPresentation.dll).

Ported from ee tools/boot/presentation_bridge.py. There the bridge serves the retail renderer and the VC6/Dinkum
layouts; here it is compiled for the build under test (hbuild.Build): every object offset it reads and the
renderer's ShowScene entry come from that build's record layouts and maps (presentation_bridge.c's EE_OFF_*/
EE_SHOWSCENE_RVA defines; their defaults are VC6's).
"""
import hashlib, json, os, pathlib, subprocess, sys
HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from windowed_overlay import test_output   # noqa: E402

GCC = pathlib.Path('C:/msys64/mingw32/bin/gcc.exe')


def build_presentation_bridge(directory, build, play_only=False, mouse=False):
    """directory: the stage; build: hbuild.Build. mouse (play only): compile the absolute-pointer mouse shim.
    play_only: -DEE_PLAY_ONLY, no test telemetry (frame dump, UI state/commands, human/lobby helpers)."""
    directory = pathlib.Path(directory).resolve(); output = directory / 'TestPresentation.dll'
    test_output(build.image('exe'), output)
    work = directory / 'presentation-build'; work.mkdir(exist_ok=True)
    source = HERE / 'presentation_bridge.c'
    defines = build.bridge_defines()
    defines['EE_UI_MANAGER_VA'] = build.address('exe', '?gUIManager@@3PAVEEUIManager@@A')
    if mouse:
        defines['EE_MOUSE_VA'] = build.address('exe', '?gMouse@@3PAVEEMouse@@A')
    env = dict(os.environ, PATH=str(GCC.parent) + os.pathsep + os.environ['PATH'])
    import re
    names = sorted(set(re.findall(r'ee_sym\("([^"]+)"\)', source.read_text(encoding='latin-1'))))
    syms = work / 'bridge_syms.h'
    syms.write_text('#define EE_BRIDGE_SYMS ' + ' '.join(f'{{"{n}", {build.address("exe", n):#x}}},' for n in names)
                    + '\n', encoding='ascii')
    subprocess.run([str(GCC), '-shared', '-O1', '-Wall', '-static-libgcc']
                   + [f'-D{k}={v:#x}' for k, v in sorted(defines.items())]
                   + (['-DEE_PLAY_ONLY'] if play_only else [])
                   + ['-include', str(syms), '-o', str(output), str(source), '-lgdi32', '-luser32', '-Wl,--kill-at'],
                   check=True, env=env)
    output.with_suffix('.presentation.json').write_text(json.dumps(dict(
        test_only=True, kind='gdi-rgb-backbuffer-presentation-v2', build=str(build.dir),
        defines={k: hex(v) for k, v in defines.items()}, mouse_shim=mouse, play_only=play_only,
        source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
        output_sha256=hashlib.sha256(output.read_bytes()).hexdigest(),
        limitations='Replaces the renderer presentation only (RGB32 back buffer through GDI); not hardware rendering'),
        indent=2) + '\n', encoding='utf-8')
    return output
