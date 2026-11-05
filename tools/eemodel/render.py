"""A small software rasterizer for spot checks (eemodel.py render): draws a model into a PNG, z-buffered, textured
(nearest texel), lit by one directional light, from a fixed three-quarter view with an orthographic camera fitted to
the model.

Two independent paths: a .cem is drawn the way the game composes it (DX7Rasterizer::DrawModelHierarchy: a sub-model's
world transform is its parent's times its own frame transform; the vertices of the frame), turned to glTF's Y-up
axes; a .gltf/.glb is drawn by glTF semantics (node matrices or TRS, morph targets weighted for the frame, node
animation sampled at the frame's time). Both give the same picture when the glTF says what the game does.
"""
import math
import pathlib
import sys
import urllib.parse

import cem
import gltf
import png
import sst


def _mul(m, p):
    """Column-major 4x4 times a point (w = 1) or a direction (w = 0)."""
    x, y, z, w = p
    return [m[0] * x + m[4] * y + m[8] * z + m[12] * w, m[1] * x + m[5] * y + m[9] * z + m[13] * w,
            m[2] * x + m[6] * y + m[10] * z + m[14] * w]


def triangles_from_cem(root, frame, texture_for):
    """-> [(3 positions, 3 normals, 3 uvs, texture key)] in glTF scene space."""
    out = []

    def walk(m, parent):
        f = m.frames[min(frame, len(m.frames) - 1)]
        world = gltf.matmul(parent, gltf.to_gltf_matrix(f.transform))
        v = f.vertices
        for mat in m.materials:
            s, n = mat.ranges[0]
            tex = texture_for(mat)
            for t in range(s, s + n):
                idx = [mat.vertex_start + k for k in m.lods[0][3 * t:3 * t + 3]]
                out.append(([_mul(world, list(v[8 * i:8 * i + 3]) + [1.0]) for i in idx],
                            [_mul(world, list(v[8 * i + 3:8 * i + 6]) + [0.0]) for i in idx],
                            [tuple(v[8 * i + 6:8 * i + 8]) for i in idx], tex))
        for c in m.children:
            walk(c, world)
    walk(root, gltf.AXES)
    return out


