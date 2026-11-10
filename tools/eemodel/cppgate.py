"""The open-format gate (eemodel.py cppgate; docs/RUST_PLAN.md D9): the models and textures the game stores from
the open tree equal what the game stored before D9.

1. The store gate (storegate.py): the game's own GEModelManager / GETextureManager code, linked from a game build,
   loads every model and texture of the open tree and digests the stored GEModel / GETexture (every array and
   pixel); the digests were recorded from the loaders before D9 (glTF -> .cem bytes -> GEModel::Load, PNG -> .sst
   bytes -> GETexture::Load) and the native loaders gave the same.
2. The decoder gate: the game's decoder (rust/crates/ee_openformats, the eemodel_gate program) over every model and
   texture of the open tree as files, the same models without the EE_model extension (what an editor that drops it
   leaves), every 25th as .glb, edited models (TRS instead of matrices, Blender's ".NNN" name suffixes, no Z-up
   wrapper, a rotated wrapper: an error, broken EE_model data: frame fixes an error, LODs dropped, frame transforms
   from the node animation, STEP and with stretched key times LINEAR) and PNG variants an editor may write (grey
   8/4-bit, grey+alpha, palette with tRNS, 16-bit, RGB with tRNS, interlaced, the ee:surface text, a bad ee:surface
   text and a truncated file: errors). Each case's result (a digest of the model with three shared-frame maps, of
   the texture's frames, or the refusal message) must equal the recorded one (golden_store.json, --golden), and the
   case must load or be refused as the Python converter (gltf.import_, sst.from_png, an independent implementation)
   decides. golden_store.json was recorded before D9's deletion by a run that also decoded each case the old way (the
   .cem / .sst bytes through the readers, the gate program's former --old) and found every digest equal.

Streaming: the cases are written to a temporary folder in batches of at most --limit-mb (default 300 MB); each batch
is decoded, compared and deleted before the next. --subset N takes every Nth model and every Nth texture (the store
gate then compares that subset). The tree is read only (default: the main checkout's build/open_data/tree).
"""
import copy
import hashlib
import json
import os
import pathlib
import struct
import subprocess
import sys
import tempfile
import time
import urllib.parse
import zlib
from array import array

import cem
import gltf
import png
import sst

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
GOLDEN = HERE / 'cppgate' / 'golden_store.json'

def build(out):
    """The game's decoder's gate program (rust/crates/ee_openformats, src/bin/eemodel_gate.rs), built for the host."""
    from rust_build import TARGET_DIR
    env = dict(os.environ, CARGO_TARGET_DIR=str(TARGET_DIR))
    r = subprocess.run(['cargo', 'build', '-p', 'ee_openformats', '--bin', 'eemodel_gate', '--release', '--offline'],
                       cwd=ROOT / 'rust', env=env, capture_output=True, text=True)
    if r.returncode:
        sys.exit(f'cargo build: {r.stdout}{r.stderr}')
    exe = pathlib.Path(TARGET_DIR) / 'release' / ('eemodel_gate' + ('.exe' if os.name == 'nt' else ''))
    if not exe.is_file():
        sys.exit(f'no {exe}')
    return exe


# ------------------------------------------------------------------------------------------------- PNG variants --

