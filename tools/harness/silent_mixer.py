"""Build a TEST-ONLY silent audio plug-in for the build under test, in an explicit staging tree.

Ported from ee tools/boot/silent_mixer.py (VC6, the retail engine's exports). Here silent_mixer.cpp is compiled by
clang-cl as the build compiles the mixer (clang_build.compile_to), takes its export table (names and ordinals)
from the build's own mixer, and is linked by lld-link against the build's engine import library and the runtime,
so it shares one C runtime and one STL with every other staged module.
"""
import hashlib
import json
import pathlib
import subprocess
import sys

import pefile

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(ROOT / "tools"))
from windowed_overlay import test_output   # noqa: E402
import clang_build                           # noqa: E402
from ee import LLD_LINK, MODULE_FLAGS, RUNTIME_LIBS, lib_paths   # noqa: E402


def build_silent_mixer(directory, build):
    directory = pathlib.Path(directory).resolve()
    output = directory / 'Miles Sound System Mixer.dll'
    reference = build.image('mixer')
    test_output(reference, output)
    work = directory / 'silent-mixer-build'
    work.mkdir(exist_ok=True)
    source = HERE / 'silent_mixer.cpp'
    obj = work / 'silent_mixer.obj'
    ok, log = clang_build.compile_to(source, MODULE_FLAGS['mixer'], obj, '-O1')
    if not ok:
        raise SystemExit(f'silent mixer: compile failed\n{log}')
    symbols = [item for item in pefile.PE(str(reference)).DIRECTORY_ENTRY_EXPORT.symbols if item.name]
    definition = work / 'silent.def'
    definition.write_text('LIBRARY "Miles Sound System Mixer"\nEXPORTS\n' +
                          '\n'.join(f'{item.name.decode()} @{item.ordinal}' for item in symbols) + '\n', encoding='ascii')
    r = subprocess.run([str(LLD_LINK), '/nologo', '/DLL', '/NODEFAULTLIB', '/MACHINE:X86', '/SAFESEH:NO',
                        f'/OUT:{output}', f'/IMPLIB:{work / "silent.lib"}', f'/DEF:{definition}', str(obj),
                        str(build.image('lle').with_suffix('.lib')), *RUNTIME_LIBS, *lib_paths()],
                       cwd=work, capture_output=True, text=True)
    if r.returncode:
        raise SystemExit(f'silent mixer: link failed\n{r.stdout}{r.stderr}')
    actual = {(item.name.decode(), item.ordinal) for item in pefile.PE(str(output)).DIRECTORY_ENTRY_EXPORT.symbols
              if item.name}
    if actual != {(item.name.decode(), item.ordinal) for item in symbols}:
        raise ValueError('Silent mixer exports differ from the build\'s plug-in contract')
    manifest = dict(test_only=True, kind='silent-mixer-v3', build=str(build.dir),
                    source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                    output_sha256=hashlib.sha256(output.read_bytes()).hexdigest(),
                    exports=sorted(n for n, _ in actual),
                    reference_sha256=hashlib.sha256(reference.read_bytes()).hexdigest(),
                    timing='GetTickCount with pitch and looping; RIFF byte-rate duration; no hardware clock',
                    limitations='No audio output, DirectSound, DirectMusic or decoding equivalence; RIFF timing only')
    output.with_suffix('.silent.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    return manifest
