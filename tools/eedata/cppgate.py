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


def cargo_gate():
    """Builds the Rust side of the gate (ee_data_gate, no game code) -> its path."""
    import rust_build
    if rust_build.run_cargo(['build', '--release', '-p', 'ee_data', '--bin', 'ee_data_gate']):
        sys.exit('cargo build of ee_data_gate failed')
    return rust_build.TARGET_DIR / 'release' / ('ee_data_gate.exe' if sys.platform == 'win32' else 'ee_data_gate')


def run(a, spec, load_db):
    out = pathlib.Path(a.out or ROOT / 'build' / 'eedata-cppgate').resolve()
    exe = build(out, a.j)
    rust = cargo_gate()
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
        status |= subprocess.run([str(exe), str(text), str(want)]).returncode
        print("--- text -> Rust compiler -> the Python importer's .dat", flush=True)
        status |= subprocess.run([str(rust), str(text), str(want)], capture_output=True).returncode
        status |= run_dump(exe, rust, db, text, tmp, out)
        status |= run_parity(exe, rust, spec, texts, tmp / 'parity', text)
        status |= run_rules_parity(exe, rust, texts, tmp / 'rules', text)
        status |= run_floats(exe, rust, tmp, out)
    print('PASS' if status == 0 else 'FAIL')
    return status


def run_dump(exe, rust, db, textdir, tmp, out):
    """G1: the content dump of the binary tables (with the rules' defaults) and of the text tables must be identical
    in each compiler, and identical between the C++ and the Rust compiler."""
    print('--- content dump: binary .dat vs text (ids resolved), C++ and Rust', flush=True)
    dat = tmp / 'dat'
    dat.mkdir()
    for n, b in db.items():
        (dat / f'{n}.dat').write_bytes(b)
    status = 0
    results = []
    for name, gate in (('cpp', exe), ('rust', rust)):
        a, b = out / f'dump_dat_{name}.json', out / f'dump_text_{name}.json'
        if subprocess.run([str(gate), '--dump-dat', str(dat), str(a)]).returncode or \
                subprocess.run([str(gate), '--dump-text', str(textdir), str(b)]).returncode:
            return 1
        same = a.read_bytes() == b.read_bytes()
        print(f'{name}: content dumps {"identical" if same else "DIFFER"} ({a.stat().st_size:,d} bytes)')
        status |= 0 if same else 1
        results.append((a, b))
    (ca, cb), (ra, rb) = results
    both = ca.read_bytes() == ra.read_bytes() and cb.read_bytes() == rb.read_bytes()
    print(f'dumps C++ vs Rust: {"identical" if both else "DIFFER"}')
    return status | (0 if both else 1)


def compare_dirs(folder, names):
    """{case: bytes or message} of each compiler's .out/.err in folder/<compiler>."""
    out = {}
    for c in ('cpp', 'rust'):
        got = {}
        for name in names:
            o, e = folder / c / f'{name}.out', folder / c / f'{name}.err'
            got[name] = o.read_bytes() if o.exists() else ('E', e.read_bytes())
        out[c] = got
    return out


def run_batch(gate, flag, case_dir, cases, textdir, compiler):
    d = case_dir / compiler
    d.mkdir(parents=True)
    for name, table, text in cases:
        (d / f'{name}__{table}{EXT[table]}').write_bytes(text)
    r = subprocess.run([str(gate), flag, str(d), str(textdir)], capture_output=True, text=True)
    return r.returncode


EXT = {}


def run_parity(exe, rust, spec, texts, folder, textdir):
    """Edited texts: the C++ loader, the Rust compiler and the Python importer must give the same bytes or all an error
    (the C++ and Rust messages byte-identical)."""
    import parity
    from binfmt import FormatError
    from textio import TextError
    folder.mkdir(parents=True)
    cs = parity.cases(texts)
    want = {}
    EXT.clear()
    EXT.update({t: ('.yaml' if spec['tables'][t]['format'] == 'yaml' else '.csv') for t in spec['tables']})
    for name, table, text in cs:
        try:   # an edited epochs.yaml compiles into dbtechtree
            want[name] = tables.Content(dict(texts, **{table: text}), spec).build('dbtechtree' if table == 'epochs' else table)
        except (TextError, FormatError) as e:
            want[name] = e
    print('--- parity: edited texts, C++ loader vs Rust vs Python importer', flush=True)
    if run_batch(exe, '--batch', folder, cs, textdir, 'cpp') or run_batch(rust, '--batch', folder, cs, textdir, 'rust'):
        return 1
    both = compare_dirs(folder, [c[0] for c in cs])
    bad = mismatch = errors = 0
    show = lambda v: f'{len(v)} bytes' if isinstance(v, bytes) else f'error: {v[1]!r}'
    for name, table, _ in cs:
        w, cpp, rs = want[name], both['cpp'][name], both['rust'][name]
        if cpp != rs:
            mismatch += 1
            print(f'  {name:22s} C++ {show(cpp)} | Rust {show(rs)}')
        if isinstance(w, bytes) and isinstance(cpp, bytes):
            ok = w == cpp
        else:
            ok = not isinstance(w, bytes) and not isinstance(cpp, bytes)
            errors += ok
        if not ok:
            bad += 1
            print(f'  {name:22s} DIFFERS  Python {f"{len(w)} bytes" if isinstance(w, bytes) else "error: " + str(w)}  |  C++ {show(cpp)}')
    print(f'{len(cs) - bad}/{len(cs)} parity cases agree with Python ({errors} rejected by both, '
          f'{len(cs) - bad - errors} accepted with identical bytes); {len(cs) - mismatch}/{len(cs)} C++ == Rust')
    return 1 if bad or mismatch else 0


