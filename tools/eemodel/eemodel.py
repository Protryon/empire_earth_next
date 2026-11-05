"""eemodel: the game's models (.cem) and textures (.sst/.tga) <-> open formats (glTF 2.0, PNG).

  python -I tools/eemodel/eemodel.py export [--game DIR] [--base DIR] OUT [NAME ...]
        every model and texture the game finds (or the named ones, e.g. models\\air_a10_10.cem) -> OUT/Models/<stem>.gltf
        + .bin, OUT/Textures/<stem>.png (+ <stem>@<frame>.png)
  python -I tools/eemodel/eemodel.py convert IN OUT
        one file: .cem -> .gltf/.glb, .gltf/.glb -> .cem, .sst/.tga -> .png, .png -> .sst/.tga (by OUT's extension)
  python -I tools/eemodel/eemodel.py check [--game DIR] [--base DIR] [--only models|textures] [--limit N]
        the gates over every .cem/.sst/.tga on the game's data path (loose files and both data.ssa archives)
  python -I tools/eemodel/eemodel.py validate FILE.gltf|.glb ...      the structural glTF 2.0 checks
  python -I tools/eemodel/eemodel.py render IN OUT.png [--frame N] [--textures DIR [--texture STEM]]
        a software rendering of a .cem (the game's transform order) or a .gltf/.glb (glTF node semantics)
  python -I tools/eemodel/eemodel.py install [--game DIR] [--base DIR] GAMEDIR
        write the open-format models and textures into GAMEDIR/Data (a COPY of the game; refuses the installs and
        the repository outside build/)
  python -I tools/eemodel/eemodel.py cppgate [--out DIR]
        build the C++ loaders' test (tools/eemodel/cppgate) and run it over every exported model and texture

Game data is never written into the repository (outside build/). --game defaults to $EE_GAME_DIR, else the AoC
install; --base to $EE_BASE_GAME_DIR, else the original game's install (the AoC exe searches its data second).
"""
import argparse
import os
import pathlib
import re
import sys
import tempfile
import time

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(HERE))
import cem        # noqa: E402
import gltf       # noqa: E402
import legacy     # noqa: E402
import sources    # noqa: E402
import sst        # noqa: E402
import validate   # noqa: E402


def texture_map(srcs):
    """{model name ("models\\air_catalina_11"): texture name ("textures\\air_catalina_11t")} from dbgraphics (the
    texture the game draws a unit's model with; the model's own texture names are not read)."""
    sys.path.append(str(ROOT / 'tools' / 'eedata'))
    import json
    import tables
    src = sources.resolve(srcs, 'db\\dbgraphics.dat')
    if src is None:
        return {}
    spec = json.loads((ROOT / 'tools' / 'eedata' / 'specs.json').read_text(encoding='utf-8'))
    text = tables.export('dbgraphics', src.read('db\\dbgraphics.dat'), spec).decode('utf-8')
    out = {}
    for rec in text.split('\n  - row:')[1:]:
        def field(k):
            mo = re.search(r'^    ' + k + r': "(.*)"$', rec, re.M)
            return mo.group(1).replace('\\\\', '\\') if mo else ''

        def arr(k):
            mo = re.search(r'^    ' + k + r': \[(.*)\]$', rec, re.M)
            return [x.replace('\\\\', '\\') for x in re.findall(r'"((?:[^"\\]|\\.)*)"', mo.group(1))] if mo else []
        pairs = [(field('mFileName'), field('mTextureName'))] + list(zip(arr('mModelNames'), arr('mTextureNames')))
        for mname, tname in pairs:
            if mname and tname:
                out.setdefault(mname.lower(), tname)
    return out


def _stem(name):
    return name.replace('\\', '/').rsplit('/', 1)[-1].rsplit('.', 1)[0]


def model_to_gltf(root, tex_for_model=None):
    """The glTF of a model; its images show the texture the game draws it with (dbgraphics) when known."""
    return gltf.export(root, display_texture=_stem(tex_for_model).lower() if tex_for_model else None)


# ---------------------------------------------------------------------------------------------------------- check --

