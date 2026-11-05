"""The .cem 1.x formats (versions 1.0-1.3): reader (LoadLegacyVersion, 'GEModel Legacy.cpp') and the conversion to the
2.0 model the game builds from them in memory (ConvertLegacyModel), docs/MODEL_FORMATS.md section 2.4.

The conversion follows the C++ step by step (including its quirks: a vertex used by two materials is copied into
both, a polygon whose first two corners name the same vertex keeps the second corner's old index, polygon indices
end up relative to their material's first vertex). `check_geometry` is an independent check of the result: every
converted polygon's corners carry the position, normal and texture coordinate of the legacy polygon they came from.
"""
import math
import struct
from array import array

import cem

PLAYER_COLOR = b'player color'


def _f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


_QUANTIZED = None


def quantized_vectors():
    """GEVector::QuantizedVectorArray: 41 x (6 x 41) unit directions on the cube faces, flattened (index = row * 246 +
    column). Each step is rounded to float32 (the x64 build's SSE arithmetic; the i686 build's x87 intermediates can
    differ in the last bit)."""
    global _QUANTIZED
    if _QUANTIZED is not None:
        return _QUANTIZED
    step = _f32(0.05)
    t = [[None] * 246 for _ in range(41)]

    def norm(x, y, z):
        mag = _f32(math.sqrt(_f32(_f32(_f32(x * x) + _f32(y * y)) + _f32(z * z))))
        if -0.0001 <= mag <= 0.0001:   # the comparisons are against float constants: exact enough here (|v| >= 1)
            return (0.0, 0.0, 0.0)
        s = _f32(1.0 / mag)
        return (_f32(x * s), _f32(y * s), _f32(z * s))

    for a in range(41):
        first = _f32(_f32(a * step) - 1.0)
        for b in range(41):
            second = _f32(_f32(b * step) - 1.0)
            x, y, z = norm(-1.0, first, second)
            t[a][0 * 41 + b] = (x, y, z)
            t[a][3 * 41 + b] = (-x, y, z)
            x, y, z = norm(first, -1.0, second)
            t[a][1 * 41 + b] = (x, y, z)
            t[a][4 * 41 + b] = (x, -y, z)
            x, y, z = norm(first, second, -1.0)
            t[a][2 * 41 + b] = (x, y, z)
            t[a][5 * 41 + b] = (x, y, -z)
    _QUANTIZED = [v for row in t for v in row]
    return _QUANTIZED


class Legacy:
    """The 1.x data as LoadLegacy* leaves it (GEModel +0x160..+0x184 and the arrays it fills)."""

    def __init__(self):
        self.polygons = []          # [(vertex indices (3), texture coordinates ((u, v) x 3))]
        self.groups = []            # [(name, [polygon indices])] (the game keeps only "player color" groups)
        self.material_polygons = [] # [[polygon indices]] per material
        self.textures = []          # texture file name per material (b'' when it has none)
        self.vertex_points = []     # point index per vertex
        self.points = []            # [array('f') pointCount x 3] per frame
        self.normals = []           # [[(x, y, z)] per vertex] per frame