def triangles_from_gltf(doc, bufs, frame, texture_for):
    out = []
    t = frame / gltf.FPS
    ch = {}
    for an in doc.get('animations', [])[:1]:
        for c in an.get('channels', []):
            ch[(c['target']['node'], c['target']['path'])] = gltf._Sampler(doc, bufs, an['samplers'][c['sampler']])

    def local(i):
        n = doc['nodes'][i]
        if 'matrix' in n:
            return [float(x) for x in n['matrix']]
        tr = ch[(i, 'translation')].at(t, 3) if (i, 'translation') in ch else n.get('translation', [0, 0, 0])
        ro = ch[(i, 'rotation')].at(t, 4) if (i, 'rotation') in ch else n.get('rotation', [0, 0, 0, 1])
        sc = ch[(i, 'scale')].at(t, 3) if (i, 'scale') in ch else n.get('scale', [1, 1, 1])
        return gltf.compose(tr, ro, sc)

    def walk(i, parent):
        n = doc['nodes'][i]
        world = gltf.matmul(parent, local(i))
        if 'mesh' in n:
            mesh = doc['meshes'][n['mesh']]
            nt = len(mesh['primitives'][0].get('targets', []))
            if (i, 'weights') in ch:
                w = ch[(i, 'weights')].at(t, nt)
            else:
                w = mesh.get('weights', [0.0] * nt)
            for p in mesh['primitives']:
                at = p['attributes']
                pos = gltf.read_accessor(doc, bufs, at['POSITION'])
                nor = gltf.read_accessor(doc, bufs, at['NORMAL']) if 'NORMAL' in at else [0.0] * len(pos)
                uv = gltf.read_accessor(doc, bufs, at['TEXCOORD_0']) if 'TEXCOORD_0' in at else [0.0] * (
                    len(pos) // 3 * 2)
                for k, tg in enumerate(p.get('targets', [])):
                    if w[k]:
                        if 'POSITION' in tg:
                            d = gltf.read_accessor(doc, bufs, tg['POSITION'])
                            pos = [a + w[k] * b for a, b in zip(pos, d)]
                        if 'NORMAL' in tg:
                            d = gltf.read_accessor(doc, bufs, tg['NORMAL'])
                            nor = [a + w[k] * b for a, b in zip(nor, d)]
                idx = gltf.read_accessor(doc, bufs, p['indices']) if 'indices' in p else list(range(len(pos) // 3))
                tex = texture_for(doc['materials'][p['material']] if 'material' in p else {})
                for q in range(0, len(idx), 3):
                    tri = idx[q:q + 3]
                    out.append(([_mul(world, pos[3 * j:3 * j + 3] + [1.0]) for j in tri],
                                [_mul(world, nor[3 * j:3 * j + 3] + [0.0]) for j in tri],
                                [tuple(uv[2 * j:2 * j + 2]) for j in tri], tex))
        for c in n.get('children', []):
            walk(c, world)
    scene = doc['scenes'][doc.get('scene', 0)]
    for r in scene['nodes']:
        walk(r, list(gltf.IDENTITY))
    return out


def rasterize(tris, textures, size):
    """-> RGB bytes (size x size)."""
    yaw, pitch = math.radians(35), math.radians(-30)
    cy, sy, cp, sp = math.cos(yaw), math.sin(yaw), math.cos(pitch), math.sin(pitch)

    def view(p):
        x, y, z = p
        x, z = cy * x + sy * z, -sy * x + cy * z
        y, z = cp * y - sp * z, sp * y + cp * z
        return x, y, z
    vt = [([view(p) for p in ps], ns, uvs, tex) for ps, ns, uvs, tex in tris]
    pts = [p for ps, _, _, _ in vt for p in ps if all(map(math.isfinite, p))]
    if not pts:
        return bytes(3 * size * size)
    xs, ys = [p[0] for p in pts], [p[1] for p in pts]
    scale = 0.9 * size / max(max(xs) - min(xs), max(ys) - min(ys), 1e-9)
    cx, cyy = (max(xs) + min(xs)) / 2, (max(ys) + min(ys)) / 2
    img = bytearray(b'\x30\x30\x38' * (size * size))
    zb = [math.inf] * (size * size)
    light = view((0.4, 0.8, 0.45))
    ln = math.sqrt(sum(c * c for c in light))
    light = [c / ln for c in light]
    for ps, ns, uvs, tex in vt:
        sc = [((p[0] - cx) * scale + size / 2, size / 2 - (p[1] - cyy) * scale, p[2]) for p in ps]
        if not all(math.isfinite(c) for s in sc for c in s):
            continue
        (x0, y0, z0), (x1, y1, z1), (x2, y2, z2) = sc
        area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
        if area == 0:
            continue
        nv = [view(n) for n in ns]
        timg = textures.get(tex)
        lo_x, hi_x = max(0, int(min(x0, x1, x2))), min(size - 1, int(max(x0, x1, x2)) + 1)
        lo_y, hi_y = max(0, int(min(y0, y1, y2))), min(size - 1, int(max(y0, y1, y2)) + 1)
        for py in range(lo_y, hi_y + 1):
            fy = py + 0.5
            for px in range(lo_x, hi_x + 1):
                fx = px + 0.5
                w0 = ((x1 - fx) * (y2 - fy) - (x2 - fx) * (y1 - fy)) / area
                w1 = ((x2 - fx) * (y0 - fy) - (x0 - fx) * (y2 - fy)) / area
                w2 = 1.0 - w0 - w1
                if w0 < 0 or w1 < 0 or w2 < 0:
                    continue
                z = w0 * z0 + w1 * z1 + w2 * z2
                k = py * size + px
                if z >= zb[k]:
                    continue
                n = [w0 * nv[0][c] + w1 * nv[1][c] + w2 * nv[2][c] for c in range(3)]
                nl = math.sqrt(sum(c * c for c in n)) or 1.0
                d = abs(sum(n[c] * light[c] for c in range(3)) / nl)
                if not math.isfinite(d):
                    d = 0.0
                shade = 0.35 + 0.65 * d
                if timg:
                    u = w0 * uvs[0][0] + w1 * uvs[1][0] + w2 * uvs[2][0]
                    v = w0 * uvs[0][1] + w1 * uvs[1][1] + w2 * uvs[2][1]
                    tw, th, tc, tp = timg
                    tx = int(math.floor((u % 1.0) * tw)) % tw if math.isfinite(u) else 0
                    ty = int(math.floor((v % 1.0) * th)) % th if math.isfinite(v) else 0
                    o = (ty * tw + tx) * tc
                    if tc == 4 and tp[o + 3] < 128:
                        continue
                    col = tp[o:o + 3]
                else:
                    col = (200, 200, 200)
                zb[k] = z
                img[3 * k:3 * k + 3] = bytes(min(255, int(c * shade)) for c in col)
    return bytes(img)


def _load_texture(path):
    p = pathlib.Path(path)
    if not p.is_file():
        return None
    if p.suffix.lower() == '.png':
        w, h, c, px, _ = png.read(p.read_bytes())
        return w, h, c, px
    t = sst.read(p.read_bytes(), p.name)
    img = t.frames[0]
    return img.width, img.height, img.channels, img.pixels


def main(a):
    src = pathlib.Path(a.input)
    textures = {}
    if src.suffix.lower() == '.cem':
        root = cem.read(src.read_bytes())
        tdir = pathlib.Path(a.textures) if a.textures else None

        def texture_for(mat):
            name = a.texture or cem.cstr(mat.texture).decode('cp1252')
            if tdir is None or not name or not cem.cstr(mat.texture):
                return None
            k = (name, mat.texture_index)
            if k not in textures:
                textures[k] = _load_texture(tdir / sst.frame_name(name, mat.texture_index))
            return k
        tris = triangles_from_cem(root, a.frame, texture_for)
    else:
        doc, bufs = gltf.load(src)

        def texture_for(mat):
            info = mat.get('pbrMetallicRoughness', {}).get('baseColorTexture')
            if not info:
                return None
            img = doc['images'][doc['textures'][info['index']]['source']]
            uri = urllib.parse.unquote(img.get('uri', ''))
            if not uri or uri.startswith('data:'):
                return None
            if uri not in textures:
                textures[uri] = _load_texture(src.parent / uri)
            return uri
        tris = triangles_from_gltf(doc, bufs, a.frame, texture_for)
    rgb = rasterize(tris, textures, a.size)
    pathlib.Path(a.output).write_bytes(png.write(a.size, a.size, 3, rgb))
    print(f'{len(tris)} triangles, {sum(1 for t in textures.values() if t)} textures -> {a.output}')
    return 0


if __name__ == '__main__':
    sys.exit('use eemodel.py render')
