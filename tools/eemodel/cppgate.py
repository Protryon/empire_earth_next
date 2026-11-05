"""The C++ gate (eemodel.py cppgate): build tools/eemodel/cppgate/eemodel_gate.exe from the game's open-format
loader (src/Low-Level Engine/Graphics Engine/GEOpenFormats.cpp with cgltf and stb_image, the build's compiler flags)
and run it over

* every model on the data path as eemodel exports it (.gltf + .bin), the same without the EE_model extension (what an
  editor that drops it leaves), and every 25th as .glb;
* edited models: TRS instead of matrices, Blender's ".NNN" name suffixes, no Z-up wrapper, a rotated wrapper (an
  error), broken EE_model data (frame fixes: an error; LODs: dropped), frame transforms from the node animation
  (STEP and, with stretched key times, LINEAR);
* every texture as PNG, and PNG variants an editor may write (grey 8/4-bit, grey+alpha, palette with tRNS, 16-bit,
  RGB with tRNS, interlaced, the ee:surface text, a bad ee:surface text and a truncated file: errors);

The C++ result must equal the Python converter's (gltf.import_ + cem.write, sst.from_png + sst.write) byte for byte,
or both must refuse. The cases go to a temporary folder outside the repository, deleted afterwards.
"""
import copy
import json
import os
import pathlib
import struct
import subprocess
import sys
import tempfile
import time
import zlib
from array import array

import cem
import gltf
import png
import sources
import sst

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
GE = ROOT / 'src' / 'Low-Level Engine' / 'Graphics Engine'


def build(out):
    import clang_build
    import verify
    from ee import CLANG, LLD_LINK, lib_paths, RUNTIME_LIBS
    out.mkdir(parents=True, exist_ok=True)
    _, inc = verify.include_paths(None)
    objs = []
    for src in (GE / 'GEOpenFormats.cpp', ROOT / 'src' / 'ThirdParty' / 'cgltf' / 'cgltf.c',
                ROOT / 'src' / 'ThirdParty' / 'stb_image' / 'stb_image.c', HERE / 'cppgate' / 'eemodel_gate.cpp'):
        obj = out / (src.stem + '.obj')
        lang = ['/std:c++17', '-EHsc'] if src.suffix == '.cpp' else []
        cmd = [str(CLANG), *clang_build.CLANG_FLAGS, *lang, '-MT', '-O1', *verify.DEFINES.split(), f'/I{GE}',
               f'/Fo{obj}', str(src)]
        r = subprocess.run(cmd, env=dict(os.environ, INCLUDE=inc), capture_output=True, text=True)
        if r.returncode:
            sys.exit(f'{src.name}: {r.stdout}{r.stderr}')
        objs.append(str(obj))
    exe = out / 'eemodel_gate.exe'
    r = subprocess.run([str(LLD_LINK), '/nologo', '/subsystem:console', '/machine:x86', f'/out:{exe}', *objs,
                        *lib_paths(), *RUNTIME_LIBS], capture_output=True, text=True)
    if r.returncode:
        sys.exit(f'link: {r.stdout}{r.stderr}')
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


def run(a):
    t0 = time.time()
    out = pathlib.Path(a.out or ROOT / 'build' / 'eemodel-cppgate').resolve()
    exe = build(out)
    srcs = sources.sources(a.game, a.base)
    with tempfile.TemporaryDirectory(prefix='eemodel-') as tmp:
        folder = pathlib.Path(tmp)
        cases = []   # (kind, file, stem, group)
        n = 0
        for src in srcs:
            for name in src.names('models\\'):
                if not name.endswith('.cem'):
                    continue
                try:
                    m = cem.read(src.read(name))
                except cem.CemError:
                    continue
                n += 1
                doc, binary = gltf.export(m)
                stem = f'm{n:04d}'
                _write_gltf(folder, stem, doc, binary)
                cases.append(('model', stem + '.gltf', stem, 'models'))
                core = copy.deepcopy(doc)
                for nd in core['nodes']:
                    nd.pop('extensions', None)
                core.pop('extensionsUsed', None)
                _write_gltf(folder, stem + '_core', core, binary)
                cases.append(('model', stem + '_core.gltf', stem + '_core', 'models without EE_model'))
                if n % 25 == 0:
                    (folder / (stem + '_glb.glb')).write_bytes(gltf.glb(doc, binary))
                    cases.append(('model', stem + '_glb.glb', stem + '_glb', 'models as .glb'))
                exts = [x['extensions']['EE_model'] for x in doc['nodes'] if 'extensions' in x]
                if n % 10 == 0 or any(len(e.get('radii', [])) > 1 or e.get('frameFixes') for e in exts):
                    for suffix, (d, b) in model_edits(doc, binary).items():
                        s = f'{stem}_{suffix}'
                        _write_gltf(folder, s, d, b)
                        cases.append(('model', s + '.gltf', s, 'edited models'))
        t = 0
        for src in srcs:
            for name in src.names('textures\\'):
                if not name.endswith(('.sst', '.tga')):
                    continue
                t += 1
                tex = sst.read(src.read(name), name)
                stem = f't{t:04d}'
                for suffix, data in sst.to_png(tex):
                    (folder / (stem + suffix)).write_bytes(data)
                cases.append(('texture', stem + '.png', stem, 'textures'))
                if t % 100 == 0:
                    for suffix, data in png_variants(tex.frames[0]).items():
                        s = f'{stem}_{suffix}'
                        (folder / (s + '.png')).write_bytes(data)
                        cases.append(('texture', s + '.png', s, 'PNG variants'))
        (folder / 'cases.txt').write_bytes(''.join(f'{k} {f}\n' for k, f, _, _ in cases).encode())
        print(f'--- {len(cases)} cases ({n} models, {t} textures); C++ conversion', flush=True)
        r = subprocess.run([str(exe), str(folder)])
        if r.returncode:
            return 1
        print('--- Python conversion and comparison', flush=True)
        groups = {}
        bad = 0
        for kind, file, stem, group in cases:
            want = _python_model(folder, file) if kind == 'model' else _python_texture(folder, stem)
            o, e = folder / (stem + '.out'), folder / (stem + '.err')
            got = o.read_bytes() if o.exists() else e.read_text(errors='replace') if e.exists() else None
            g = groups.setdefault(group, [0, 0, 0])
            if isinstance(want, bytes) and isinstance(got, bytes):
                ok = want == got
                g[1] += ok
            else:
                ok = not isinstance(want, bytes) and isinstance(got, str)
                g[2] += ok
            g[0] += 1
            if not ok:
                bad += 1
                if bad <= 20:
                    show = lambda v: f'{len(v)} bytes' if isinstance(v, bytes) else f'error: {v}'
                    print(f'  {file:28s} DIFFERS  Python {show(want)}  |  C++ {show(got)}')
        for gname, (total, same, both) in groups.items():
            print(f'  {gname:26s} {total:5} cases: {same} identical bytes, {both} refused by both')
    print(f'{"PASS" if bad == 0 else f"FAIL ({bad} cases differ)"} ({time.time() - t0:.0f} s)')
    return 1 if bad else 0
