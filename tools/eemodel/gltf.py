"""cem models <-> glTF 2.0 (docs/MODEL_FORMATS.md section 4).

Export (`export`): one glTF document per .cem file: a wrapper node turns the game's Z-up axes into glTF's Y-up, the
model tree becomes a node tree (one node + mesh per model, one child node per tag point), materials name their
texture's PNG, the frames become morph targets with a weights animation, moving tag points and per-frame transforms
become node animations. Everything the core cannot hold exactly sits in the node extension EE_model (the frame
transforms, bounding radii, centre point, the LOD index arrays and their per-material ranges, the vertex ranges, and
the few frame values a morph target delta cannot reproduce in float32).

Import (`import_`): glTF (ours or an editor's) -> cem.Model. With the EE_model data present (and consistent) the
result is the original model field for field; without it (an editor that drops unknown extensions) the model gets
one LOD, computed centre and radii, cumulative vertex/polygon ranges and the frame transforms sampled from the node
animation.

All JSON numbers are written as the exact decimal value of the float32 (Python repr of the widened float), so every
reader, float32 or float64, gets the same bits back.
"""
import base64
import json
import math
import pathlib
import re
import struct
import sys
import urllib.parse
from array import array

import cem

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent / 'eedata'))
from scalars import decode_str, encode_str   # noqa: E402

EXT = 'EE_model'
FPS = 10.0                  # frame i is keyframe time i / FPS
AXES = [1.0, 0.0, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0]   # Z up -> Y up
AXES_NAME = 'EE axes (Z up)'
IDENTITY = [1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0]
FLOAT, UINT, USHORT, UBYTE = 5126, 5125, 5123, 5121
_SUFFIX = re.compile(r'\.[0-9]{3}$')


class GltfError(Exception):
    pass


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def to_gltf_matrix(t):
    """GETransformation mMatrix (row-major, column vectors) -> glTF column-major list."""
    return [t[r * 4 + c] for c in range(4) for r in range(4)]


def from_gltf_matrix(g):
    return array('f', [g[c * 4 + r] for r in range(4) for c in range(4)])


def decompose(g):
    """glTF column-major matrix -> (translation, rotation quaternion xyzw, scale), approximately."""
    cols = [[g[c * 4 + r] for r in range(3)] for c in range(3)]
    s = [math.sqrt(sum(x * x for x in col)) for col in cols]
    det = (cols[0][0] * (cols[1][1] * cols[2][2] - cols[2][1] * cols[1][2])
           - cols[1][0] * (cols[0][1] * cols[2][2] - cols[2][1] * cols[0][2])
           + cols[2][0] * (cols[0][1] * cols[1][2] - cols[1][1] * cols[0][2]))
    if det < 0:
        s[0] = -s[0]
    r = [[cols[c][i] / s[c] if s[c] else (1.0 if i == c else 0.0) for c in range(3)] for i in range(3)]
    tr = r[0][0] + r[1][1] + r[2][2]
    if tr > 0:
        w = math.sqrt(1.0 + tr) * 2
        q = [(r[2][1] - r[1][2]) / w, (r[0][2] - r[2][0]) / w, (r[1][0] - r[0][1]) / w, 0.25 * w]
    elif r[0][0] > r[1][1] and r[0][0] > r[2][2]:
        w = math.sqrt(max(1e-30, 1.0 + r[0][0] - r[1][1] - r[2][2])) * 2
        q = [0.25 * w, (r[0][1] + r[1][0]) / w, (r[0][2] + r[2][0]) / w, (r[2][1] - r[1][2]) / w]
    elif r[1][1] > r[2][2]:
        w = math.sqrt(max(1e-30, 1.0 + r[1][1] - r[0][0] - r[2][2])) * 2
        q = [(r[0][1] + r[1][0]) / w, 0.25 * w, (r[1][2] + r[2][1]) / w, (r[0][2] - r[2][0]) / w]
    else:
        w = math.sqrt(max(1e-30, 1.0 + r[2][2] - r[0][0] - r[1][1])) * 2
        q = [(r[0][2] + r[2][0]) / w, (r[1][2] + r[2][1]) / w, 0.25 * w, (r[1][0] - r[0][1]) / w]
    n = math.sqrt(sum(x * x for x in q)) or 1.0
    return [g[12], g[13], g[14]], [x / n for x in q], s