def read_body(r, major, minor):
    """After the header: LoadLegacyVersion's chunks -> a cem.Model in the 2.0 layout (converted), .legacy kept."""
    v10 = major == 1 and minor == 0
    m = cem.Model()
    m.major, m.minor = major, minor
    lg = m.legacy = Legacy()
    # LoadLegacyCountsChunk
    fc, mc, pc, polys, gc, vc = (r.u32() for _ in range(6))
    if fc == 0 or mc == 0 or pc == 0 or polys == 0 or vc == 0:
        raise cem.CemError('zero frame, material, point, polygon or vertex count')
    tc = sub = 0
    if not v10:
        tc = r.u32()
        if major != 1 or minor >= 2:
            sub = r.u32()
    m._child_count = sub
    m.polygon_count = polys
    # LoadLegacyMiscellaneousChunk: name, centre, an unread flag, (1.3) one unread value per point
    m.name = r.string()
    m.center = r.floats(3)
    r.u8()
    if major != 1 or minor >= 3:
        r.take(4 * pc)
    # LoadLegacyPolygonsChunk: per corner a vertex index, a texture coordinate, colour/alpha/lighting (unread)
    for _ in range(polys):
        vi, uv = [], []
        for _ in range(3):
            vi.append(r.u32())
            u, v = struct.unpack('<ff', r.take(8))
            uv.append((u, v))
            r.take(4 * 7)
        lg.polygons.append((vi, uv))
    # LoadLegacyPolygonGroupsChunk
    for _ in range(gc):
        name = r.string()
        n = r.u32()
        idx = list(r.u32s(n))
        lg.groups.append((name, idx))
    # LoadLegacyMaterialsChunk
    for _ in range(mc):
        n = r.u32()
        lg.material_polygons.append(list(r.u32s(n)))
        has = r.u8()
        tex = b''
        if has:
            tex = r.string()
            r.u32()   # a texture index, read and dropped
        lg.textures.append(tex)
    # LoadLegacyVerticesChunk: point index + an unread float per vertex
    for _ in range(vc):
        lg.vertex_points.append(r.u32())
        r.take(4)
    # LoadTagPointsChunk (not 1.0)
    if not v10:
        m.tag_names = [r.string() for _ in range(tc)]
    # LoadLegacyFrameChunk
    qv = None
    radii, tags, transforms, boxes = [], [], [], []
    identity = array('f', [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1])
    for _ in range(fc):
        radii.append(r.floats(1))
        lg.points.append(r.floats(3 * pc))
        ns = []
        for _ in range(vc):
            if major == 1 and minor < 3:
                ns.append(struct.unpack('<fff', r.take(12)))
            else:
                if qv is None:
                    qv = quantized_vectors()
                i = r.u16()
                ns.append(qv[i] if i < len(qv) else (0.0, 0.0, 0.0))
        lg.normals.append(ns)
        if major == 1 and minor < 3:
            r.take(12 * polys)
        t = array('f', [0.0] * (3 * tc))
        tr = array('f', identity)
        bb = array('f', [0.0, 0.0, 0.0, 1.0, 1.0, 1.0])
        if not v10:
            t = r.floats(3 * tc)
            if not (major == 1 and minor < 2):
                tr = r.floats(16)
                if major != 1 or minor >= 3:
                    bb = r.floats(6)
        tags.append(t)
        transforms.append(tr)
        boxes.append(bb)
    _convert(m, radii, tags, transforms, boxes)
    return m