def run_rules_parity(exe, rust, texts, folder, textdir):
    """Edited rules documents: both compilers give the same dump or the same message."""
    import rules_parity
    folder.mkdir(parents=True)
    docs = {n: t for n, t in texts.items() if n in ('combat', 'gameoptions', 'civbuilder', 'civpowers', 'aipersonalities', 'epochs', 'dbcivilization')}
    cs = rules_parity.cases(docs)
    for _, doc, _t in cs:
        EXT[doc] = '.yaml'
    print('--- rules parity: edited documents, C++ vs Rust', flush=True)
    if run_batch(exe, '--rules-batch', folder, cs, textdir, 'cpp') or run_batch(rust, '--rules-batch', folder, cs, textdir, 'rust'):
        return 1
    both = compare_dirs(folder, [c[0] for c in cs])
    bad = errors = 0
    show = lambda v: f'{len(v)} bytes' if isinstance(v, bytes) else f'error: {v[1]!r}'
    for name, _, _t in cs:
        cpp, rs = both['cpp'][name], both['rust'][name]
        errors += not isinstance(cpp, bytes)
        if cpp != rs:
            bad += 1
            print(f'  {name:22s} C++ {show(cpp)} | Rust {show(rs)}')
    print(f'{len(cs) - bad}/{len(cs)} rules cases agree ({errors} rejected by both, {len(cs) - bad - errors} accepted with identical dumps)')
    return 1 if bad else 0


def run_floats(exe, rust, tmp, out):
    """A sweep of float strings (edge cases and random magnitudes): the C++ std::from_chars and Rust f32::from_str
    bits and the messages must be identical."""
    import random
    rnd = random.Random(0xEE0)
    cases = ['0', '-0', '0.0', '-0.0', '1', '1.5', '.5', '5.', '1e2', '1E2', '1e+2', '1e-2', '0.1', '0.2', '0.3',
             '3.4028235e38', '3.4028236e38', '1e39', '-1e39', '1.17549435e-38', '1.17549434e-38', '1.4e-45', '7e-46',
             '7.1e-46', '1e-50', '-1e-50', '1e99999', '1e-99999', '0e99999', '0e-99999', '1.0e0000000000000000001',
             '16777217', '16777219', '33554434', '9007199254740993', '1.00000017881393432617187499',
             '1.00000017881393432617187500', '1.00000017881393432617187501', '2.5e-1', '4.684328e-39', '.inf', '-.inf',
             '.nan', 'inf', '-inf', 'infinity', '-infinity', 'nan', 'NaN', 'Infinity', '.INF', '0x3f800000',
             '0X3F800000', '0x7fc00001', '0xffffffff', '0x100000000', '0x', 'abc', '', ' ', '1e', '1.2.3', '+', '-',
             '.', 'e5', '-.5', '+.5', '- 1', '1 2', '0x1p3', '1_000', ' 3.5 ', '\t2.5\t']
    for _ in range(12000):
        kind = rnd.randrange(6)
        if kind == 0:
            cases.append(repr(rnd.uniform(-1e6, 1e6)))
        elif kind == 1:   # random digits with a random exponent
            digits = ''.join(rnd.choice('0123456789') for _ in range(rnd.randrange(1, 18)))
            cases.append(f'{digits}e{rnd.randrange(-60, 60)}')
        elif kind == 2:   # a float's exact decimal expansion, nearby values
            f = rnd.uniform(1e-44, 1e38) if rnd.random() < 0.5 else 10 ** rnd.uniform(-44, 38)
            cases.append(format(f, '.25e'))
        elif kind == 3:
            cases.append(f'{rnd.randrange(0, 10**9)}.{rnd.randrange(0, 10**9):09d}')
        elif kind == 4:   # just past float boundaries: 2^k +- tiny
            k = rnd.randrange(-149, 128)
            cases.append(format(2.0 ** k * (1 + rnd.choice((-1, 1)) * 2 ** -rnd.randrange(23, 30)), '.40g'))
        else:
            cases.append(str(rnd.randrange(-2**31, 2**31)))
    infile = tmp / 'floats.in'
    infile.write_text('\n'.join(cases), encoding='ascii')
    for name, gate in (('cpp', exe), ('rust', rust)):
        if subprocess.run([str(gate), '--floats', str(infile), str(out / f'floats_{name}.txt')]).returncode:
            return 1
    a = (out / 'floats_cpp.txt').read_bytes()
    b = (out / 'floats_rust.txt').read_bytes()
    print(f'float sweep: {len(cases)} strings; C++ and Rust {"identical" if a == b else "DIFFER"}', flush=True)
    if a != b:
        la, lb = a.split(b'\n'), b.split(b'\n')
        shown = 0
        for i, (x, y) in enumerate(zip(la, lb)):
            if x != y and shown < 20:
                print(f'  {cases[i]!r}: C++ {x!r} | Rust {y!r}')
                shown += 1
        return 1
    return 0