class Tally:
    def __init__(self):
        self.counts = {}
        self.examples = {}

    def add(self, key, example=None):
        self.counts[key] = self.counts.get(key, 0) + 1
        if example is not None:
            self.examples.setdefault(key, [])
            if len(self.examples[key]) < 5:
                self.examples[key].append(example)

    def report(self, title):
        print(f'--- {title}')
        for k in sorted(self.counts):
            ex = self.examples.get(k)
            print(f'  {self.counts[k]:6}  {k}' + (f'   e.g. {", ".join(ex)}' if ex else ''))


def _game_view_diff(a, b):
    """The fields the game reads that differ between two model trees (strings up to their NUL) -> sorted names."""
    out = set()
    if sum(1 for _ in cem.walk(a)) != sum(1 for _ in cem.walk(b)):
        return ['sub-models']
    for (x, d), (y, _) in zip(cem.walk(a), cem.walk(b)):
        if cem.cstr(x.name) != cem.cstr(y.name):
            out.add('name')
        if x.center != y.center:
            out.add('center')
        if len(x.lods) != len(y.lods):
            out.add('LOD count')
        elif any(p != q for p, q in zip(x.lods, y.lods)):
            out.add('LOD indices')
        if x.polygon_count != y.polygon_count:
            out.add('polygon count')
        if [cem.cstr(t) for t in x.tag_names] != [cem.cstr(t) for t in y.tag_names]:
            out.add('tag names')
        if len(x.materials) != len(y.materials):
            out.add('material count')
        else:
            for p, q in zip(x.materials, y.materials):
                for k in ('texture_index', 'vertex_start', 'vertex_count'):
                    if getattr(p, k) != getattr(q, k):
                        out.add('material ' + k)
                if p.ranges[:1] != q.ranges[:1]:
                    out.add('material LOD 0 range')
                for k in ('name', 'texture'):
                    if cem.cstr(getattr(p, k)) != cem.cstr(getattr(q, k)):
                        out.add('material ' + k)
        if len(x.frames) != len(y.frames):
            out.add('frame count')
        else:
            for p, q in zip(x.frames, y.frames):
                for k in ('radius', 'vertices', 'tags', 'transform', 'bbox'):
                    if getattr(p, k).tobytes() != getattr(q, k).tobytes():
                        out.add('frame ' + k)
    return sorted(out)


def check_models(srcs, limit, tally):
    status = 0
    n = 0
    for src in srcs:
        for name in src.names('models\\'):
            if not name.endswith('.cem'):
                continue
            if limit and n >= limit:
                return status
            n += 1
            data = src.read(name)
            where = f'{src.label}:{name}'
            try:
                m = cem.read(data)
            except cem.CemError as e:
                tally.add(f'models: not a model the game can load ({e})', where)
                continue
            tally.add(f'models: read, version {m.major}.{m.minor}')
            canon = cem.write(m, exact=False)
            if m.major == 2:
                if cem.write(m) == data:
                    tally.add('models: (a) reader/writer byte-identical to the file')
                else:
                    tally.add('models: (a) FAIL reader/writer differs from the file', where)
                    status = 1
                if canon != data:
                    tally.add('models: (a) canonical 2.0 form drops unread bytes (string tails, trailing data)', where)
            else:
                problems = legacy.check_geometry(m)
                if problems:
                    tally.add('models: (a) FAIL legacy conversion geometry', where + ' ' + problems[0])
                    status = 1
                else:
                    tally.add('models: (a) legacy 1.x converted to 2.0, geometry checked')
            doc, binary = gltf.export(m)
            errs = validate.validate(doc, [binary])
            if errs:
                tally.add('models: (b) FAIL glTF validation', where + ' ' + errs[0])
                status = 1
            else:
                tally.add('models: (b) glTF valid')
            # through the text form, as a file would be
            text = gltf.dumps(dict(doc, buffers=[{'byteLength': len(binary), 'uri': 'x.bin'}]))
            doc2, bufs = gltf.load(text, resolve=lambda uri: binary)
            back = gltf.import_(doc2, bufs)
            out = cem.write(back, exact=False)
            if out == canon:
                tally.add('models: (c) cem -> glTF -> cem byte-identical to the canonical 2.0 form')
            else:
                diff = _game_view_diff(cem.read(canon), back)
                tally.add(f'models: (c) FAIL glTF round trip ({", ".join(diff)})', where)
                status = 1
            gb = gltf.glb(doc, binary)
            d3, b3 = gltf.load(gb)
            if cem.write(gltf.import_(d3, b3), exact=False) != canon:
                tally.add('models: (c) FAIL .glb round trip', where)
                status = 1
            # an editor that drops EE_model: what the core alone keeps
            for nd in doc2['nodes']:
                nd.pop('extensions', None)
            core = gltf.import_(doc2, bufs)
            for k in _game_view_diff(cem.read(canon), core) or ['nothing']:
                tally.add(f'models: (d) without EE_model, differs: {k}')
    return status


