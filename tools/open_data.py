"""open_data: the game's data as one loose Data tree (open formats where they exist) and the bundle a playthrough needs.

  python -I tools/open_data.py build OUT [--game DIR] [--base DIR] [--binary] [--force]
        OUT/Data: every file of both data.ssa archives and of both installs' loose Data, merged in the game's search
        order; db tables as text (tools/eedata), models as glTF (tools/eemodel), textures as PNG (tools/eemodel);
        OUT/Users (the install's), OUT/WONLobby.cfg, OUT/zzWONVersion.cfg, OUT/open_data.json (what was built).
        --binary: the same tree with the binary forms (.dat, .cem, .sst/.tga) unconverted (a test variant).
  python -I tools/open_data.py pack OUT.zip [--part all|media|db] [--tree DIR] [--game DIR] [--base DIR]
        the bundle: the open tree (built into build/open_data/tree unless --tree names one), fonts/ (Liberation, with
        its LICENSE and AUTHORS) and README.txt. OUT must be under build/ or outside the repository.
        --part: the releases ship it in parts that unzip into one folder (tools/fetch_data.py pulls them):
          media  everything but Data/db and open_data.json: models, the original textures, sounds, music, scripts,
                 fonts (large; changes rarely)
          db     Data/db, Data/ai and open_data.json: the tables, the rules documents and the computer players'
                 scripts, the tweakables (small; republished whenever the data format or a script changes)
        The third part, Data/Textures-4x, comes from tools/upscale.
  python -I tools/open_data.py ai TREE
        refresh a built tree's Data/ai from the repository's ai/ (the AI manifest ais.json and the next engine's Lua
        scripts; seconds, no rebuild) and record their hash in its open_data.json ("ai"). A build copies them too.
  python -I tools/open_data.py ids TREE
        write a binary tree's Data/db/dbobjects.ids and dbtechtree.ids (the row ids the text tables would carry, from
        its .dat files; the computer players' scripts name classes and techs by them). A --binary build writes them.
  python -I tools/open_data.py check DIR
        counts per folder of a tree or bundle and the rules it must keep (no data.ssa, no dxm, no binary file that has
        an open replacement)

The merge follows the original game's data path (EEShowSinglePlayerScreen; tools/eemodel/sources.py): this install's
Data folder, the original game's Data folder, this install's data.ssa, the original game's data.ssa. The first place
that has a name (compared without case) wins. The original game's folder therefore beats this install's archive; the
build reports every such clash. Left out: Data\\dxm (a community mod), the data.ssa archives themselves, Bink movies
(.bik; tools/make_play_dir.py --movies converts them) and the original game's own campaigns (its Data\\Campaigns:
Art of Conquest never lists them; merged in, they would appear as custom campaigns). The campaign archives (Data\\Campaigns\\*.ssa) stay archives: a
campaign is a package by design (its name, description, author, password and required civilization are archive tags,
the campaign editor writes .ssa files, built-in campaigns are checked against the archive's checksum and the progress
files are keyed by it).

--game defaults to $EE_GAME_DIR, else the Art of Conquest install; --base to $EE_BASE_GAME_DIR, else the original
game's install. Both are only read. Game data never goes into the repository outside build/.
"""
import argparse
import collections
import json
import os
import pathlib
import shutil
import sys
import time
import zipfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools' / 'eemodel'))
sys.path.insert(0, str(ROOT / 'tools' / 'eedata'))
import sources   # noqa: E402  (tools/eemodel: the game's data path)
import ssa       # noqa: E402

TREE = ROOT / 'build' / 'open_data' / 'tree'
AI_SCRIPTS = ROOT / 'ai'   # the next engine's Lua scripts (docs/AI_LUA_PLAN.md 3.2): written here, not retail data


def converters_hash():
    """A hash of the code that decides a tree's content (this tool, eedata, eemodel): a tree built by other code is
    stale (e.g. the db text format changed) and the harness rebuilds it."""
    import hashlib
    h = hashlib.sha256()
    for p in sorted([pathlib.Path(__file__).resolve(), *(ROOT / 'tools' / 'eedata').glob('*.py'),
                     *(ROOT / 'tools' / 'eedata').glob('*.json'), *(ROOT / 'tools' / 'eemodel').glob('*.py')]):
        h.update(p.name.encode() + b'\0' + p.read_bytes().replace(b'\r\n', b'\n'))
    return h.hexdigest()[:16]