def _png(w, h, ctype, depth, rows, plte=None, trns=None, interlace=False, text=None):
    """rows: [[samples]] (one list of channel samples per row). Filter 0 everywhere."""
    nch = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[ctype]

    def line(samples):
        if depth == 16:
            return b'\0' + b''.join(struct.pack('>H', s) for s in samples)
        if depth == 8:
            return b'\0' + bytes(samples)
        out = bytearray()
        per = 8 // depth
        for i in range(0, len(samples), per):
            b = 0
            for k, s in enumerate(samples[i:i + per]):
                b |= s << (8 - depth * (k + 1))
            out.append(b)
        return b'\0' + bytes(out)
    raw = b''
    if not interlace:
        raw = b''.join(line(r) for r in rows)
    else:
        for x0, y0, dx, dy in ((0, 0, 8, 8), (4, 0, 8, 8), (0, 4, 4, 8), (2, 0, 4, 4), (0, 2, 2, 4), (1, 0, 2, 2),
                               (0, 1, 1, 2)):
            for y in range(y0, h, dy):
                xs = range(x0, w, dx)
                if not xs:
                    continue
                raw += line([v for x in xs for v in rows[y][x * nch:(x + 1) * nch]])

    def chunk(k, d):
        return struct.pack('>I', len(d)) + k + d + struct.pack('>I', zlib.crc32(k + d) & 0xffffffff)
    out = png.SIGNATURE + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, depth, ctype, 0, 0, 1 if interlace else 0))
    if plte is not None:
        out += chunk(b'PLTE', plte)
    if trns is not None:
        out += chunk(b'tRNS', trns)
    for k, v in (text or {}).items():
        out += chunk(b'tEXt', k.encode('latin-1') + b'\0' + v.encode('latin-1'))
    return out + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b'')


