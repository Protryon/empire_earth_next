"""The gate of the Rust text compiler (eedata.py cppgate): build tools/eedata/cppgate/eedata_gate.exe (the game's C++
seam, EEDataText.h and EERules.cpp, linked with the Rust library ee_data through its C API) and run it over every table:
the text must compile to the bytes the Python importer (an independent implementation) makes of it, and the edited
texts of parity.py must give the same bytes or an error in both. The content dumps, the parity and rules cases and a
float sweep are also compared with the digests recorded from the C++ compiler before it was deleted
(cppgate/golden_cpp.json, kept only as digests: no game data): those apply while the data is the one they were
recorded from. The exported text goes to a temporary folder outside the repository and is deleted afterwards.
"""
import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile

import tables

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
GOLDEN = HERE / 'cppgate' / 'golden_cpp.json'


def cargo_build():
    """Builds the Rust side: the C ABI staticlib for the C++ gate, and the float-sweep tool -> their paths."""
    import rust_build
    target = rust_build.TRIPLES[rust_build.host_target()]
    if rust_build.run_cargo(['build', '--release', '--target', target, '-p', 'ee_data_cabi']):
        sys.exit('cargo build of ee_data_cabi failed')
    if rust_build.run_cargo(['build', '--release', '-p', 'ee_data', '--bin', 'ee_data_gate']):
        sys.exit('cargo build of ee_data_gate failed')
    name = 'ee_data_cabi.lib' if sys.platform == 'win32' else 'libee_data_cabi.a'
    tool = 'ee_data_gate.exe' if sys.platform == 'win32' else 'ee_data_gate'
    return rust_build.TARGET_DIR / target / 'release' / name, rust_build.TARGET_DIR / 'release' / tool


def build(out, rust_lib):
    import clang_build
    import verify
    from ee import CLANG, LLD_LINK, lib_paths, RUNTIME_LIBS
    db = ROOT / 'src' / 'Empire Earth' / 'Database'
    out.mkdir(parents=True, exist_ok=True)
    _, inc = verify.include_paths(None)
    objs = []
    for src in (db / 'EERules.cpp', HERE / 'cppgate' / 'eedata_gate.cpp'):
        obj = out / (src.stem + '.obj')
        cmd = [str(CLANG), *clang_build.CLANG_FLAGS, '/std:c++17', '-EHsc', '-MT', '-O1',
               *verify.DEFINES.split(), f'/I{db}', f'/Fo{obj}', str(src)]
        r = subprocess.run(cmd, env=dict(__import__('os').environ, INCLUDE=inc), capture_output=True, text=True)
        if r.returncode:
            sys.exit(f'{src.name}: {r.stdout}{r.stderr}')
        objs.append(str(obj))
    exe = out / 'eedata_gate.exe'
    system = ['advapi32.lib', 'ws2_32.lib', 'userenv.lib', 'ntdll.lib', 'bcrypt.lib', 'synchronization.lib']
    r = subprocess.run([str(LLD_LINK), '/nologo', '/subsystem:console', '/machine:x64', f'/out:{exe}', *objs,
                        str(rust_lib), *system, *lib_paths(), *RUNTIME_LIBS], capture_output=True, text=True)
    if r.returncode:
        sys.exit(f'link: {r.stdout}{r.stderr}')
    return exe


def texts_digest(texts):
    h = hashlib.sha256()
    for k in sorted(texts):
        h.update(k.encode())
        h.update(b'\0')
        h.update(texts[k])
        h.update(b'\0')
    return h.hexdigest()


def sha(b):
    return hashlib.sha256(b).hexdigest()


def run(a, spec, load_db):
    out = pathlib.Path(a.out or ROOT / 'build' / 'eedata-cppgate').resolve()
    rust_lib, rust_tool = cargo_build()
    exe = build(out, rust_lib)
    db = load_db(a, spec)
    golden = json.loads(GOLDEN.read_text())
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
        data_is_golden = texts_digest(texts) == golden['texts']
        if not data_is_golden:
            print('NOTE: the data differs from the one the C++ digests were recorded from: comparing with Python only')
        print("--- text -> the game's seam (Rust compiler) -> the Python importer's .dat", flush=True)
        status |= subprocess.run([str(exe), str(text), str(want)]).returncode
        status |= run_dump(exe, db, text, tmp, out, golden if data_is_golden else None)
        status |= run_parity(exe, spec, texts, tmp / 'parity', text, golden if data_is_golden else None)
        status |= run_rules_parity(exe, texts, tmp / 'rules', text, golden if data_is_golden else None)
        status |= run_floats(rust_tool, tmp, out, golden if data_is_golden else None)
    print('PASS' if status == 0 else 'FAIL')
    return status


def run_dump(exe, db, textdir, tmp, out, golden):
    """G1: the content dump of the binary tables (with the rules' defaults) and of the text tables must be identical,
    and equal the C++ compiler's dump where the digest applies."""
    print('--- content dump: binary .dat vs text (ids resolved), through the game\'s code', flush=True)
    dat = tmp / 'dat'
    dat.mkdir()
    for n, b in db.items():
        (dat / f'{n}.dat').write_bytes(b)
    a, b = out / 'dump_dat.json', out / 'dump_text.json'
    if subprocess.run([str(exe), '--dump-dat', str(dat), str(a)]).returncode or \
            subprocess.run([str(exe), '--dump-text', str(textdir), str(b)]).returncode:
        return 1
    da, db_ = a.read_bytes(), b.read_bytes()
    same = da == db_
    print(f'content dumps {"identical" if same else "DIFFER"} ({a.stat().st_size:,d} bytes)')
    status = 0 if same else 1
    if golden:
        ok = sha(da) == golden['dump_dat'] and sha(db_) == golden['dump_text']
        print(f'dumps vs the C++ compiler\'s digests: {"identical" if ok else "DIFFER"}')
        status |= 0 if ok else 1
    return status


