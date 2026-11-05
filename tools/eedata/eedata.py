"""eedata: converter between the game's binary db tables and text (CSV / YAML): the fields the game reads.

  python -I tools/eedata/eedata.py unpack [--ssa PATH] OUT          data.ssa -> OUT/<table>.dat (the 39 db files)
  python -I tools/eedata/eedata.py export [--ssa PATH | --db DIR] OUT [TABLE ...]
                                                                   db -> OUT/<table>.csv|.yaml
  python -I tools/eedata/eedata.py import TEXTDIR OUT [TABLE ...]   text -> OUT/<table>.dat
  python -I tools/eedata/eedata.py install [--ssa PATH] GAMEDIR
                                    write the text tables into GAMEDIR/Data/db of a game install COPY (for tests)
  python -I tools/eedata/eedata.py check [--ssa PATH | --db DIR] [--text DIR]
                                    the gates: (a) text -> db -> text round trip of every table, (b) every value
                                    the game reads equals retail, (c) record sizes vs the sources' static_asserts,
                                    specs up to date
  python -I tools/eedata/eedata.py holes [--ssa PATH | --db DIR]   print the retail hole rows' values (overlay.HOLES)
  python -I tools/eedata/eedata.py census [-j N]                  list every read of every record member (compiles
                                    every unit syntax-only; slow): the evidence for the unused fields
  python -I tools/eedata/eedata.py specs [--check]                  regenerate tools/eedata/specs.json
  python -I tools/eedata/eedata.py cppgen [--check]                 regenerate the C++ spec header
  python -I tools/eedata/eedata.py cppgate [--ssa PATH | --db DIR] [--out DIR] [-j N]
                                    build the C++ text compiler test and run it over every exported table

Game data is never written into the repository. --ssa defaults to $EE_DATA_SSA, else the AoC install's
Data/data.ssa. The text carries only what the game reads: fields nothing reads are left out (the importer writes
zero or the spec's default), empty database rows (holes) are left out (the importer writes the table's hole record),
and junk bytes (after a string's NUL, padding, the converter's 0xCC fill) are gone. The files therefore differ from
retail, values the game reads do not.
"""
import argparse
import os
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import binfmt     # noqa: E402
import specgen    # noqa: E402
import ssa        # noqa: E402
import tables     # noqa: E402
from textio import TextError   # noqa: E402

ROOT = HERE.parent.parent
DEFAULT_SSA = pathlib.Path(r'C:\Program Files (x86)\Empire Earth\Empire Earth - The Art of Conquest\Data\data.ssa')


def ssa_path(a):
    return pathlib.Path(getattr(a, 'ssa', None) or os.environ.get('EE_DATA_SSA') or DEFAULT_SSA)


def load_db(a, spec, names=None):
    """{table: bytes} from --db DIR (loose .dat files) or the archive."""
    names = names or sorted(spec['tables'])
    out = {}
    if getattr(a, 'db', None):
        for n in names:
            out[n] = (pathlib.Path(a.db) / f'{n}.dat').read_bytes()
        return out
    entries, _, d = ssa.read_ssa(ssa_path(a))
    for n in names:
        key = f'db\\{n}.dat'
        if key not in entries:
            sys.exit(f'{key} is not in {ssa_path(a)}')
        out[n] = ssa.entry_bytes(d, *entries[key])
    return out


def ext(spec, name):
    return '.yaml' if spec['tables'][name]['format'] == 'yaml' else '.csv'


def outside_repo(p):
    p = pathlib.Path(p).resolve()
    if p.is_relative_to(ROOT):
        sys.exit(f'{p} is inside the repository: game data never goes there')
    return p


def cmd_unpack(a, spec):
    out = outside_repo(a.out)
    out.mkdir(parents=True, exist_ok=True)
    for n, b in load_db(a, spec).items():
        (out / f'{n}.dat').write_bytes(b)
    print(f'{len(spec["tables"])} db files -> {out}')