def png_variants(img):
    """{suffix: PNG bytes} from a 32x32 crop of an Image."""
    w, h = min(32, img.width), min(32, img.height)
    n = img.channels
    px = [[img.pixels[(y * img.width + x) * n:(y * img.width + x) * n + n] for x in range(w)] for y in range(h)]
    rgba = [[tuple(p) + ((255,) if n == 3 else ()) for p in row] for row in px]
    out = {}
    out['gray8'] = _png(w, h, 0, 8, [[p[0] for p in r] for r in rgba])
    out['gray4'] = _png(w, h, 0, 4, [[p[0] >> 4 for p in r] for r in rgba])
    out['gray8trns'] = _png(w, h, 0, 8, [[p[0] for p in r] for r in rgba], trns=struct.pack('>H', rgba[0][0][0]))
    out['grayalpha'] = _png(w, h, 4, 8, [[v for p in r for v in (p[0], p[3])] for r in rgba])
    colors = sorted({p for r in rgba for p in r})[:256]
    index = {c: i for i, c in enumerate(colors)}
    out['palette'] = _png(w, h, 3, 8, [[index.get(p, 0) for p in r] for r in rgba],
                          plte=b''.join(bytes(c[:3]) for c in colors), trns=bytes(c[3] for c in colors))
    out['rgba16'] = _png(w, h, 6, 16, [[v * 257 for p in r for v in p] for r in rgba])
    out['rgbtrns'] = _png(w, h, 2, 8, [[v for p in r for v in p[:3]] for r in rgba],
                          trns=struct.pack('>HHH', *rgba[0][0][:3]))
    out['interlaced'] = _png(w, h, 6, 8, [[v for p in r for v in p] for r in rgba], interlace=True)
    out['surface'] = _png(w, h, 6, 8, [[v for p in r for v in p] for r in rgba], text={'ee:surface': '64x128'})
    out['badsurface'] = _png(w, h, 6, 8, [[v for p in r for v in p] for r in rgba], text={'ee:surface': '64 x128'})
    out['truncated'] = out['interlaced'][:len(out['interlaced']) // 2]
    return out


# ------------------------------------------------------------------------------------------------ model edits --

def _with_bin(doc, binary):
    return copy.deepcopy(doc), bytearray(binary)


def model_edits(doc, binary):
    """{suffix: (doc, bin)} edited variants of an exported model."""
    out = {}
    d, b = _with_bin(doc, binary)
    for n in d['nodes'][1:]:
        if 'matrix' in n:
            n['translation'], n['rotation'], n['scale'] = gltf.decompose(n.pop('matrix'))
    out['trs'] = (d, b)
    d, b = _with_bin(doc, binary)
    for n in d['nodes'][1:]:
        n['name'] = n.get('name', '') + '.001'
    for m in d.get('materials', []):
        m['name'] = m.get('name', '') + '.002'
    out['dup'] = (d, b)
    d, b = _with_bin(doc, binary)
    d['scenes'][0]['nodes'] = d['nodes'][0]['children']
    d['nodes'][0]['children'] = []
    d['nodes'][0]['name'] = 'unused'
    out['nowrap'] = (d, b)
    d, b = _with_bin(doc, binary)
    d['nodes'][0]['matrix'] = gltf.compose([0, 0, 0], [0.0, 0.3826834, 0.0, 0.9238795], [1, 1, 1])
    out['rotated'] = (d, b)
    exts = [n['extensions']['EE_model'] for n in doc['nodes'] if 'extensions' in n]
    if any(len(e.get('radii', [])) > 1 for e in exts):
        d, b = _with_bin(doc, binary)
        for n in d['nodes']:
            e = n.get('extensions', {}).get('EE_model')
            if e and len(e['radii']) > 1:
                e['frameFixes'] = [len(d['accessors']) + 5]
        out['badfix'] = (d, b)
        d, b = _with_bin(doc, binary)
        for n in d['nodes']:
            n.get('extensions', {}).get('EE_model', {}).pop('transforms', None)
        out['animtf'] = (d, b)
        # stretched key times: the frame times fall between keys (LINEAR interpolation, STEP holds)
        d, b = _with_bin(doc, binary)
        for n in d['nodes']:
            n.get('extensions', {}).get('EE_model', {}).pop('transforms', None)
        seen = set()
        for an in d.get('animations', []):
            for s in an['samplers']:
                a = d['accessors'][s['input']]
                if s['input'] in seen:
                    continue
                seen.add(s['input'])
                v = d['bufferViews'][a['bufferView']]
                off = v.get('byteOffset', 0) + a.get('byteOffset', 0)
                t = array('f')
                t.frombytes(bytes(b[off:off + 4 * a['count']]))
                t = array('f', [x * 1.5 for x in t])
                b[off:off + 4 * a['count']] = t.tobytes()
                a['min'], a['max'] = [t[0]], [t[-1]]
            for i, s in enumerate(an['samplers']):
                if s.get('interpolation') == 'STEP' and i % 2:
                    s['interpolation'] = 'LINEAR'
        out['stretched'] = (d, b)
    if any(e.get('lods') for e in exts):
        d, b = _with_bin(doc, binary)
        for n in d['nodes']:
            e = n.get('extensions', {}).get('EE_model')
            if e and e.get('lods'):
                e['lods'][0]['ranges'] = e['lods'][0]['ranges'][:-1] or [[1, 2, 3]]
        out['badlod'] = (d, b)
    return out


def _write_gltf(folder, name, doc, binary):
    d = dict(doc, buffers=[{'byteLength': len(binary), 'uri': name + '.bin'}])
    (folder / (name + '.gltf')).write_bytes(gltf.dumps(d))
    (folder / (name + '.bin')).write_bytes(bytes(binary))


def _python_model(folder, file):
    try:
        doc, bufs = gltf.load(folder / file)
        return cem.write(gltf.import_(doc, bufs), exact=False)
    except (gltf.GltfError, cem.CemError, KeyError, IndexError, ValueError, TypeError, struct.error) as e:
        return e


def _python_texture(folder, stem):
    files = [(folder / (stem + '.png')).read_bytes()]
    k = 1
    while (folder / f'{stem}@{k}.png').is_file():
        files.append((folder / f'{stem}@{k}.png').read_bytes())
        k += 1
    try:
        return sst.write(sst.from_png(files))
    except (png.PngError, sst.TextureError, zlib.error, ValueError, struct.error, IndexError) as e:
        return e


def _digest(result):
    data = result if isinstance(result, bytes) else b'E:' + result.encode('utf-8', 'surrogateescape')
    return hashlib.sha256(data).hexdigest()[:12]


def iter_cases(tree, subset):
    """Yield (kind, file, stem, group, {file name: bytes}) for every case, in a fixed order."""
    models = sorted(p for p in (tree / 'Data' / 'Models').iterdir() if p.suffix.lower() == '.gltf')
    for n, path in enumerate(models, 1):
        if (n - 1) % subset:
            continue
        doc = json.loads(path.read_bytes())
        binary = (path.parent / urllib.parse.unquote(doc['buffers'][0]['uri'])).read_bytes()
        stem = f'm{n:04d}'
        yield ('model', stem + '.gltf', stem, 'models', _gltf_files(stem, doc, binary))
        core = copy.deepcopy(doc)
        for nd in core['nodes']:
            nd.pop('extensions', None)
        core.pop('extensionsUsed', None)
        yield ('model', stem + '_core.gltf', stem + '_core', 'models without EE_model',
               _gltf_files(stem + '_core', core, binary))
        if n % 25 == 0:
            yield ('model', stem + '_glb.glb', stem + '_glb', 'models as .glb',
                   {stem + '_glb.glb': gltf.glb(dict(doc, buffers=[{'byteLength': len(binary)}]), binary)})
        exts = [x['extensions']['EE_model'] for x in doc['nodes'] if 'extensions' in x]
        if n % 10 == 0 or any(len(e.get('radii', [])) > 1 or e.get('frameFixes') for e in exts):
            for suffix, (d, b) in model_edits(doc, binary).items():
                s = f'{stem}_{suffix}'
                yield ('model', s + '.gltf', s, 'edited models', _gltf_files(s, d, b))
    folder = tree / 'Data' / 'Textures'
    textures = sorted(p for p in folder.iterdir() if p.suffix.lower() == '.png' and '@' not in p.stem)
    for t, path in enumerate(textures, 1):
        if (t - 1) % subset:
            continue
        stem = f't{t:04d}'
        files = {stem + '.png': path.read_bytes()}
        k = 1
        while (folder / f'{path.stem}@{k}.png').is_file():
            files[f'{stem}@{k}.png'] = (folder / f'{path.stem}@{k}.png').read_bytes()
            k += 1
        yield ('texture', stem + '.png', stem, 'textures', files)
        if t % 100 == 0:
            for suffix, data in png_variants(sst.from_png([files[stem + '.png']]).frames[0]).items():
                s = f'{stem}_{suffix}'
                yield ('texture', s + '.png', s, 'PNG variants', {s + '.png': data})


def _gltf_files(name, doc, binary):
    d = dict(doc, buffers=[{'byteLength': len(binary), 'uri': name + '.bin'}])
    return {name + '.gltf': gltf.dumps(d), name + '.bin': bytes(binary)}


def _folder_size(folder):
    return sum(e.stat().st_size for e in os.scandir(folder) if e.is_file())


def _clear(folder):
    for e in os.scandir(folder):
        if e.is_file():
            os.unlink(e.path)


class Stream:
    """Decodes cases in bounded batches: flush() runs the decoder on the batch, compares it and deletes it."""

    def __init__(self, exe, folder, limit):
        self.exe, self.folder, self.limit = exe, folder, limit
        self.batch, self.used = [], 0
        self.groups, self.bad = {}, 0
        self.digests, self.names, self.peak, self.batches = [], [], 0, 0

    def add(self, case):
        kind, file, stem, group, files = case
        cost = sum(len(v) for v in files.values()) + 4096
        if self.batch and self.used + cost > self.limit:
            self.flush()
        for name, data in files.items():
            (self.folder / name).write_bytes(data)
        self.batch.append((kind, file, stem, group))
        self.used += cost

    def flush(self):
        if not self.batch:
            return
        (self.folder / 'cases.txt').write_bytes(''.join(f'{k} {f}\n' for k, f, _, _ in self.batch).encode())
        r = subprocess.run([str(self.exe), str(self.folder)], stdout=subprocess.DEVNULL)
        if r.returncode:
            sys.exit(f'the decoder failed with exit status {r.returncode:#x}')
        self.peak = max(self.peak, _folder_size(self.folder))
        self.batches += 1
        for kind, file, stem, group in self.batch:
            want = _python_model(self.folder, file) if kind == 'model' else _python_texture(self.folder, stem)
            o, e = self.folder / (stem + '.out'), self.folder / (stem + '.err')
            if o.exists():
                got = o.read_bytes()
            elif e.exists():
                got = e.read_text(encoding='utf-8', errors='surrogateescape')
            else:
                got = None
            g = self.groups.setdefault(group, [0, 0, 0])
            ok = got is not None and isinstance(want, bytes) == isinstance(got, bytes)
            g[1 if isinstance(got, bytes) else 2] += ok
            g[0] += 1
            self.digests.append(_digest(got) if got is not None else 'none')
            self.names.append(stem)
            if not ok:
                self.bad += 1
                if self.bad <= 20:
                    show = lambda v: 'loads' if isinstance(v, bytes) else f'error: {v}'
                    print(f'  {file:28s} DIFFERS  Python {show(want)}  |  decoder {show(got)}')
        _clear(self.folder)
        self.batch, self.used = [], 0


def tree_fingerprint(tree):
    f = tree / 'open_data.json'
    return hashlib.sha256(f.read_bytes()).hexdigest() if f.is_file() else None


def run(a):
    t0 = time.time()
    import storegate
    out = pathlib.Path(a.out or ROOT / 'build' / 'eemodel-cppgate').resolve()
    tree = pathlib.Path(a.tree).resolve() if a.tree else storegate.DEFAULT_TREE
    subset = max(1, a.subset)
    bad = 0
    if not a.no_store:
        print('--- the store gate (storegate.py): the GEModel / GETexture the game stores', flush=True)
        cmd = [sys.executable, '-I', str(HERE / 'storegate.py'), '--tree', str(tree), '--subset', str(subset)]
        if a.build:
            cmd += ['--build', str(a.build)]
        bad += subprocess.run(cmd).returncode != 0
    exe = pathlib.Path(a.exe) if a.exe else build(out)
    fingerprint = tree_fingerprint(tree)
    limit = int(a.limit_mb * 1_000_000)
    print(f'--- the decoder gate over {"all" if subset == 1 else f"every {subset}th"} model/texture of {tree}, '
          f'temporary folder capped at {a.limit_mb} MB', flush=True)
    out.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='eemodel-', dir=out) as tmp:
        stream = Stream(exe, pathlib.Path(tmp), limit)
        for case in iter_cases(tree, subset):
            stream.add(case)
        stream.flush()
    for gname, (total, loads, refused) in stream.groups.items():
        print(f'  {gname:26s} {total:5} cases: {loads} loaded, {refused} refused (as the Python decides)')
    print(f'{sum(g[0] for g in stream.groups.values())} cases in {stream.batches} batches; '
          f'peak temporary use {stream.peak / 1e6:.1f} MB')
    bad += stream.bad
    if a.record_golden:
        if subset != 1:
            sys.exit('--record-golden needs the whole tree (no --subset)')
        pathlib.Path(a.record_golden).write_text(json.dumps(
            {'tree': fingerprint, 'count': len(stream.digests),
             'digests': ' '.join(f'{n}={d}' for n, d in zip(stream.names, stream.digests))}, indent=1) + '\n')
        print(f'golden recorded: {a.record_golden} ({len(stream.digests)} digests)')
    golden = a.golden or (None if a.record_golden else GOLDEN)
    if golden:
        g = json.loads(pathlib.Path(golden).read_text())
        recorded = dict(item.split('=', 1) for item in g['digests'].split(' '))
        if g.get('tree') != fingerprint:
            print(f'golden NOT compared: it records tree {g.get("tree")}, this one is {fingerprint}')
            bad += 1
        else:
            diffs = [(n, recorded.get(n), d) for n, d in zip(stream.names, stream.digests) if recorded.get(n) != d]
            print(f'golden digests: {len(stream.digests) - len(diffs)}/{len(stream.digests)} equal '
                  f'({g["count"]} recorded)')
            bad += len(diffs)
            for n, w, d in diffs[:20]:
                print(f'  case {n}: golden {w}, got {d}')
    print(f'{"PASS" if bad == 0 else f"FAIL ({bad} cases differ)"} ({time.time() - t0:.0f} s)')
    return 1 if bad else 0