def run_batch(exe, flag, folder, cases, textdir, ext):
    folder.mkdir(parents=True)
    for name, table, text in cases:
        (folder / f'{name}__{table}{ext(table)}').write_bytes(text)
    return subprocess.run([str(exe), flag, str(folder), str(textdir)]).returncode


def outcomes(folder, names):
    out = {}
    for name in names:
        o, e = folder / f'{name}.out', folder / f'{name}.err'
        out[name] = o.read_bytes() if o.exists() else ('E', e.read_bytes())
    return out


def run_parity(exe, spec, texts, folder, textdir, golden):
    """Edited texts: the compiled bytes must equal the Python importer's, or both reject the text (and match the C++
    compiler's digests where they apply)."""
    import parity
    from binfmt import FormatError
    from textio import TextError
    cs = parity.cases(texts)
    want = {}
    for name, table, text in cs:
        try:   # an edited epochs.yaml compiles into dbtechtree
            want[name] = tables.Content(dict(texts, **{table: text}), spec).build('dbtechtree' if table == 'epochs' else table)
        except (TextError, FormatError) as e:
            want[name] = e
    print('--- parity: edited texts, the game\'s seam vs the Python importer', flush=True)
    ext = lambda t: '.yaml' if spec['tables'][t]['format'] == 'yaml' else '.csv'
    if run_batch(exe, '--batch', folder, cs, textdir, ext):
        return 1
    got = outcomes(folder, [c[0] for c in cs])
    bad = errors = gbad = 0
    show = lambda v: f'{len(v)} bytes' if isinstance(v, bytes) else f'error: {v[1]!r}'
    for name, table, _ in cs:
        w, g = want[name], got[name]
        if isinstance(w, bytes) and isinstance(g, bytes):
            ok = w == g
        else:
            ok = not isinstance(w, bytes) and not isinstance(g, bytes)
            errors += ok
        if not ok:
            bad += 1
            print(f'  {name:22s} DIFFERS  Python {f"{len(w)} bytes" if isinstance(w, bytes) else "error: " + str(w)}  |  game {show(g)}')
        if golden:
            digest = ('B' + sha(g)) if isinstance(g, bytes) else ('E' + sha(g[1]))
            if digest != golden['parity'][name]:
                gbad += 1
                print(f'  {name:22s} DIFFERS from the C++ compiler\'s digest ({show(g)})')
    print(f'{len(cs) - bad}/{len(cs)} parity cases agree with Python ({errors} rejected by both, '
          f'{len(cs) - bad - errors} accepted with identical bytes)'
          + (f'; {len(cs) - gbad}/{len(cs)} agree with the C++ compiler\'s digests' if golden else ''))
    return 1 if bad or gbad else 0


def run_rules_parity(exe, texts, folder, textdir, golden):
    """Edited rules documents: each gives a dump or a message; with the digests of the C++ compiler they must agree."""
    import rules_parity
    docs = {n: t for n, t in texts.items() if n in ('combat', 'gameoptions', 'civbuilder', 'civpowers', 'aipersonalities', 'epochs', 'dbcivilization')}
    cs = rules_parity.cases(docs)
    print('--- rules parity: edited documents', flush=True)
    if run_batch(exe, '--rules-batch', folder, cs, textdir, lambda t: '.yaml'):
        return 1
    got = outcomes(folder, [c[0] for c in cs])
    bad = 0
    errors = sum(1 for v in got.values() if not isinstance(v, bytes))
    if golden:
        for name, _, _t in cs:
            g = got[name]
            digest = ('B' + sha(g)) if isinstance(g, bytes) else ('E' + sha(g[1]))
            if digest != golden['rules'][name]:
                bad += 1
                print(f'  {name:22s} DIFFERS from the C++ compiler\'s digest ({g if not isinstance(g, bytes) else len(g)})')
    print(f'{len(cs)} rules cases ({errors} rejected)' + (f'; {len(cs) - bad}/{len(cs)} agree with the C++ compiler\'s digests' if golden else ''))
    return 1 if bad else 0


def float_cases():
    """A sweep of float strings: edge cases and random magnitudes (seeded: the same list each run)."""
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
    return cases


def run_floats(rust_tool, tmp, out, golden):
    """The float sweep: the output (bits and messages) must equal the C++ compiler's digest. It is independent of
    the data, but gated on it with the rest for simplicity of the digest file."""
    cases = float_cases()
    infile = tmp / 'floats.in'
    infile.write_text('\n'.join(cases), encoding='ascii')
    outfile = out / 'floats.txt'
    if subprocess.run([str(rust_tool), '--floats', str(infile), str(outfile)]).returncode:
        return 1
    digest = sha(outfile.read_bytes())
    if golden:
        ok = digest == golden.get('floats')
        print(f'float sweep: {len(cases)} strings; vs the C++ compiler\'s digest: {"identical" if ok else "DIFFERS"}', flush=True)
        if not ok:
            print(f'  sha256 {digest}')
        return 0 if ok else 1
    print(f'float sweep: {len(cases)} strings (sha256 {digest}); data differs, no digest to compare')
    return 0
