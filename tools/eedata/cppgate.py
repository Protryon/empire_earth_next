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
    for src in (db / 'EEDataText.cpp', db / 'EEDataDump.cpp', db / 'EERules.cpp', HERE / 'cppgate' / 'eedata_gate.cpp'):
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
        texts = tables.export_tree(db, spec)   # the tables and the rules documents
        c = tables.Content(texts, spec)
        import rules
        for n, e in rules.check_all(texts, c, spec):
            print(f'rules {n}: {e}')
            status = 1
        for n, t in texts.items():
            (text / (n + tables.text_ext(spec, n))).write_bytes(t)
            if n in spec['tables'] and not spec['tables'][n].get('text_only'):
                (want / f'{n}.dat').write_bytes(c.build(n))
        print("--- text -> C++ loader -> the Python importer's .dat", flush=True)
        r = subprocess.run([str(exe), str(text), str(want)])
        status |= r.returncode
        status |= run_dump(exe, db, text, tmp, out)
        status |= run_parity(exe, spec, texts, tmp / 'parity', text)
    print('PASS' if status == 0 else 'FAIL')
    return status


def run_dump(exe, db, textdir, tmp, out):
    """G1: the content dump (EEDataDump.cpp: canonical JSON of every value the loaders read; EERules.cpp: the rules)
    of the binary tables (the rules' defaults) and of the text tables compiled by the game's loader (the rules
    documents) must be identical. The two dumps stay in the build folder (OUT/dump_dat.json, dump_text.json)."""
    print('--- content dump: binary .dat vs text (ids resolved), through the game\'s code', flush=True)
    dat = tmp / 'dat'
    dat.mkdir()
    for n, b in db.items():
        (dat / f'{n}.dat').write_bytes(b)
    a, b = out / 'dump_dat.json', out / 'dump_text.json'
    if subprocess.run([str(exe), '--dump-dat', str(dat), str(a)]).returncode or \
            subprocess.run([str(exe), '--dump-text', str(textdir), str(b)]).returncode:
        return 1
    same = a.read_bytes() == b.read_bytes()
    print(f'content dumps {"identical" if same else "DIFFER"} ({a.stat().st_size:,d} bytes)')
    return 0 if same else 1


def run_parity(exe, spec, texts, folder, textdir):
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
        try:   # an edited epochs.yaml compiles into dbtechtree
            want[name] = tables.Content(dict(texts, **{table: text}), spec).build('dbtechtree' if table == 'epochs' else table)
        except (TextError, FormatError) as e:
            want[name] = e
    print('--- parity: edited texts, C++ loader vs Python importer', flush=True)
    r = subprocess.run([str(exe), '--batch', str(folder), str(textdir)])
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