def compose(t, q, s):
    """(translation, quaternion xyzw, scale) -> glTF column-major matrix (float32-rounded)."""
    x, y, z, w = q
    r = [[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
         [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
         [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]]
    g = [0.0] * 16
    for c in range(3):
        for i in range(3):
            g[c * 4 + i] = f32(r[i][c] * s[c])
    g[12], g[13], g[14], g[15] = f32(t[0]), f32(t[1]), f32(t[2]), 1.0
    return g


def matmul(a, b):
    """Column-major 4x4 product a * b."""
    return [sum(a[k * 4 + r] * b[c * 4 + k] for k in range(4)) for c in range(4) for r in range(4)]


def is_signed_permutation(g):
    return all(v in (0.0, 1.0, -1.0) for v in g) and g[12:16] == [0.0, 0.0, 0.0, 1.0]


# ---------------------------------------------------------------------------------------------------- export --

class _Builder:
    def __init__(self):
        self.bin = bytearray()
        self.views = []
        self.accessors = []

    def view(self, data, stride=None, target=None):
        while len(self.bin) % 4:
            self.bin.append(0)
        v = {'buffer': 0, 'byteOffset': len(self.bin), 'byteLength': len(data)}
        if stride:
            v['byteStride'] = stride
        if target:
            v['target'] = target
        self.bin += data
        self.views.append(v)
        return len(self.views) - 1

    def accessor(self, view, offset, ctype, count, typ, mn=None, mx=None, sparse=None):
        a = {'componentType': ctype, 'count': count, 'type': typ}
        if view is not None:
            a['bufferView'] = view
            if offset:
                a['byteOffset'] = offset
        if mn is not None:
            a['min'], a['max'] = mn, mx
        if sparse:
            a['sparse'] = sparse
        self.accessors.append(a)
        return len(self.accessors) - 1


def _minmax(values, n, comp):
    mn = [math.inf] * comp
    mx = [-math.inf] * comp
    for i in range(n):
        for c in range(comp):
            v = values[i * comp + c]
            if v < mn[c]:
                mn[c] = v
            if v > mx[c]:
                mx[c] = v
    return mn, mx


def _delta(base, value):
    """A float32 d with float32(base + d) == value, or None."""
    if value != value or base != base or math.isinf(value) or math.isinf(base):
        return None
    d = f32(value - base)
    if math.isinf(d):
        return None
    if f32(base + d) == value:
        return d
    bits = struct.unpack('<i', struct.pack('<f', d))[0]
    for k in (1, -1, 2, -2, 3, -3):
        e = struct.unpack('<f', struct.pack('<i', bits + k))[0]
        if not math.isinf(e) and e == e and f32(base + e) == value:
            return e
    return None


def texture_uri(name, frame=0):
    """The PNG of a texture (frame i > 0: <name>@<i>.png, sst.py), relative to Data/Models."""
    return '../Textures/' + urllib.parse.quote(name + (f'@{frame}' if frame else '') + '.png')


def export(root, frames_per_second=FPS, display_texture=None):
    """cem.Model tree -> (glTF JSON dict, bin bytes). display_texture: the texture the game draws the model with
    (dbgraphics; the model's own texture names are not read): the images point at its PNG (the frame a material's
    texture index selects) and keep the model's name as image name."""
    b = _Builder()
    doc = {'asset': {'version': '2.0', 'generator': 'eemodel (empire_earth_next tools/eemodel)'},
           'extensionsUsed': [EXT], 'scene': 0, 'scenes': [{'nodes': [0]}],
           'nodes': [{'name': AXES_NAME, 'matrix': AXES, 'children': []}],
           'meshes': [], 'materials': [], 'textures': [], 'images': [], 'samplers': [{}]}
    channels, samplers = [], []
    mat_keys, img_keys = {}, {}
    times_acc = {}

    def times(n):
        if n not in times_acc:
            t = array('f', [f32(i / frames_per_second) for i in range(n)])
            v = b.view(t.tobytes())
            times_acc[n] = b.accessor(v, 0, FLOAT, n, 'SCALAR', [t[0]], [t[-1]])
        return times_acc[n]

    def material(mat):
        name = decode_str(cem.cstr(mat.name))
        tex = decode_str(cem.cstr(mat.texture))
        key = (name, tex, mat.texture_index)
        if key in mat_keys:
            return mat_keys[key]
        m = {'name': name, 'pbrMetallicRoughness': {'metallicFactor': 0.0, 'roughnessFactor': 1.0}}
        if tex:
            uri = texture_uri(display_texture, mat.texture_index) if display_texture else texture_uri(tex)
            if (tex, uri) not in img_keys:
                doc['images'].append({'name': tex, 'uri': uri})
                doc['textures'].append({'sampler': 0, 'source': len(doc['images']) - 1})
                img_keys[(tex, uri)] = len(doc['textures']) - 1
            m['pbrMetallicRoughness']['baseColorTexture'] = {'index': img_keys[(tex, uri)]}
        if mat.texture_index:
            m['extras'] = {'ee_textureIndex': mat.texture_index}
        doc['materials'].append(m)
        mat_keys[key] = len(doc['materials']) - 1
        return mat_keys[key]

    def add_model(m, parent):
        ni = len(doc['nodes'])
        node = {'name': decode_str(cem.cstr(m.name))}
        doc['nodes'].append(node)
        doc['nodes'][parent].setdefault('children', []).append(ni)
        vc, fc = m.vertex_count, len(m.frames)
        f0 = m.frames[0].vertices
        ext = {}
        # vertices of frame 0, interleaved as in the file (position, normal, texture coordinate: 32 bytes)
        vview = b.view(f0.tobytes(), 32, 34962)
        # morph targets: frame i - frame 0 (position, normal), float32-exact where possible, else a sparse fix
        dview = None
        fixes = []
        if fc > 1:
            deltas = array('f')
            for fi in range(1, fc):
                v = m.frames[fi].vertices
                fix = []
                for i in range(vc):
                    for c in range(6):
                        k = 8 * i + c
                        d = _delta(f0[k], v[k])
                        if d is None:
                            fix.append((6 * i + c, v[k]))
                            d = 0.0
                        deltas.append(d)
                fixes.append(fix)
            dview = b.view(deltas.tobytes(), 24, 34962)
        lod0 = m.lods[0]
        iview = b.view(lod0.tobytes(), None, 34963)
        prims = []
        for mi, mat in enumerate(m.materials):
            vs, vn = mat.vertex_start, mat.vertex_count
            pos = [f0[8 * (vs + i) + c] for i in range(vn) for c in range(3)]
            pmin, pmax = _minmax(pos, vn, 3)
            attrs = {'POSITION': b.accessor(vview, 32 * vs, FLOAT, vn, 'VEC3', pmin, pmax),
                     'NORMAL': b.accessor(vview, 32 * vs + 12, FLOAT, vn, 'VEC3'),
                     'TEXCOORD_0': b.accessor(vview, 32 * vs + 24, FLOAT, vn, 'VEC2')}
            ps, pn = mat.ranges[0]
            prim = {'attributes': attrs, 'indices': b.accessor(iview, 12 * ps, UINT, 3 * pn, 'SCALAR'),
                    'material': material(mat), 'mode': 4}
            if fc > 1:
                targets = []
                for fi in range(1, fc):
                    off = 24 * (vc * (fi - 1) + vs)
                    dp = [deltas[6 * (vc * (fi - 1) + vs + i) + c] for i in range(vn) for c in range(3)]
                    dmin, dmax = _minmax(dp, vn, 3)
                    targets.append({'POSITION': b.accessor(dview, off, FLOAT, vn, 'VEC3', dmin, dmax),
                                    'NORMAL': b.accessor(dview, off + 12, FLOAT, vn, 'VEC3')})
                prim['targets'] = targets
            prims.append(prim)
        mesh = {'name': node['name'], 'primitives': prims}
        if fc > 1:
            mesh['weights'] = [0.0] * (fc - 1)
            mesh['extras'] = {'targetNames': [f'frame {i}' for i in range(1, fc)]}
        doc['meshes'].append(mesh)
        node['mesh'] = len(doc['meshes']) - 1
        # the EE_model data
        ext['center'] = list(m.center)
        ext['radii'] = [f.radius[0] for f in m.frames]
        ext['vertexStarts'] = [mat.vertex_start for mat in m.materials]
        ranges0 = [list(mat.ranges[0]) for mat in m.materials]
        cum = 0
        cumulative = True
        for s, n in ranges0:
            if s != cum:
                cumulative = False
            cum += n
        if not cumulative or cum != len(lod0) // 3:
            ext['lod0Ranges'] = ranges0
        if m.polygon_count * 3 != len(lod0):
            ext['polygonCount'] = m.polygon_count
        lods = []
        for li in range(1, len(m.lods)):
            arr = m.lods[li]
            lod = {'ranges': [list(mat.ranges[li]) for mat in m.materials]}
            if len(arr):
                lod['indices'] = b.accessor(b.view(arr.tobytes()), 0, UINT, len(arr), 'SCALAR')
            lods.append(lod)
        if lods:
            ext['lods'] = lods
        if any(fixes):
            fx = []
            for fix in fixes:
                if not fix:
                    fx.append(None)
                    continue
                idx = array('I', [k for k, _ in fix])
                val = array('f', [v for _, v in fix])
                iv, vv = b.view(idx.tobytes()), b.view(val.tobytes())
                fx.append(b.accessor(None, 0, FLOAT, 6 * vc, 'SCALAR', sparse={
                    'count': len(fix), 'indices': {'bufferView': iv, 'componentType': UINT},
                    'values': {'bufferView': vv}}))
            ext['frameFixes'] = fx
        # transforms: a static one as the node matrix, moving ones as TRS animation + exact matrices in EE_model
        tf = [to_gltf_matrix(f.transform) for f in m.frames]
        moving = any(t != tf[0] for t in tf)
        if fc == 1 or (not moving and tf[0] == IDENTITY):
            if tf[0] != IDENTITY:
                node['matrix'] = tf[0]
        else:
            # an animated node may not have a matrix (glTF 3.7.3.4): TRS, with the exact matrices in EE_model
            ext['transforms'] = tf
            trs = [decompose(t) for t in tf]
            node['translation'], node['rotation'], node['scale'] = trs[0]
        if moving:
            for path, k, typ in (('translation', 0, 'VEC3'), ('rotation', 1, 'VEC4'), ('scale', 2, 'VEC3')):
                out = array('f', [x for t in trs for x in t[k]])
                acc = b.accessor(b.view(out.tobytes()), 0, FLOAT, fc, typ)
                samplers.append({'input': times(fc), 'output': acc, 'interpolation': 'STEP'})
                channels.append({'sampler': len(samplers) - 1, 'target': {'node': ni, 'path': path}})
        if fc > 1:
            w = array('f', [1.0 if (j == fi - 1) else 0.0 for fi in range(fc) for j in range(fc - 1)])
            acc = b.accessor(b.view(w.tobytes()), 0, FLOAT, fc * (fc - 1), 'SCALAR')
            samplers.append({'input': times(fc), 'output': acc, 'interpolation': 'LINEAR'})
            channels.append({'sampler': len(samplers) - 1, 'target': {'node': ni, 'path': 'weights'}})
        node['extensions'] = {EXT: ext}
        # sub-models, then the tag points
        for c in m.children:
            add_model(c, ni)
        for ti, tn in enumerate(m.tag_names):
            tni = len(doc['nodes'])
            pos = [list(f.tags[3 * ti:3 * ti + 3]) for f in m.frames]
            doc['nodes'].append({'name': decode_str(cem.cstr(tn)), 'translation': pos[0]})
            node.setdefault('children', []).append(tni)
            if any(p != pos[0] for p in pos):
                out = array('f', [x for p in pos for x in p])
                acc = b.accessor(b.view(out.tobytes()), 0, FLOAT, fc, 'VEC3')
                samplers.append({'input': times(fc), 'output': acc, 'interpolation': 'STEP'})
                channels.append({'sampler': len(samplers) - 1, 'target': {'node': tni, 'path': 'translation'}})

    add_model(root, 0)
    if channels:
        doc['animations'] = [{'name': 'frames', 'channels': channels, 'samplers': samplers}]
    for k in ('materials', 'textures', 'images'):
        if not doc[k]:
            del doc[k]
    if 'textures' not in doc:
        del doc['samplers']
    doc['accessors'] = b.accessors
    doc['bufferViews'] = b.views
    while len(b.bin) % 4:
        b.bin.append(0)
    doc['buffers'] = [{'byteLength': len(b.bin)}]
    return doc, bytes(b.bin)


def dumps(doc):
    """The .gltf text (UTF-8, LF, two-space indent, arrays of numbers on one line)."""
    s = json.dumps(doc, indent=1, ensure_ascii=False, allow_nan=False)
    # collapse numeric arrays onto one line
    s = re.sub(r'\[\s*((?:-?[0-9][0-9.eE+-]*,\s*)*-?[0-9][0-9.eE+-]*)\s*\]',
               lambda mo: '[' + re.sub(r',\s*', ', ', mo.group(1)) + ']', s)
    return (s + '\n').encode('utf-8')


def write_files(doc, binary, gltf_path):
    p = pathlib.Path(gltf_path)
    doc = dict(doc)
    doc['buffers'] = [{'byteLength': len(binary), 'uri': urllib.parse.quote(p.stem + '.bin')}]
    p.write_bytes(dumps(doc))
    p.with_suffix('.bin').write_bytes(binary)


def glb(doc, binary):
    j = json.dumps(doc, separators=(',', ':'), ensure_ascii=False, allow_nan=False).encode('utf-8')
    j += b' ' * (-len(j) % 4)
    binary = binary + b'\0' * (-len(binary) % 4)
    out = struct.pack('<III', 0x46546C67, 2, 12 + 8 + len(j) + 8 + len(binary))
    out += struct.pack('<II', len(j), 0x4E4F534A) + j + struct.pack('<II', len(binary), 0x004E4942) + binary
    return out


# ---------------------------------------------------------------------------------------------------- import --

def load(path_or_bytes, base=None, resolve=None):
    """A .gltf or .glb -> (doc, [buffer bytes]). resolve(uri) -> bytes for external buffers (default: files next to
    the .gltf)."""
    if isinstance(path_or_bytes, (str, pathlib.Path)):
        p = pathlib.Path(path_or_bytes)
        data = p.read_bytes()
        base = p.parent
    else:
        data = path_or_bytes
    glb_bin = None
    if data[:4] == b'glTF':
        magic, ver, length = struct.unpack_from('<III', data, 0)
        if ver != 2:
            raise GltfError(f'GLB version {ver}')
        pos, doc = 12, None
        while pos < min(length, len(data)):
            n, kind = struct.unpack_from('<II', data, pos)
            chunk = data[pos + 8:pos + 8 + n]
            if kind == 0x4E4F534A:
                doc = json.loads(chunk.decode('utf-8'))
            elif kind == 0x004E4942 and glb_bin is None:
                glb_bin = chunk
            pos += 8 + n + (-n % 4)
        if doc is None:
            raise GltfError('GLB without a JSON chunk')
    else:
        doc = json.loads(data.decode('utf-8-sig'))
    bufs = []
    for i, bf in enumerate(doc.get('buffers', [])):
        uri = bf.get('uri')
        if uri is None:
            if glb_bin is None or i != 0:
                raise GltfError(f'buffer {i} has no data')
            bufs.append(glb_bin)
        elif uri.startswith('data:'):
            bufs.append(base64.b64decode(uri.split(',', 1)[1]))
        elif resolve:
            bufs.append(resolve(urllib.parse.unquote(uri)))
        else:
            bufs.append((pathlib.Path(base) / urllib.parse.unquote(uri)).read_bytes())
        if len(bufs[-1]) < bf['byteLength']:
            raise GltfError(f'buffer {i} is shorter than its byteLength')
    return doc, bufs


_COMP = {5120: ('b', 1), 5121: ('B', 1), 5122: ('h', 2), 5123: ('H', 2), 5125: ('I', 4), 5126: ('f', 4)}
_NCOMP = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT2': 4, 'MAT3': 9, 'MAT4': 16}


def read_accessor(doc, bufs, ai):
    """An accessor's elements as a flat list (floats or ints; normalized integers scaled to [0,1]/[-1,1])."""
    a = doc['accessors'][ai]
    code, size = _COMP[a['componentType']]
    nc = _NCOMP[a['type']]
    count = a['count']
    if 'bufferView' in a:
        v = doc['bufferViews'][a['bufferView']]
        buf = bufs[v['buffer']]
        start = v.get('byteOffset', 0) + a.get('byteOffset', 0)
        stride = v.get('byteStride') or size * nc
        if stride == size * nc:
            arr = array(code)
            arr.frombytes(buf[start:start + count * stride])
            out = arr.tolist()
        else:
            out = []
            for i in range(count):
                out.extend(struct.unpack_from(f'<{nc}{code}', buf, start + i * stride))
    else:
        out = [0.0 if code == 'f' else 0] * (count * nc)
    sp = a.get('sparse')
    if sp:
        iv = doc['bufferViews'][sp['indices']['bufferView']]
        icode, isize = _COMP[sp['indices']['componentType']]
        ib = bufs[iv['buffer']]
        ioff = iv.get('byteOffset', 0) + sp['indices'].get('byteOffset', 0)
        vv = doc['bufferViews'][sp['values']['bufferView']]
        vb = bufs[vv['buffer']]
        voff = vv.get('byteOffset', 0) + sp['values'].get('byteOffset', 0)
        for k in range(sp['count']):
            idx = struct.unpack_from('<' + icode, ib, ioff + k * isize)[0]
            out[idx * nc:(idx + 1) * nc] = struct.unpack_from(f'<{nc}{code}', vb, voff + k * size * nc)
    if a.get('normalized') and code != 'f':
        div = {'b': 127, 'B': 255, 'h': 32767, 'H': 65535}[code]
        out = [f32(x / div) for x in out]   # as cgltf: a float division, no clamp
    return out


def _bits(values):
    return array('f', values).tobytes()


def _is_int(x, hi=2 ** 32):
    return isinstance(x, int) and not isinstance(x, bool) and 0 <= x < hi


def _is_num(x):
    return isinstance(x, (int, float)) and not isinstance(x, bool)


def _is_pair_list(r, n):
    return isinstance(r, list) and len(r) == n and all(
        isinstance(p, list) and len(p) == 2 and _is_int(p[0]) and _is_int(p[1]) for p in r)


def _name(s):
    """A glTF name -> the game's bytes: Blender's duplicate suffix (.NNN) dropped, CP-1252 (eedata's mapping)."""
    s = _SUFFIX.sub('', s if isinstance(s, str) else '')
    try:
        return encode_str(s)
    except ValueError as e:
        raise GltfError(f'name "{s}" has characters outside CP-1252') from e


def _f32list(v, n):
    return [f32(float(x)) for x in v] if isinstance(v, list) and len(v) == n and all(map(_is_num, v)) else None


def _local(n):
    """A node's local matrix (column-major, float32 values), from matrix or TRS."""
    m = _f32list(n.get('matrix'), 16)
    if 'matrix' in n:
        if m is None:
            raise GltfError('node matrix is not 16 numbers')
        return m
    t, r, s = _trs(n)
    if t == [0.0, 0.0, 0.0] and r == [0.0, 0.0, 0.0, 1.0] and s == [1.0, 1.0, 1.0]:
        return list(IDENTITY)
    return compose(t, r, s)


def _trs(n):
    out = []
    for k, d in (('translation', [0.0, 0.0, 0.0]), ('rotation', [0.0, 0.0, 0.0, 1.0]), ('scale', [1.0, 1.0, 1.0])):
        if k in n:
            v = _f32list(n[k], len(d))
            if v is None:
                raise GltfError(f'node {k} is not {len(d)} numbers')
            out.append(v)
        else:
            out.append(list(d))
    return out


class _Sampler:
    def __init__(self, doc, bufs, s):
        self.t = read_accessor(doc, bufs, s['input'])
        self.v = read_accessor(doc, bufs, s['output'])
        self.mode = s.get('interpolation', 'LINEAR')

    def at(self, time, nc):
        """The value at a time: STEP holds the key at or before it, LINEAR interpolates (in double precision),
        CUBICSPLINE uses its key values only."""
        t, v = self.t, self.v
        cub = self.mode == 'CUBICSPLINE'
        step = 3 * nc if cub else nc
        off = nc if cub else 0

        def key(i):
            return [float(x) for x in v[i * step + off:i * step + off + nc]]
        if len(v) < len(t) * step:
            raise GltfError('animation sampler output shorter than its input')
        if time <= t[0] or len(t) == 1:
            return key(0)
        if time >= t[-1]:
            return key(len(t) - 1)
        i = 0
        while i + 1 < len(t) and t[i + 1] <= time:
            i += 1
        if t[i] == time or self.mode == 'STEP':
            return key(i)
        a, b_ = key(i), key(i + 1)
        u = (time - t[i]) / (t[i + 1] - t[i])
        return [x + (y - x) * u for x, y in zip(a, b_)]


def _texture_name(doc, mat):
    info = mat.get('pbrMetallicRoughness', {}).get('baseColorTexture')
    if not isinstance(info, dict) or not _is_int(info.get('index'), len(doc.get('textures', []))):
        return b''
    src = doc['textures'][info['index']].get('source')
    if not _is_int(src, len(doc.get('images', []))):
        return b''
    img = doc['images'][src]
    if isinstance(img.get('name'), str) and img['name']:
        return _name(img['name'])
    uri = img.get('uri')
    if isinstance(uri, str) and uri and not uri.startswith('data:'):
        stem = urllib.parse.unquote(uri).replace('\\', '/').rsplit('/', 1)[-1]
        return _name(stem.rsplit('.', 1)[0] if '.' in stem else stem)
    return b''


def import_(doc, bufs):
    """A glTF document -> cem.Model tree (version 2.0). docs/MODEL_FORMATS.md 4.3 is the specification (the C++
    loader, GEModelGLTF.cpp, implements the same rules)."""
    nodes = doc.get('nodes', [])
    parents = {}
    for i, n in enumerate(nodes):
        for c in n.get('children', []):
            parents[c] = i
    scenes = doc.get('scenes', [])
    if scenes:
        roots = scenes[doc.get('scene', 0)].get('nodes', [])
    else:
        roots = [i for i in range(len(nodes)) if i not in parents]
    # the root model: the first node with a mesh, depth first from the scene's roots
    root = None
    stack = list(reversed(roots))
    while stack:
        i = stack.pop()
        if 'mesh' in nodes[i]:
            root = i
            break
        stack.extend(reversed(nodes[i].get('children', [])))
    if root is None:
        raise GltfError('no node with a mesh')
    # the transform above the root, relative to the Z-up wrapper
    above = list(IDENTITY)
    k = root
    while k in parents:
        k = parents[k]
        above = matmul(_local(nodes[k]), above)
    correction = None
    if above != AXES:
        axes_inv = [AXES[c * 4 + r] for r in range(4) for c in range(4)]   # a rotation's inverse: its transpose
        correction = matmul(axes_inv, above)
        if not is_signed_permutation(correction):
            raise GltfError('the transform above the root model is not an axis change')
    channels = {}
    for an in doc.get('animations', [])[:1]:
        for ch in an.get('channels', []):
            tg = ch['target']
            if 'node' in tg:
                channels[(tg['node'], tg['path'])] = _Sampler(doc, bufs, an['samplers'][ch['sampler']])
    return _import_model(doc, bufs, root, channels, correction)


def _import_model(doc, bufs, ni, channels, correction):
    node = doc['nodes'][ni]
    mesh = doc['meshes'][node['mesh']]
    ext = node.get('extensions', {}).get(EXT)
    if not isinstance(ext, dict):
        ext = {}
    m = cem.Model()
    m.name = _name(node.get('name', ''))
    prims = mesh['primitives']
    if not prims:
        raise GltfError(f'node {ni}: mesh without primitives')
    ntargets = len(prims[0].get('targets', []))
    fc = ntargets + 1
    # per primitive: [frame][8 x n floats] and the indices
    pverts, pidx = [], []
    for pi, p in enumerate(prims):
        if p.get('mode', 4) != 4:
            raise GltfError(f'node {ni} primitive {pi}: only triangle lists are supported')
        if len(p.get('targets', [])) != ntargets:
            raise GltfError(f'node {ni}: primitives with different morph target counts')
        at = p['attributes']
        if 'POSITION' not in at:
            raise GltfError(f'node {ni} primitive {pi}: no POSITION')
        pos = read_accessor(doc, bufs, at['POSITION'])
        n = len(pos) // 3
        nor = read_accessor(doc, bufs, at['NORMAL']) if 'NORMAL' in at else [0.0] * (3 * n)
        uv = read_accessor(doc, bufs, at['TEXCOORD_0']) if 'TEXCOORD_0' in at else [0.0] * (2 * n)
        if len(nor) != 3 * n or len(uv) != 2 * n:
            raise GltfError(f'node {ni} primitive {pi}: attribute counts differ')
        frames = []
        for f in range(fc):
            if f == 0:
                fp, fn = pos, nor
            else:
                t = p['targets'][f - 1]
                dp = read_accessor(doc, bufs, t['POSITION']) if 'POSITION' in t else [0.0] * (3 * n)
                dn = read_accessor(doc, bufs, t['NORMAL']) if 'NORMAL' in t else [0.0] * (3 * n)
                if len(dp) != 3 * n or len(dn) != 3 * n:
                    raise GltfError(f'node {ni} primitive {pi}: morph target counts differ')
                fp = [f32(a + b) for a, b in zip(pos, dp)]
                fn = [f32(a + b) for a, b in zip(nor, dn)]
            vals = []
            for i in range(n):
                vals += fp[3 * i:3 * i + 3] + fn[3 * i:3 * i + 3] + uv[2 * i:2 * i + 2]
            frames.append(vals)
        pverts.append((n, frames))
        idx = [int(x) for x in read_accessor(doc, bufs, p['indices'])] if 'indices' in p else list(range(n))
        if len(idx) % 3:
            raise GltfError(f'node {ni} primitive {pi}: index count not a multiple of 3')
        if any(x >= n for x in idx):
            raise GltfError(f'node {ni} primitive {pi}: index out of range')
        pidx.append(idx)
    nmat = len(prims)
    # vertex layout: EE_model's vertex starts when they are valid and consistent, else one range after the other
    starts = ext.get('vertexStarts')
    layout = None
    if isinstance(starts, list) and len(starts) == nmat and all(_is_int(s) for s in starts):
        vc = max(s + pv[0] for s, pv in zip(starts, pverts))
        layout = [None] * vc
        for s, (n, frames) in zip(starts, pverts):
            for i in range(n):
                key = _bits([x for fr in frames for x in fr[8 * i:8 * i + 8]])
                if layout[s + i] is None:
                    layout[s + i] = (key, [fr[8 * i:8 * i + 8] for fr in frames])
                elif layout[s + i][0] != key:   # bit patterns: NaN normals occur (bld_market_10)
                    layout = None
                    break
            if layout is None:
                break
        if layout is not None and any(x is None for x in layout):
            layout = None
    if layout is None:
        starts, layout = [], []
        for n, frames in pverts:
            starts.append(len(layout))
            layout += [(None, [fr[8 * i:8 * i + 8] for fr in frames]) for i in range(n)]
    vc = len(layout)
    frame_vertices = [array('f', [x for _, vs in layout for x in vs[f]]) for f in range(fc)]
    # frame fixes: the exact values a float32 delta cannot give
    fixes = ext.get('frameFixes')
    if isinstance(fixes, list):
        for fi, acc in enumerate(fixes, start=1):
            if acc is None or fi >= fc:
                continue
            accs = doc.get('accessors', [])
            if not _is_int(acc, len(accs)) or 'sparse' not in accs[acc] or accs[acc]['count'] != 6 * vc or \
                    accs[acc]['type'] != 'SCALAR' or accs[acc]['componentType'] != FLOAT:
                raise GltfError(f'node {ni}: EE_model frameFixes[{fi - 1}] does not match the vertices')
            vals = read_accessor(doc, bufs, acc)
            for idx in _sparse_indices(doc, bufs, accs[acc]):
                frame_vertices[fi][8 * (idx // 6) + idx % 6] = vals[idx]
    # LOD 0: EE_model's ranges when they hold the primitives' triangle counts, else one after the other
    counts = [len(x) // 3 for x in pidx]
    r0 = ext.get('lod0Ranges')
    if not (_is_pair_list(r0, nmat) and all(r[1] == c for r, c in zip(r0, counts))):
        r0, cum = [], 0
        for c in counts:
            r0.append([cum, c])
            cum += c
    total0 = max([s + c for s, c in r0] + [0])
    lod0 = [0] * (3 * total0)
    for (s, c), idx in zip(r0, pidx):
        lod0[3 * s:3 * (s + c)] = idx
    m.lods = [array('I', lod0)]
    # LODs 1..: all of EE_model's or none
    extra = []
    lods = ext.get('lods')
    if isinstance(lods, list):
        accs = doc.get('accessors', [])
        for lod in lods:
            rr = lod.get('ranges') if isinstance(lod, dict) else None
            ia = lod.get('indices') if isinstance(lod, dict) else None
            if not _is_pair_list(rr, nmat) or (ia is not None and not (
                    _is_int(ia, len(accs)) and accs[ia]['type'] == 'SCALAR'
                    and accs[ia]['componentType'] in (UBYTE, USHORT, UINT))):
                extra = None
                break
            arr = [int(x) for x in read_accessor(doc, bufs, ia)] if ia is not None else []
            if any(3 * (s + c) > len(arr) or any(x >= pv[0] for x in arr[3 * s:3 * (s + c)])
                   for (s, c), pv in zip(rr, pverts)):
                extra = None
                break
            extra.append((rr, arr))
        extra = extra or []
    for rr, arr in extra:
        m.lods.append(array('I', arr))
    mats = doc.get('materials', [])
    for pi, p in enumerate(prims):
        mat = mats[p['material']] if 'material' in p else {}
        ti = 0
        ex = mat.get('extras')
        if isinstance(ex, dict) and 'ee_textureIndex' in ex:
            ti = ex['ee_textureIndex']
            if not _is_int(ti):
                raise GltfError(f'material {p.get("material")}: ee_textureIndex is not an integer 0..2^32-1')
        ranges = [tuple(r0[pi])] + [tuple(rr[pi]) for rr, _ in extra]
        m.materials.append(cem.Material(_name(mat.get('name', '')), ti, ranges, starts[pi], pverts[pi][0],
                                        _texture_name(doc, mat)))
    pc = ext.get('polygonCount')
    m.polygon_count = pc if _is_int(pc) else total0
    # children: sub-models (with a mesh) and tag points (without)
    subs, tags = [], []
    for c in node.get('children', []):
        (subs if 'mesh' in doc['nodes'][c] else tags).append(c)
    m.tag_names = [_name(doc['nodes'][t].get('name', '')) for t in tags]
    times = [f32(i / FPS) for i in range(fc)]
    tagpos = []
    for t in tags:
        rest = _local(doc['nodes'][t])[12:15]
        smp = channels.get((t, 'translation'))
        tagpos.append([smp.at(times[f], 3) if smp else rest for f in range(fc)])
    # frame transforms: EE_model's exact matrices, else the node animation, else the node's own transform
    tf = ext.get('transforms')
    if isinstance(tf, list) and len(tf) == fc and all(_f32list(x, 16) is not None for x in tf):
        tf = [_f32list(x, 16) for x in tf]
    else:
        ts, rs, ss = (channels.get((ni, p)) for p in ('translation', 'rotation', 'scale'))
        if ts or rs or ss:
            t0, r0q, s0 = _trs(node)
            tf = [compose(ts.at(times[f], 3) if ts else t0, rs.at(times[f], 4) if rs else r0q,
                          ss.at(times[f], 3) if ss else s0) for f in range(fc)]
        else:
            tf = [_local(node)] * fc
    if correction is not None:
        tf = [[f32(x) for x in matmul(correction, t)] for t in tf]
    for f in range(fc):
        m.frames.append(cem.Frame(array('f', [0.0]), frame_vertices[f],
                                  array('f', [x for p in tagpos for x in p[f]]), from_gltf_matrix(tf[f]),
                                  array('f', compute_bbox(frame_vertices[f], vc))))
    center = _f32list(ext.get('center'), 3)
    m.center = array('f', center if center is not None else compute_center(m))
    radii = _f32list(ext.get('radii'), fc)
    for f, r in enumerate(radii if radii is not None else compute_radii(m)):
        m.frames[f].radius = array('f', [r])
    for c in subs:
        m.children.append(_import_model(doc, bufs, c, channels, None))
    return m


def _sparse_indices(doc, bufs, a):
    sp = a['sparse']
    iv = doc['bufferViews'][sp['indices']['bufferView']]
    icode, isize = _COMP[sp['indices']['componentType']]
    base = iv.get('byteOffset', 0) + sp['indices'].get('byteOffset', 0)
    return [struct.unpack_from('<' + icode, bufs[iv['buffer']], base + k * isize)[0] for k in range(sp['count'])]


def compute_bbox(v, vc):
    """GEModel::ComputeBoundingBoxes for one frame (its else-if order included)."""
    mn = list(v[0:3])
    mx = list(v[0:3])
    for j in range(vc):
        for c in range(3):
            x = v[8 * j + c]
            if x < mn[c]:
                mn[c] = x
            elif x > mx[c]:
                mx[c] = x
    return mn + mx


def compute_center(m):
    """GEModel::ComputeBoundingVolumes' centre: the mean of every frame's box corners (float32 steps)."""
    s = f32(1.0 / f32(len(m.frames) * 2.0))
    acc = [0.0, 0.0, 0.0]
    for fr in m.frames:
        b = fr.bbox
        for c in range(3):
            acc[c] = f32(acc[c] + f32(b[c] + b[c + 3]))
    return [f32(s * a) for a in acc]


def compute_radii(m):
    c = m.center
    out = []
    for fr in m.frames:
        v = fr.vertices
        best = 0.0
        for j in range(m.vertex_count):
            d = f32(f32(f32(f32(v[8 * j] - c[0]) ** 2) + f32(f32(v[8 * j + 1] - c[1]) ** 2))
                    + f32(f32(v[8 * j + 2] - c[2]) ** 2))
            if d > best:
                best = d
        out.append(f32(math.sqrt(best)))
    return out