def check_textures(srcs, limit, tally):
    status = 0
    n = 0
    for src in srcs:
        for name in src.names('textures\\'):
            if not name.endswith(('.sst', '.tga')):
                continue
            if limit and n >= limit:
                return status
            n += 1
            data = src.read(name)
            where = f'{src.label}:{name}'
            try:
                tex = sst.read(data, name)
            except sst.TextureError as e:
                tally.add(f'textures: FAIL not readable as the game reads it ({e})', where)
                status = 1
                continue
            kind = name[-3:]
            tally.add(f'textures: read .{kind}, {"1 frame" if len(tex.frames) == 1 else "several frames"}')
            if sst.write(tex, kind) == data:
                tally.add(f'textures: (a) .{kind} canonical form byte-identical to the file')
            else:
                tally.add(f'textures: (a) .{kind} canonical form drops unread bytes')
            pngs = sst.to_png(tex)
            back = sst.from_png([p for _, p in pngs])
            if (back.width, back.height, back.frames) == (tex.width, tex.height, tex.frames):
                tally.add('textures: (b) -> PNG -> game view equal (surface size, every frame pixel for pixel)')
            else:
                tally.add('textures: (b) FAIL PNG round trip', where)
                status = 1
                continue
            again = sst.to_png(sst.read(sst.write(back), 'x.sst'))
            if again == pngs:
                tally.add('textures: (c) PNG -> .sst -> PNG byte-identical')
            else:
                tally.add('textures: (c) FAIL PNG fixed point', where)
                status = 1
    return status


# ---------------------------------------------------------------------------------------------------- commands --

def cmd_check(a):
    t0 = time.time()
    srcs = sources.sources(a.game, a.base)
    print('data path: ' + ', '.join(s.label for s in srcs))
    tally = Tally()
    status = 0
    if a.only in (None, 'models'):
        status |= check_models(srcs, a.limit, tally)
    if a.only in (None, 'textures'):
        status |= check_textures(srcs, a.limit, tally)
    tally.report('gates')
    print(f'{"PASS" if status == 0 else "FAIL"} ({time.time() - t0:.0f} s)')
    return status


def export_model(src, name, out_dir, texmap):
    m = cem.read(src.read(name))
    doc, binary = model_to_gltf(m, texmap.get(name[:-4]))
    gltf.write_files(doc, binary, out_dir / (_stem(name) + '.gltf'))


def export_texture(src, name, out_dir):
    tex = sst.read(src.read(name), name)
    for suffix, data in sst.to_png(tex):
        (out_dir / (_stem(name) + suffix)).write_bytes(data)


def resolved_names(srcs, prefix, exts):
    """{name without extension: (source, name)} as the game resolves them (directories, then archives; for textures
    .tga before .sst)."""
    out = {}
    for ext in exts:
        for s in srcs:
            for n in s.names(prefix):
                if n.endswith(ext) and n[:-len(ext)] not in out:
                    found = sources.resolve(srcs, n)
                    out[n[:-len(ext)]] = (found, n)
    return out


def cmd_export(a, out=None):
    srcs = sources.sources(a.game, a.base)
    out = pathlib.Path(out or a.out)
    (out / 'Models').mkdir(parents=True, exist_ok=True)
    (out / 'Textures').mkdir(parents=True, exist_ok=True)
    texmap = texture_map(srcs)
    models = resolved_names(srcs, 'models\\', ['.cem'])
    textures = resolved_names(srcs, 'textures\\', ['.tga', '.sst'])
    want = {n.lower() for n in getattr(a, 'names', None) or []}
    done = bad = 0
    for stem, (src, name) in sorted(models.items()):
        if want and name not in want:
            continue
        try:
            export_model(src, name, out / 'Models', texmap)
            done += 1
        except cem.CemError as e:
            print(f'skipped {src.label}:{name}: {e}')
            bad += 1
    for stem, (src, name) in sorted(textures.items()):
        if want and name not in want:
            continue
        export_texture(src, name, out / 'Textures')
        done += 1
    print(f'{done} files converted, {bad} skipped -> {out}')
    return 0