AI_NOTICES = ('LICENSE', 'UPSTREAM', 'README', 'NOTICE', 'COPYING')   # a ported AI's notices ship with its scripts


def ai_files():
    """The repository's ai/ files a data tree gets: the AI manifest ais.json (the lobby's AIs, ee/EECPEngine.h), every
    Lua script, and the notice files a ported AI ships (ai/ageofai/LICENSE, UPSTREAM: the GPL-3.0 text and the
    upstream pin). The game's own AI folder hash (EECPAIFolderHash, the multiplayer checksum) covers ais.json and the
    scripts only; the notices change no behaviour."""
    notices = [p for p in AI_SCRIPTS.rglob('*') if p.is_file() and p.name.split('.')[0].upper() in AI_NOTICES]
    return sorted([*AI_SCRIPTS.rglob('*.lua'), *([AI_SCRIPTS / 'ais.json'] if (AI_SCRIPTS / 'ais.json').exists() else []),
                   *notices])


def scripts_hash():
    """A hash of the repository's ai/ files (paths and contents): a tree whose open_data.json records another one
    has stale scripts (refresh_scripts copies them in seconds; the converters' hash leaves them out)."""
    import hashlib
    h = hashlib.sha256()
    for p in ai_files():
        h.update(p.relative_to(AI_SCRIPTS).as_posix().encode() + b'\0' + p.read_bytes().replace(b'\r\n', b'\n'))
    return h.hexdigest()[:16]


def refresh_scripts(tree):
    """Data/ai of a tree replaced by the repository's ai/ (ais.json, *.lua, the notices), its hash recorded in open_data.json -> the hash.
    The old files are unlinked first, so a staged copy's hard links into another tree are never written through."""
    tree = pathlib.Path(tree)
    target = tree / 'Data' / 'ai'
    if target.exists():
        shutil.rmtree(target)
    for p in ai_files():
        dst = target / p.relative_to(AI_SCRIPTS)
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_bytes(p.read_bytes().replace(b'\r\n', b'\n'))
    digest = scripts_hash()
    meta = tree / 'open_data.json'
    if meta.exists():
        report = json.loads(meta.read_text())
        if report.get('ai') != digest:
            report['ai'] = digest
            meta.write_bytes((json.dumps(report, indent=2) + '\n').encode())
    return digest


FONTS = ROOT / 'toolchain' / 'fonts'
SKIP_DIRS = {'dxm'}                       # Data\dxm: a community mod
# the original game's own campaigns: Art of Conquest never lists them (its campaign screen lists its own
# Data\Campaigns; the original's were reachable only by name), merged in they would appear as custom campaigns
BASE_ONLY_SKIP = {'campaigns'}
SKIP_EXT = {'.bik'}                      # movies: converted by make_play_dir.py --movies
ROOT_FILES = ['WONLobby.cfg', 'zzWONVersion.cfg']
# empty folders the game expects (it creates the user folders itself too)
EMPTY_DIRS = ['Saved Games', 'Scenarios', 'Unit AI Scripts', 'Movies', 'Music']

README = """Empire Earth: The Art of Conquest - open data bundle
====================================================

Derived from the retail game.

What this is
  The data a playthrough of the empire_earth_next build of Empire Earth: The Art of Conquest needs, as one loose
  Data folder: the contents of both retail data.ssa archives (Art of Conquest and the original Empire Earth) and
  the original game's loose data, merged in the game's own search order. The database tables are text (CSV/YAML),
  the models glTF 2.0 and the textures PNG; sounds (.wav, .mp3), unit AI scripts (.tai), random map scripts (.rmv),
  civilizations (.civ), the music (DirectMusic .sgt/.sty/.dls) and the campaign packages (.ssa) are as shipped.
  Neither a data.ssa archive nor an installation of the original Empire Earth is needed.

How to use it
  Unpack it next to the game program (EE-AOC.exe on Windows, ee-aoc on Linux/macOS) so that the program, Data and
  Users are in one folder, and run the program. The game keeps its settings in config.yaml in that folder (it
  creates the file). On Linux/macOS the data may also live elsewhere: set EE_DATA_ROOT to the unpacked folder.
  tools/make_play_dir.py in the source repository prepares such a folder from this bundle.

Not included
  Movies (no .bik on the source installation; make_play_dir.py --movies converts them from a retail disc).

Contents
  Data/            the game data (Data/db: tables as text; Data/Models: glTF; Data/Textures: PNG)
  Users/           the default user folder
  fonts/           Liberation fonts 2.1.5 (SIL Open Font License, see fonts/LICENSE), the build's text fonts
  WONLobby.cfg, zzWONVersion.cfg   the online lobby's configuration (the WON services no longer exist)
  open_data.json   what the bundle was built from
"""