def cmd_export(a, spec):
    out = outside_repo(a.out)
    out.mkdir(parents=True, exist_ok=True)
    for n, b in load_db(a, spec, a.tables).items():
        (out / f'{n}{ext(spec, n)}').write_bytes(tables.export(n, b, spec))
    print(f'{len(a.tables or spec["tables"])} tables -> {out}')


def cmd_import(a, spec):
    src = pathlib.Path(a.text)
    out = outside_repo(a.out)
    out.mkdir(parents=True, exist_ok=True)
    bad = 0
    for n in a.tables or sorted(spec['tables']):
        p = src / f'{n}{ext(spec, n)}'
        if not p.exists():
            continue
        try:
            (out / f'{n}.dat').write_bytes(tables.import_(n, p.read_bytes(), spec))
        except (TextError, binfmt.FormatError) as e:
            print(f'{p}: {e}')
            bad += 1
    sys.exit(1 if bad else 0)


def cmd_install(a, spec):
    game = outside_repo(a.gamedir)
    if game == DEFAULT_SSA.parent.parent.resolve():
        sys.exit('refusing the retail install: copy it and install into the copy')
    if not (game / 'Data').is_dir():
        sys.exit(f'{game}: no Data folder (not a game install)')
    out = game / 'Data' / 'db'
    out.mkdir(exist_ok=True)
    for n, b in load_db(a, spec).items():
        (out / f'{n}{ext(spec, n)}').write_bytes(tables.export(n, b, spec))
    print(f'{len(spec["tables"])} text tables -> {out}')


def cmd_holes(a, spec):
    import check
    for n, b in load_db(a, spec).items():
        v = check.hole_values(n, b, spec)
        if v:
            print(f'    {n!r}: {v!r},')


def cmd_specs(a, spec):
    new = specgen.dumps(specgen.generate())
    if a.check:
        ok = specgen.SPECS.read_bytes() == new
        print('specs.json is ' + ('up to date' if ok else 'STALE: run eedata.py specs'))
        sys.exit(0 if ok else 1)
    specgen.SPECS.write_bytes(new)
    print(f'-> {specgen.SPECS}')


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__)
    sub = ap.add_subparsers(dest='cmd', required=True)

    def src(p):
        p.add_argument('--ssa', help='data.ssa (default $EE_DATA_SSA or the AoC install)')
        p.add_argument('--db', help='a folder of loose db .dat files instead of the archive')

    p = sub.add_parser('unpack')
    p.add_argument('--ssa')
    p.add_argument('out')
    p = sub.add_parser('export')
    src(p)
    p.add_argument('out')
    p.add_argument('tables', nargs='*')
    p = sub.add_parser('import')
    p.add_argument('text')
    p.add_argument('out')
    p.add_argument('tables', nargs='*')
    p = sub.add_parser('install')
    src(p)
    p.add_argument('gamedir')
    p = sub.add_parser('holes')
    src(p)
    p = sub.add_parser('census')
    p.add_argument('-j', type=int, default=2)
    p.add_argument('--out', help='write the use sites here (TSV)')
    p = sub.add_parser('check')
    src(p)
    p.add_argument('--text', help='also write the exported text here (outside the repo)')
    p = sub.add_parser('specs')
    p.add_argument('--check', action='store_true')
    p = sub.add_parser('cppgen')
    p.add_argument('--check', action='store_true')
    p = sub.add_parser('cppgate')
    src(p)
    p.add_argument('--out', help='build/output folder (default build/eedata-cppgate)')
    p.add_argument('-j', type=int, default=2)
    a = ap.parse_args()
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    spec = specgen.load()
    if a.cmd == 'check':
        import check
        sys.exit(check.run(a, spec, load_db))
    if a.cmd == 'cppgen':
        import cppgen
        sys.exit(cppgen.run(a, spec))
    if a.cmd == 'census':
        import census
        sys.exit(census.run(a, spec))
    if a.cmd == 'cppgate':
        import cppgate
        sys.exit(cppgate.run(a, spec, load_db))
    {'unpack': cmd_unpack, 'export': cmd_export, 'import': cmd_import, 'install': cmd_install,
     'specs': cmd_specs, 'holes': cmd_holes}[a.cmd](a, spec)


if __name__ == '__main__':
    main()