def cmd_convert(a):
    i, o = pathlib.Path(a.input), pathlib.Path(a.output)
    ie, oe = i.suffix.lower(), o.suffix.lower()
    if ie == '.cem' and oe in ('.gltf', '.glb'):
        doc, binary = gltf.export(cem.read(i.read_bytes()))
        if oe == '.glb':
            o.write_bytes(gltf.glb(doc, binary))
        else:
            gltf.write_files(doc, binary, o)
    elif ie in ('.gltf', '.glb') and oe == '.cem':
        doc, bufs = gltf.load(i)
        o.write_bytes(cem.write(gltf.import_(doc, bufs), exact=False))
    elif ie in ('.sst', '.tga') and oe == '.png':
        tex = sst.read(i.read_bytes(), i.name)
        for suffix, data in sst.to_png(tex):
            o.with_name(o.stem + suffix).write_bytes(data)
    elif ie == '.png' and oe in ('.sst', '.tga'):
        files = [i.read_bytes()]
        k = 1
        while i.with_name(f'{i.stem}@{k}.png').is_file():
            files.append(i.with_name(f'{i.stem}@{k}.png').read_bytes())
            k += 1
        o.write_bytes(sst.write(sst.from_png(files), oe[1:]))
    else:
        sys.exit(f'cannot convert {ie} to {oe}')
    return 0


def cmd_validate(a):
    status = 0
    for f in a.files:
        doc, bufs = gltf.load(f)
        errs = validate.validate(doc, bufs)
        print(f'{f}: ' + ('valid' if not errs else f'{len(errs)} problems'))
        for e in errs[:20]:
            print('  ' + e)
        status |= bool(errs)
    return status


def _refuse(target):
    t = target.resolve()
    for d in sources.game_dirs() + [sources.AOC, sources.BASE]:
        try:
            if t == d.resolve() or t.is_relative_to(d.resolve()):
                sys.exit(f'refusing to write into the game install {d}: use a copy')
        except OSError:
            pass
    if t.is_relative_to(ROOT) and not t.is_relative_to(ROOT / 'build'):
        sys.exit('refusing to write game data into the repository (outside build/)')


def cmd_install(a):
    target = pathlib.Path(a.gamedir)
    _refuse(target)
    if not (target / 'Data').is_dir():
        sys.exit(f'{target} has no Data folder')
    data = target / 'Data'
    for sub in ('Models', 'Textures'):
        (data / sub).mkdir(exist_ok=True)
    return cmd_export(a, out=data)


def cmd_render(a):
    import render
    return render.main(a)


def cmd_cppgate(a):
    import cppgate
    return cppgate.run(a)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0], epilog=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    for name in ('export', 'check', 'install', 'cppgate'):
        p = sub.add_parser(name)
        p.add_argument('--game')
        p.add_argument('--base')
        if name == 'export':
            p.add_argument('out')
            p.add_argument('names', nargs='*')
        if name == 'check':
            p.add_argument('--only', choices=['models', 'textures'])
            p.add_argument('--limit', type=int, default=0)
        if name == 'install':
            p.add_argument('gamedir')
        if name == 'cppgate':
            p.add_argument('--out')
    p = sub.add_parser('convert')
    p.add_argument('input')
    p.add_argument('output')
    p = sub.add_parser('validate')
    p.add_argument('files', nargs='+')
    p = sub.add_parser('render')
    p.add_argument('input')
    p.add_argument('output')
    p.add_argument('--frame', type=int, default=0)
    p.add_argument('--textures', help='folder of the PNG (or .sst/.tga) textures, for a .cem')
    p.add_argument('--texture', help='for a .cem: the texture stem to draw every material with (dbgraphics)')
    p.add_argument('--size', type=int, default=512)
    a = ap.parse_args()
    return {'export': cmd_export, 'check': cmd_check, 'install': cmd_install, 'convert': cmd_convert,
            'validate': cmd_validate, 'render': cmd_render, 'cppgate': cmd_cppgate}[a.cmd](a)


if __name__ == '__main__':
    sys.exit(main())