README_MEDIA = """
This part
  The media part: everything above but Data/db and open_data.json, which are the db part (ee_open_db-<sha>.zip,
  small, republished whenever the tables or rules change). The 4x textures (ee_textures_4x-<sha>.zip,
  Data/Textures-4x) are the third, optional part. Unzip the parts into one folder; tools/fetch_data.py in the source
  repository does that.
"""

README_DB = """
This part
  The db part: Data/db (the tables as text and the rules documents, what a mod tweaks), Data/ai (the computer
  players' Lua scripts) and open_data.json. It needs
  the media part (ee_open_media-<sha>.zip: models, textures, sounds, fonts) in the same folder: unzip this one over
  it (tools/fetch_data.py in the source repository does that).
"""


def outside_repo(p):
    p = pathlib.Path(p).resolve()
    if p.is_relative_to(ROOT) and not p.is_relative_to(ROOT / 'build'):
        sys.exit(f'{p} is inside the repository: game data only goes under build/ or outside it')
    for d in (sources.AOC, sources.BASE):
        if p == d.resolve() or p.is_relative_to(d.resolve()):
            sys.exit(f'{p} is inside the game install {d}: the installs are only read')
    return p


class Place:
    """One place on the data path: a loose Data folder or a data.ssa archive. names: {lower name: name as stored}."""

    def __init__(self, label, folder=None, archive=None, skip=()):
        self.label, self.folder, self.archive = label, folder, archive
        self.names = {}
        self.dirs = []   # a loose folder's subfolders (also the empty ones), as spelled
        self.tags = []
        if archive is not None:
            entries, self.tags, self._data = ssa.read_ssa(archive)
            self._entries = entries
            self.names = {n.lower(): n for n in entries}
        else:
            for dirpath, dirnames, filenames in os.walk(folder):
                rel = pathlib.Path(dirpath).relative_to(folder)
                if rel.parts and (rel.parts[0].lower() in SKIP_DIRS or rel.parts[0].lower() in skip):
                    dirnames[:] = []
                    continue
                if rel.parts:
                    self.dirs.append('\\'.join(rel.parts))
                for f in filenames:
                    name = '\\'.join(rel.parts + (f,))
                    if name.lower() == 'data.ssa' or pathlib.Path(f).suffix.lower() in SKIP_EXT:
                        continue
                    self.names[name.lower()] = name

    def read(self, name):
        if self.archive is not None:
            return ssa.entry_bytes(self._data, *self._entries[self.names[name]])
        return (self.folder / self.names[name].replace('\\', os.sep)).read_bytes()


def places(game, base):
    dirs = sources.game_dirs(game, base)
    out = [Place(f'{d.name}\\Data', folder=d / 'Data', skip=BASE_ONLY_SKIP if i else ()) for i, d in enumerate(dirs)]
    for d in dirs:
        if (d / 'Data' / 'data.ssa').is_file():
            out.append(Place(f'{d.name}\\Data\\data.ssa', archive=d / 'Data' / 'data.ssa'))
    return dirs, out


def canonical_dirs(places_):
    """{lower folder path: its spelling}: the loose folders' spelling first (Models, Sounds), else Title Case for a
    folder only the archives have (civilizations -> Civilizations), so a case-sensitive file system gets one folder"""
    out = {}
    for p in sorted(places_, key=lambda p: p.folder is None):   # every loose folder before any archive
        for name in [d + '\\-' for d in p.dirs] + list(p.names.values()):
            parts = name.split('\\')[:-1]
            for i in range(len(parts)):
                key = '\\'.join(parts[:i + 1]).lower()
                if key not in out:
                    spelled = parts[i] if p.folder is not None else parts[i].title() if parts[i].islower() else parts[i]
                    out[key] = (out['\\'.join(parts[:i]).lower()] + '\\' if i else '') + spelled
    return out


