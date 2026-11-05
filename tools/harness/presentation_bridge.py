"""Build a staging-only GDI presentation bridge for this display-only VM (TestPresentation.dll).

Ported from ee tools/boot/presentation_bridge.py. There the bridge serves the retail renderer and the VC6/Dinkum
layouts; here it is compiled for the build under test (hbuild.Build): every object offset it reads and the
renderer's ShowScene entry come from that build's record layouts and maps (presentation_bridge.c's EE_OFF_*/
EE_SHOWSCENE_RVA defines; their defaults are VC6's). x64 builds get the x86_64 DLL (mingw64 gcc, build.gcc); the
game's symbols are named as the i686 build decorates them (LinkMap translates for an x64 map) and passed as RVAs.
"""
import hashlib, json, os, pathlib, subprocess, sys
HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from windowed_overlay import test_output   # noqa: E402

GCC = pathlib.Path('C:/msys64/mingw32/bin/gcc.exe')   # i686 builds (an x64 build: build.gcc)


def symbol_names(source=HERE / 'presentation_bridge.c'):
    """Every ee_sym("...") name of the bridge source (i686 decorations)."""
    import re
    return sorted(set(re.findall(r'ee_sym\("([^"]+)"\)', pathlib.Path(source).read_text(encoding='latin-1'))))


def compile_bridge(output, defines, rvas, gcc=GCC, play_only=False, work=None):
    """Compile presentation_bridge.c into output: defines {NAME: int}, rvas {i686 name: RVA} (bridge_syms.h)."""
    output = pathlib.Path(output)
    work = pathlib.Path(work or output.parent / 'presentation-build'); work.mkdir(exist_ok=True)
    source = HERE / 'presentation_bridge.c'
    syms = work / 'bridge_syms.h'
    syms.write_text('#define EE_BRIDGE_SYMS ' + ' '.join(f'{{"{n}", {rvas[n]:#x}}},' for n in sorted(rvas))
                    + '\n', encoding='ascii')
    env = dict(os.environ, PATH=str(pathlib.Path(gcc).parent) + os.pathsep + os.environ['PATH'])
    subprocess.run([str(gcc), '-shared', '-O1', '-Wall', '-static-libgcc']
                   + [f'-D{k}={v:#x}' for k, v in sorted(defines.items())]
                   + (['-DEE_PLAY_ONLY'] if play_only else [])
                   + ['-include', str(syms), '-o', str(output), str(source), '-lgdi32', '-luser32', '-Wl,--kill-at'],
                   check=True, env=env)
    return output


def build_presentation_bridge(directory, build, play_only=False, mouse=False):
    """directory: the stage; build: hbuild.Build. mouse (play only): compile the absolute-pointer mouse shim.
    play_only: -DEE_PLAY_ONLY, no test telemetry (frame dump, UI state/commands, human/lobby helpers)."""
    directory = pathlib.Path(directory).resolve(); output = directory / 'TestPresentation.dll'
    test_output(build.image('exe'), output)
    source = HERE / 'presentation_bridge.c'
    defines = build.bridge_defines()
    defines['EE_UI_MANAGER_RVA'] = build.rva('exe', '?gUIManager@@3PAVEEUIManager@@A')
    if mouse:
        defines['EE_MOUSE_RVA'] = build.rva('exe', '?gMouse@@3PAVEEMouse@@A')
    rvas = {n: build.rva('exe', n) for n in symbol_names(source)}
    compile_bridge(output, defines, rvas, build.gcc, play_only, directory / 'presentation-build')
    output.with_suffix('.presentation.json').write_text(json.dumps(dict(
        test_only=True, kind='gdi-rgb-backbuffer-presentation-v2', build=str(build.dir), arch=build.arch,
        defines={k: hex(v) for k, v in defines.items()}, mouse_shim=mouse, play_only=play_only,
        source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
        output_sha256=hashlib.sha256(output.read_bytes()).hexdigest(),
        limitations='Replaces the renderer presentation only (RGB32 back buffer through GDI); not hardware rendering'),
        indent=2) + '\n', encoding='utf-8')
    return output
