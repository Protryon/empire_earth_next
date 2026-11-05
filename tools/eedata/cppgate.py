"""The C++ gate (eedata.py cppgate): build tools/eedata/cppgate/eedata_gate.exe from the game's text loader
(src/Empire Earth/Database/EEDataText.cpp, the same compiler flags as the build) and run it over every table: the
text must compile to the bytes the Python importer makes of it, and the edited texts of parity.py must give the same
bytes or an error in both.
The exported text goes to a temporary folder outside the repository and is deleted afterwards.
"""
import pathlib
import subprocess
import sys
import tempfile

import tables

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(ROOT / 'tools'))


def build(out, jobs):
    import clang_build
    import verify
    from ee import CLANG, LLD_LINK, lib_paths, RUNTIME_LIBS
    db = ROOT / 'src' / 'Empire Earth' / 'Database'
    out.mkdir(parents=True, exist_ok=True)
    _, inc = verify.include_paths(None)
    objs = []
    for src in (db / 'EEDataText.cpp', HERE / 'cppgate' / 'eedata_gate.cpp'):
        obj = out / (src.stem + '.obj')
        cmd = [str(CLANG), *clang_build.CLANG_FLAGS, '/std:c++17', '-EHsc', '-MT', '-O1',
               *verify.DEFINES.split(), f'/I{db}', f'/Fo{obj}', str(src)]
        r = subprocess.run(cmd, env=dict(__import__('os').environ, INCLUDE=inc), capture_output=True, text=True)
        if r.returncode:
            sys.exit(f'{src.name}: {r.stdout}{r.stderr}')
        objs.append(str(obj))
    exe = out / 'eedata_gate.exe'
    r = subprocess.run([str(LLD_LINK), '/nologo', '/subsystem:console', '/machine:x64', f'/out:{exe}', *objs,
                        *lib_paths(), *RUNTIME_LIBS], capture_output=True, text=True)
    if r.returncode:
        sys.exit(f'link: {r.stdout}{r.stderr}')
    return exe


def run(a, spec, load_db):
    out = pathlib.Path(a.out or ROOT / 'build' / 'eedata-cppgate').resolve()
    exe = build(out, a.j)
    db = load_db(a, spec)
    status = 0
    with tempfile.TemporaryDirectory(prefix='eedata-') as tmp:
        tmp = pathlib.Path(tmp)
        text, want = tmp / 'text', tmp / 'want'
        text.mkdir(parents=True)
        want.mkdir(parents=True)
        texts = {}
        for n, b in db.items():
            t = tables.export(n, b, spec)
            texts[n] = t
            (text / (n + ('.yaml' if spec['tables'][n]['format'] == 'yaml' else '.csv'))).write_bytes(t)
            (want / f'{n}.dat').write_bytes(tables.import_(n, t, spec))
        print("--- text -> C++ loader -> the Python importer's .dat", flush=True)
        r = subprocess.run([str(exe), str(text), str(want)])
        status |= r.returncode
        status |= run_parity(exe, spec, texts, tmp / 'parity')
    print('PASS' if status == 0 else 'FAIL')
    return status


def run_parity(exe, spec, texts, folder):
    """Edited texts: the C++ loader and the Python importer must give the same bytes or both an error."""
    import parity
    from binfmt import FormatError
    from textio import TextError
    folder.mkdir(parents=True)
    cs = parity.cases(texts)
    want = {}
    for name, table, text in cs:
        ext = '.yaml' if spec['tables'][table]['format'] == 'yaml' else '.csv'
        (folder / f'{name}__{table}{ext}').write_bytes(text)
        try:
            want[name] = tables.import_(table, text, spec)
        except (TextError, FormatError) as e:
            want[name] = e
    print('--- parity: edited texts, C++ loader vs Python importer', flush=True)
    r = subprocess.run([str(exe), '--batch', str(folder)])
    if r.returncode:
        return 1
    bad = errors = 0
    for name, table, _ in cs:
        out, err = folder / f'{name}.out', folder / f'{name}.err'
        got = out.read_bytes() if out.exists() else err.read_text(errors='replace')
        w = want[name]
        if isinstance(w, bytes) and isinstance(got, bytes):
            ok = w == got
        else:
            ok = not isinstance(w, bytes) and not isinstance(got, bytes)
            errors += ok
        if not ok:
            bad += 1
            show = lambda v: f'{len(v)} bytes' if isinstance(v, bytes) else f'error: {v}'
            print(f'  {name:22s} DIFFERS  Python {show(w)}  |  C++ {show(got)}')
    print(f'{len(cs) - bad}/{len(cs)} parity cases agree ({errors} rejected by both, '
          f'{len(cs) - bad - errors} accepted with identical bytes)')
    return 1 if bad else 0