def spell(name, dirs):
    parts = name.split('\\')
    if len(parts) == 1:
        return name
    return dirs['\\'.join(parts[:-1]).lower()] + '\\' + parts[-1]


ID_TABLES = ('dbobjects', 'dbtechtree')   # the tables whose row ids the game reads from a binary tree (EEDbRowIds)


def write_row_ids(tree):
    """A binary tree's Data/db/<table>.ids for ID_TABLES: `key id` lines, the ids tools/eedata gives the rows when it
    converts the tables (ids.generate), so the next engine's scripts name the same classes and techs on both trees
    (docs/AI_RESEARCH.md 8.36). -> {table: rows}"""
    import ids as idsmod
    import tables
    spec = db_tables()
    db = pathlib.Path(tree) / 'Data' / 'db'
    dbs = {t: (db / f'{t}.dat').read_bytes() for t in ID_TABLES}
    generated = idsmod.generate(tables.parse_all(dbs, spec), spec)
    out = {}
    for t in ID_TABLES:
        rows = sorted(generated[t].items())
        head = f'# {t}: the row ids of the .dat (tools/open_data.py ids; key id)'
        text = '\n'.join([head] + [f'{k} {v}' for k, v in rows]) + '\n'
        (db / f'{t}.ids').write_bytes(text.encode('ascii'))
        out[t] = len(rows)
    return out


def db_tables():
    spec = json.loads((ROOT / 'tools' / 'eedata' / 'specs.json').read_text(encoding='utf-8'))
    return spec


def cmd_build(a, out=None):
    out = outside_repo(out or a.out)
    if out.exists():
        if not a.force:
            sys.exit(f'{out} exists: pass --force to replace it')
        if not (out / 'open_data.json').exists():
            sys.exit(f'{out} exists and was not made by this tool (no open_data.json): refusing to delete it')
        shutil.rmtree(out)
    t0 = time.time()
    dirs, plcs = places(a.game, a.base)
    if len(dirs) < 2:
        print(f'warning: only {[str(d) for d in dirs]} found: the tree lacks the other install\'s data', flush=True)
    for p in plcs:
        print(f'  {p.label}: {len(p.names)} files', flush=True)
    winner = {}
    clashes = collections.Counter()
    for p in plcs:
        for low in p.names:
            if low in winner:
                clashes[(winner[low].label, p.label)] += 1
            else:
                winner[low] = p
    spelled = canonical_dirs(plcs)
    data = out / 'Data'
    data.mkdir(parents=True)
    report = dict(made=time.strftime('%Y-%m-%d %H:%M:%S'), binary=bool(a.binary), converters=converters_hash(),
                  sources=[dict(place=p.label, files=len(p.names), tags=dict(p.tags)) for p in plcs],
                  shadowed={f'{k[1]} under {k[0]}': v for k, v in sorted(clashes.items())})
    converted = set()   # lower names replaced by an open form
    if not a.binary:
        # db tables -> text (tools/eedata): the 37 tables with a text form; the two without stay .dat
        import tables
        spec = db_tables()
        (data / 'db').mkdir()
        dbs = {}
        for t in tables.binary_tables(spec):
            name = f'db\\{t}.dat'
            if name not in winner:
                sys.exit(f'{name} is on no data path place')
            dbs[t] = winner[name].read(name)
        # all at once: the tables reference each other's rows by id (DATA_FORMATS.md 8.6)
        for t, text in tables.export_tree(dbs, spec).items():   # + the rules documents (eedata rules.py)
            (data / 'db' / f'{t}{tables.text_ext(spec, t)}').write_bytes(text)
            converted.add(f'db\\{t}.dat')
        print(f'db: {len(spec["tables"])} tables as text', flush=True)
        # models -> glTF, textures -> PNG (tools/eemodel export: the same data path, resolved as the game does)
        import eemodel
        ns = argparse.Namespace(game=a.game, base=a.base, names=None)
        log = []
        real_print = print

        def capture(*args, **kw):
            log.append(' '.join(str(x) for x in args))
            real_print(*args, **kw)
        eemodel.print = capture
        eemodel.cmd_export(ns, out=data)
        eemodel.print = real_print
        skipped = [ln for ln in log if ln.startswith('skipped ')]
        report['models_skipped'] = skipped
        models = {p.stem.lower() for p in (data / 'Models').glob('*.gltf')}
        pngs = {p.stem.split('@')[0].lower() for p in (data / 'Textures').glob('*.png')}
        for low in winner:
            stem = low.rsplit('\\', 1)[-1].rsplit('.', 1)[0]
            if low.startswith('models\\') and low.endswith('.cem') and '\\' not in low[7:] and stem in models:
                converted.add(low)
            if low.startswith('textures\\') and low.endswith(('.tga', '.sst')) and '\\' not in low[9:] and stem in pngs:
                converted.add(low)
        report['models_gltf'] = len(models)
        report['textures_png'] = len(pngs)
    copied = collections.Counter()
    for low, p in sorted(winner.items()):
        if low in converted:
            continue
        target = data / spell(p.names[low], spelled).replace('\\', os.sep)
        target.parent.mkdir(parents=True, exist_ok=True)
        if p.folder is not None:
            shutil.copy2(p.folder / p.names[low].replace('\\', os.sep), target)
        else:
            target.write_bytes(p.read(low))
        copied[pathlib.Path(low).suffix or '(none)'] += 1
    for d in EMPTY_DIRS:
        (data / d).mkdir(exist_ok=True)
    report['ai'] = refresh_scripts(out)   # the next engine's scripts (the repository's, not retail data)
    if a.binary:
        report['ids'] = write_row_ids(out)   # the scripts' names for the .dat rows (EEDbRowIds)
    report['copied'] = dict(sorted(copied.items()))
    game = dirs[0]
    shutil.copytree(game / 'Users', out / 'Users')
    for f in ROOT_FILES:
        if (game / f).is_file():
            shutil.copy2(game / f, out / f)
    report['files'] = sum(1 for _ in out.rglob('*') if _.is_file()) + 1
    (out / 'open_data.json').write_bytes((json.dumps(report, indent=2) + '\n').encode())
    for k, v in report['shadowed'].items():
        print(f'  shadowed: {v} files of {k}', flush=True)
    print(f'tree: {report["files"]} files in {time.time() - t0:.0f} s -> {out}', flush=True)
    return out