def _convert(m, radii, tags, transforms, boxes):
    """ConvertLegacyModel (no shared frames: the file's view)."""
    lg = m.legacy
    np_ = len(lg.polygons)
    nv = len(lg.vertex_points)
    fc = len(lg.points)
    poly = [list(p[0]) for p in lg.polygons]                     # mPolygonArrays[0], legacy order
    # per frame: points, normals; texture coordinates from the polygons' corners (later polygons win)
    frames = []
    for f in range(fc):
        pts = lg.points[f]
        vs = []
        for i in range(nv):
            pi = lg.vertex_points[i]
            vs.append([pts[3 * pi], pts[3 * pi + 1], pts[3 * pi + 2], *lg.normals[f][i], 0.0, 0.0])
        for vi, uv in lg.polygons:
            for k in range(3):
                vs[vi[k]][6], vs[vi[k]][7] = uv[k]
        frames.append(vs)
    # split the vertices whose corners disagree on the texture coordinate
    new = []                                                      # [(uv, source vertex)]
    for i in range(nv):
        coord = (frames[0][i][6], frames[0][i][7])
        split = []                                                # [[uv, vertex, [(polygon, corner)]]]
        for j, (vi, uv) in enumerate(lg.polygons):
            k = next((k for k in range(3) if vi[k] == i), 3)
            if k >= 3 or uv[k] == coord:
                continue
            for s in split:
                if uv[k] == s[0]:
                    s[2].append((j, k))
                    break
            else:
                split.append([uv[k], i, [(j, k)]])
        for s in split:
            new.append((s[0], s[1]))
            for j, k in s[2]:
                poly[j][k] = len(new) + nv - 1
    for f in range(fc):
        vs = frames[f]
        for uv, src in new:
            vs.append([*vs[src][:6], uv[0], uv[1]])
    total = nv + len(new)
    # groups: one per material that has polygons, split where some of its polygons take the player colour
    player = [idx for name, idx in lg.groups if cem.cstr(name) == PLAYER_COLOR]
    groups = []                                                   # [[name, texture, polygons, texture index]]
    tex_index = 0
    for i, mp in enumerate(lg.material_polygons):
        if not mp:
            continue
        g = [b'', lg.textures[i], [], tex_index]
        tex_index += 1
        groups.append(g)
        count = sum(1 for pg in player for p in pg for q in mp if q == p)
        if count:
            if count == len(mp):
                g[0] = PLAYER_COLOR
                g[2].extend(mp)
            else:
                h = [PLAYER_COLOR, lg.textures[i], [], g[3]]
                groups.append(h)
                for q in mp:
                    hits = sum(1 for pg in player for p in pg if p == q)
                    if hits:
                        h[2].extend([q] * hits)
                    else:
                        g[2].append(q)
        else:
            g[2].extend(mp)
    # reorder the polygons by group
    before = [list(p) for p in poly]
    order = [q for g in groups for q in g[2]]
    if len(order) > np_:
        raise cem.CemError(f'legacy conversion writes {len(order)} polygons into an array of {np_}')
    for l, q in enumerate(order):
        poly[l] = list(before[q])
    # the vertices of each group, in vertex order (a vertex used by several groups is copied into each)
    vgroups = [[] for _ in groups]
    for v in range(total):
        for k, g in enumerate(groups):
            for q in g[2]:
                if v in before[q]:
                    vgroups[k].append(v)
                    break
    newframes = []
    for f in range(fc):
        vs = frames[f]
        newframes.append([vs[v] for vg in vgroups for v in vg])
    # polygon indices -> the index within the group's vertices (first matching corner only)
    snap = [list(p) for p in poly]
    l = 0
    for i, g in enumerate(groups):
        for k, v in enumerate(vgroups[i]):
            for j in range(len(g[2])):
                c = snap[l + j]
                if c[0] == v:
                    poly[l + j][0] = k
                elif c[1] == v:
                    poly[l + j][1] = k
                elif c[2] == v:
                    poly[l + j][2] = k
        l += len(g[2])
    # the converted model
    m.lods = [array('I', [x for p in poly for x in p])]
    m.materials = []
    ps = vs_ = 0
    for i, g in enumerate(groups):
        m.materials.append(cem.Material(g[0], g[3], [(ps, len(g[2]))], vs_, len(vgroups[i]), g[1]))
        ps += len(g[2])
        vs_ += len(vgroups[i])
    m.frames = []
    for f in range(fc):
        verts = array('f')
        for v in newframes[f]:
            verts.extend(v)
        m.frames.append(cem.Frame(radii[f], verts, tags[f], transforms[f], boxes[f]))
    m._groups = groups
    m._vgroups = vgroups


def check_geometry(m):
    """Independent check of the conversion: [problem] (empty when every converted polygon of the groups carries the
    legacy polygon's corner data, in every frame)."""
    lg = m.legacy
    problems = []
    l = 0
    for gi, mat in enumerate(m.materials):
        src_polys = m._groups[gi][2]
        for j, q in enumerate(src_polys):
            vi, uv = lg.polygons[q]
            tri = m.lods[0][3 * (l + j):3 * (l + j) + 3]
            for f, fr in enumerate(m.frames):
                pts = lg.points[f]
                for k in range(3):
                    if k > 0 and vi[k] in vi[:k]:
                        continue   # a repeated corner: the game keeps a stale index there (see the module doc)
                    v = mat.vertex_start + tri[k]
                    got = fr.vertices[8 * v:8 * v + 8]
                    pi = lg.vertex_points[vi[k]]
                    want = [pts[3 * pi], pts[3 * pi + 1], pts[3 * pi + 2], *lg.normals[f][vi[k]], *uv[k]]
                    if [_f32(x) for x in want] != list(got):
                        problems.append(f'material {gi} polygon {j} corner {k} frame {f}')
        l += len(src_polys)
    return problems