def folder_counts(root):
    """{top folder: (files, bytes)} of a tree"""
    out = collections.defaultdict(lambda: [0, 0])
    for p in pathlib.Path(root).rglob('*'):
        if p.is_file():
            rel = p.relative_to(root).parts
            key = '/'.join(rel[:2]) if rel[0] == 'Data' and len(rel) > 2 else rel[0] if len(rel) > 1 else '.'
            out[key][0] += 1
            out[key][1] += p.stat().st_size
    return dict(sorted(out.items()))


def check_tree(root):
    """the bundle's rules -> list of problems"""
    root = pathlib.Path(root)
    problems = []
    data = root / 'Data'
    if not (data / 'db').is_dir():
        problems.append('no Data/db')
    for p in root.rglob('*'):
        rel = p.relative_to(root)
        low = [x.lower() for x in rel.parts]
        if 'dxm' in low:
            problems.append(f'{rel}: the dxm mod')
        if p.is_file():
            if p.name.lower() == 'data.ssa':
                problems.append(f'{rel}: a data.ssa archive')
            if p.suffix.lower() == '.bik':
                problems.append(f'{rel}: a Bink movie')
            stem = p.stem.lower()
            sib = p.with_suffix
            if p.suffix.lower() == '.cem' and (sib('.gltf').exists() or sib('.glb').exists()):
                problems.append(f'{rel}: a .cem next to its glTF')
            if p.suffix.lower() in ('.sst', '.tga') and sib('.png').exists() and low[:2] == ['data', 'textures']:
                problems.append(f'{rel}: a texture next to its PNG')
            if p.suffix.lower() == '.dat' and (sib('.csv').exists() or sib('.yaml').exists()):
                problems.append(f'{rel}: a db table next to its text')
            del stem
    return problems


def cmd_check(a):
    root = pathlib.Path(a.dir)
    if root.suffix.lower() == '.zip':
        with zipfile.ZipFile(root) as z:
            names = z.namelist()
            c = collections.Counter()
            for n in names:
                parts = n.split('/')
                c['/'.join(parts[:2]) if parts[0] == 'Data' and len(parts) > 2 else parts[0] if len(parts) > 1 else '.'] += 1
            for k, v in sorted(c.items()):
                print(f'  {k}: {v} files')
            print(f'{len(names)} files, {root.stat().st_size / 1e6:.1f} MB')
        return 0
    for k, (n, b) in folder_counts(root).items():
        print(f'  {k}: {n} files, {b / 1e6:.1f} MB')
    problems = check_tree(root)
    for p in problems[:50]:
        print('PROBLEM', p)
    print(f'{len(problems)} problems')
    return 1 if problems else 0


def pack_part(rel, part):
    """whether a tree file or folder (its path relative to the tree) belongs to the --part"""
    if part == 'all':
        return True
    is_db = rel.parts[:2] in (('Data', 'db'), ('Data', 'ai')) or rel.as_posix() == 'open_data.json'
    return (part == 'db') == is_db


def cmd_pack(a):
    zip_path = outside_repo(a.out)
    part = a.part
    if a.tree:
        tree = pathlib.Path(a.tree)
    else:
        tree = TREE
        if not (tree / 'open_data.json').exists() or json.loads((tree / 'open_data.json').read_text())['binary']:
            a.force, a.binary = True, False
            cmd_build(a, out=tree)
    problems = check_tree(tree)
    if problems:
        sys.exit('the tree breaks the bundle rules:\n  ' + '\n  '.join(problems[:20]))
    if not (FONTS / 'LICENSE').exists():
        sys.exit(f'{FONTS} has no fonts: python -I tools/fetch_posix.py fonts')
    zip_path.parent.mkdir(parents=True, exist_ok=True)
    tmp = zip_path.with_name(zip_path.name + '.part')
    t0 = time.time()
    n = 0
    with zipfile.ZipFile(tmp, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for p in sorted(tree.rglob('*')):
            rel = p.relative_to(tree)
            if not pack_part(rel, part):
                continue
            if p.is_file():
                z.write(p, rel.as_posix())
                n += 1
            elif p.is_dir() and not any(p.iterdir()):
                z.write(p, rel.as_posix() + '/')   # an empty folder the game expects
        if part != 'db':
            for p in sorted(FONTS.iterdir()):
                if p.is_file() and not p.name.startswith('.'):
                    z.write(p, 'fonts/' + p.name)
                    n += 1
        readme = README + {'all': '', 'media': README_MEDIA, 'db': README_DB}[part]
        z.writestr('README.txt' if part == 'all' else f'README-{part}.txt', readme.replace('\n', '\r\n'))
        n += 1
    tmp.replace(zip_path)
    print(f'{zip_path}: {n} files, {zip_path.stat().st_size / 1e6:.1f} MB in {time.time() - t0:.0f} s')
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__)
    sub = ap.add_subparsers(dest='cmd', required=True)
    for name in ('build', 'pack'):
        p = sub.add_parser(name)
        p.add_argument('out')
        p.add_argument('--game')
        p.add_argument('--base')
        if name == 'build':
            p.add_argument('--binary', action='store_true')
            p.add_argument('--force', action='store_true')
        else:
            p.add_argument('--tree')
            p.add_argument('--part', choices=('all', 'media', 'db'), default='all',
                           help='media: all but Data/db; db: Data/db and open_data.json (default all)')
    p = sub.add_parser('check')
    p.add_argument('dir')
    p = sub.add_parser('ai')
    p.add_argument('tree')
    p = sub.add_parser('ids')
    p.add_argument('tree')
    a = ap.parse_args()
    if a.cmd == 'build':
        cmd_build(a)
        return 0
    if a.cmd == 'ai':
        if not (pathlib.Path(a.tree) / 'Data').is_dir():
            sys.exit(f'{a.tree}: no Data folder')
        print(f'{a.tree}: Data/ai from {AI_SCRIPTS} ({refresh_scripts(a.tree)})')
        return 0
    if a.cmd == 'ids':
        if not (pathlib.Path(a.tree) / 'Data' / 'db' / 'dbobjects.dat').is_file():
            sys.exit(f'{a.tree}: no Data/db/dbobjects.dat (a binary tree)')
        print(f'{a.tree}: {write_row_ids(a.tree)}')
        return 0
    return {'pack': cmd_pack, 'check': cmd_check}[a.cmd](a)


if __name__ == '__main__':
    sys.exit(main())
