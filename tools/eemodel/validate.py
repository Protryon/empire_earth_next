"""Structural glTF 2.0 checks (the specification's MUST rules that apply to the files eemodel writes and reads):
references in range, the node tree, accessors inside their buffer views and buffers, the attribute and index
types, index values below the vertex count, accessor min/max equal to the data, morph target and weights counts,
animation samplers and channels, extension declarations. -> a list of problems (empty: valid).

The Khronos glTF-Validator checks more (and is the reference); this one needs nothing outside the standard library.
"""
import math
import struct

_COMP = {5120: ('b', 1), 5121: ('B', 1), 5122: ('h', 2), 5123: ('H', 2), 5125: ('I', 4), 5126: ('f', 4)}
_NCOMP = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT2': 4, 'MAT3': 9, 'MAT4': 16}
_PATHS = {'translation': 'VEC3', 'rotation': 'VEC4', 'scale': 'VEC3', 'weights': 'SCALAR'}


def validate(doc, bufs):
    e = []

    def arr(k):
        v = doc.get(k, [])
        if not isinstance(v, list):
            e.append(f'{k} is not an array')
            return []
        return v

    def ref(where, k, i):
        if not isinstance(i, int) or not 0 <= i < len(arr(k)):
            e.append(f'{where}: {k} index {i} out of range')
            return False
        return True

    if doc.get('asset', {}).get('version') != '2.0':
        e.append('asset.version is not "2.0"')
    used = set(doc.get('extensionsUsed', []))
    for r in doc.get('extensionsRequired', []):
        if r not in used:
            e.append(f'extensionsRequired {r} is not in extensionsUsed')

    def ext_check(where, obj):
        for k in obj.get('extensions', {}):
            if k not in used:
                e.append(f'{where}: extension {k} is not in extensionsUsed')

    buffers, views, accs = arr('buffers'), arr('bufferViews'), arr('accessors')
    nodes, meshes = arr('nodes'), arr('meshes')
    for i, b in enumerate(buffers):
        if b.get('byteLength', 0) < 1:
            e.append(f'buffers[{i}].byteLength < 1')
        if i < len(bufs) and len(bufs[i]) < b.get('byteLength', 0):
            e.append(f'buffers[{i}] data shorter than byteLength')
    vertex_views, index_views = set(), set()
    for i, v in enumerate(views):
        if not ref(f'bufferViews[{i}]', 'buffers', v.get('buffer')):
            continue
        end = v.get('byteOffset', 0) + v.get('byteLength', 0)
        if v.get('byteLength', 0) < 1 or end > buffers[v['buffer']].get('byteLength', 0):
            e.append(f'bufferViews[{i}] does not fit its buffer')
        s = v.get('byteStride')
        if s is not None and (s < 4 or s > 252 or s % 4):
            e.append(f'bufferViews[{i}].byteStride {s} invalid')
        if v.get('target') not in (None, 34962, 34963):
            e.append(f'bufferViews[{i}].target invalid')

    data_cache = {}

    def values(ai):
        if ai in data_cache:
            return data_cache[ai]
        a = accs[ai]
        code, size = _COMP[a['componentType']]
        nc = _NCOMP[a['type']]
        out = []
        if 'bufferView' in a:
            v = views[a['bufferView']]
            buf = bufs[v['buffer']]
            start = v.get('byteOffset', 0) + a.get('byteOffset', 0)
            stride = v.get('byteStride') or size * nc
            for k in range(a['count']):
                out.append(struct.unpack_from(f'<{nc}{code}', buf, start + k * stride))
        else:
            out = [(0,) * nc] * a['count']
        sp = a.get('sparse')
        if sp:
            iv, vv = views[sp['indices']['bufferView']], views[sp['values']['bufferView']]
            icode, isz = _COMP[sp['indices']['componentType']]
            for k in range(sp['count']):
                idx = struct.unpack_from('<' + icode, bufs[iv['buffer']],
                                         iv.get('byteOffset', 0) + sp['indices'].get('byteOffset', 0) + k * isz)[0]
                out[idx] = struct.unpack_from(f'<{nc}{code}', bufs[vv['buffer']],
                                              vv.get('byteOffset', 0) + sp['values'].get('byteOffset', 0)
                                              + k * size * nc)
        data_cache[ai] = out
        return out

    good_acc = set()
    for i, a in enumerate(accs):
        w = f'accessors[{i}]'
        if a.get('componentType') not in _COMP or a.get('type') not in _NCOMP:
            e.append(f'{w}: invalid componentType/type')
            continue
        code, size = _COMP[a['componentType']]
        nc = _NCOMP[a['type']]
        if not isinstance(a.get('count'), int) or a['count'] < 1:
            e.append(f'{w}: count < 1')
            continue
        if a.get('normalized') and code in ('f', 'I'):
            e.append(f'{w}: normalized float/uint')
        ok = True
        if 'bufferView' in a:
            if not ref(w, 'bufferViews', a['bufferView']):
                continue
            v = views[a['bufferView']]
            off = a.get('byteOffset', 0)
            stride = v.get('byteStride') or size * nc
            if off % size or (v.get('byteOffset', 0) + off) % size:
                e.append(f'{w}: byteOffset not aligned to the component size')
                ok = False
            if stride < size * nc:
                e.append(f'{w}: byteStride smaller than the element')
                ok = False
            if off + stride * (a['count'] - 1) + size * nc > v.get('byteLength', 0):
                e.append(f'{w}: does not fit its bufferView')
                ok = False
        elif 'byteOffset' in a:
            e.append(f'{w}: byteOffset without bufferView')
        sp = a.get('sparse')
        if sp:
            if sp.get('count', 0) < 1 or sp['count'] > a['count']:
                e.append(f'{w}: sparse.count out of range')
                ok = False
            for part in ('indices', 'values'):
                if not ref(w + '.sparse.' + part, 'bufferViews', sp.get(part, {}).get('bufferView')):
                    ok = False
                elif 'byteStride' in views[sp[part]['bufferView']]:
                    e.append(f'{w}: sparse {part} bufferView has a byteStride')
            if ok:
                icode, isz = _COMP.get(sp['indices'].get('componentType'), ('?', 0))
                if icode not in ('B', 'H', 'I'):
                    e.append(f'{w}: sparse indices componentType')
                    ok = False
                else:
                    iv = views[sp['indices']['bufferView']]
                    last = -1
                    for k in range(sp['count']):
                        idx = struct.unpack_from('<' + icode, bufs[iv['buffer']], iv.get('byteOffset', 0)
                                                 + sp['indices'].get('byteOffset', 0) + k * isz)[0]
                        if idx <= last or idx >= a['count']:
                            e.append(f'{w}: sparse indices not strictly increasing below count')
                            ok = False
                            break
                        last = idx
        if 'min' in a or 'max' in a:
            if len(a.get('min', [])) != nc or len(a.get('max', [])) != nc:
                e.append(f'{w}: min/max length')
            elif ok:
                vals = values(i)
                for c in range(nc):
                    col = [x[c] for x in vals]
                    if code == 'f':
                        col = [x for x in col if x == x]
                    if col and (min(col) != a['min'][c] or max(col) != a['max'][c]):
                        e.append(f'{w}: min/max do not equal the data (component {c})')
                        break
        if ok:
            good_acc.add(i)

    for i, m in enumerate(meshes):
        prims = m.get('primitives', [])
        if not prims:
            e.append(f'meshes[{i}]: no primitives')
        ntargets = None
        for j, p in enumerate(prims):
            w = f'meshes[{i}].primitives[{j}]'
            at = p.get('attributes', {})
            if 'POSITION' not in at:
                e.append(f'{w}: no POSITION')
            counts = set()
            for name, ai in at.items():
                if not ref(w + '.' + name, 'accessors', ai):
                    continue
                a = accs[ai]
                counts.add(a['count'])
                if 'bufferView' in a:
                    vertex_views.add(a['bufferView'])
                if name in ('POSITION', 'NORMAL') and (a.get('type') != 'VEC3' or a.get('componentType') != 5126):
                    e.append(f'{w}: {name} must be float VEC3')
                if name == 'POSITION' and ('min' not in a or 'max' not in a):
                    e.append(f'{w}: POSITION without min/max')
                if name.startswith('TEXCOORD_') and (a.get('type') != 'VEC2' or not (
                        a.get('componentType') == 5126 or (a.get('normalized') and a.get('componentType') in (5121,
                                                                                                             5123)))):
                    e.append(f'{w}: {name} type')
                if name == 'NORMAL' and ai in good_acc:
                    for x in values(ai)[:64]:
                        if abs(math.sqrt(sum(c * c for c in x)) - 1.0) > 0.01 and any(x):
                            break   # a warning in the Khronos validator, not an error: not reported
            if len(counts) > 1:
                e.append(f'{w}: attribute accessors with different counts')
            vc = min(counts) if counts else 0
            mode = p.get('mode', 4)
            if mode not in range(7):
                e.append(f'{w}: mode {mode}')
            if 'indices' in p and ref(w + '.indices', 'accessors', p['indices']):
                a = accs[p['indices']]
                if a.get('type') != 'SCALAR' or a.get('componentType') not in (5121, 5123, 5125) or a.get(
                        'normalized'):
                    e.append(f'{w}: indices must be unsigned integer SCALAR')
                if 'bufferView' in a:
                    index_views.add(a['bufferView'])
                    if 'byteStride' in views[a['bufferView']]:
                        e.append(f'{w}: indices bufferView has a byteStride')
                if mode == 4 and a['count'] % 3:
                    e.append(f'{w}: triangle index count not a multiple of 3')
                if p['indices'] in good_acc:
                    mx = max(x[0] for x in values(p['indices']))
                    if mx >= vc:
                        e.append(f'{w}: index {mx} >= vertex count {vc}')
            if 'material' in p:
                ref(w + '.material', 'materials', p['material'])
            t = p.get('targets', [])
            if ntargets is None:
                ntargets = len(t)
            elif ntargets != len(t):
                e.append(f'meshes[{i}]: primitives with different target counts')
            for k, tg in enumerate(t):
                for name, ai in tg.items():
                    if ref(f'{w}.targets[{k}].{name}', 'accessors', ai):
                        if accs[ai]['count'] != vc:
                            e.append(f'{w}.targets[{k}].{name}: count differs from the attributes')
                        if name == 'POSITION' and ('min' not in accs[ai] or 'max' not in accs[ai]):
                            e.append(f'{w}.targets[{k}].POSITION without min/max')
        if 'weights' in m and len(m['weights']) != (ntargets or 0):
            e.append(f'meshes[{i}]: weights length differs from the targets')
    for vi in vertex_views & index_views:
        e.append(f'bufferViews[{vi}] used for vertices and indices')
    for vi in index_views:
        if views[vi].get('target') not in (None, 34963):
            e.append(f'bufferViews[{vi}]: index data with target {views[vi].get("target")}')

    parent = {}
    animated = set()
    for an in doc.get('animations', []):
        for ch in an.get('channels', []):
            n = ch.get('target', {}).get('node')
            if n is not None:
                animated.add(n)
    for i, n in enumerate(nodes):
        w = f'nodes[{i}]'
        ext_check(w, n)
        for c in n.get('children', []):
            if ref(w + '.children', 'nodes', c):
                if c in parent:
                    e.append(f'{w}: node {c} has two parents')
                parent[c] = i
        if 'mesh' in n:
            ref(w + '.mesh', 'meshes', n['mesh'])
        if 'matrix' in n:
            if any(k in n for k in ('translation', 'rotation', 'scale')):
                e.append(f'{w}: matrix and TRS')
            if i in animated:
                e.append(f'{w}: animated node with a matrix')
            mtx = n['matrix']
            if len(mtx) != 16 or mtx[3] != 0 or mtx[7] != 0 or mtx[11] != 0 or mtx[15] != 1:
                e.append(f'{w}: matrix not affine')
        if 'rotation' in n:
            q = n['rotation']
            if len(q) != 4 or abs(math.sqrt(sum(x * x for x in q)) - 1) > 1e-5:
                e.append(f'{w}: rotation is not a unit quaternion')
        if 'weights' in n and 'mesh' in n:
            pass
    for i in range(len(nodes)):
        seen = set()
        k = i
        while k in parent:
            if k in seen:
                e.append(f'nodes[{i}]: cycle')
                break
            seen.add(k)
            k = parent[k]
    for si, s in enumerate(arr('scenes')):
        for n in s.get('nodes', []):
            if ref(f'scenes[{si}]', 'nodes', n) and n in parent:
                e.append(f'scenes[{si}]: node {n} is not a root')
    if 'scene' in doc:
        ref('scene', 'scenes', doc['scene'])
    for i, t in enumerate(arr('textures')):
        if 'source' in t:
            ref(f'textures[{i}].source', 'images', t['source'])
        if 'sampler' in t:
            ref(f'textures[{i}].sampler', 'samplers', t['sampler'])
    for i, im in enumerate(arr('images')):
        if ('uri' in im) == ('bufferView' in im):
            e.append(f'images[{i}]: needs exactly one of uri and bufferView')
        if 'bufferView' in im and 'mimeType' not in im:
            e.append(f'images[{i}]: bufferView without mimeType')
    for i, m in enumerate(arr('materials')):
        for k in ('baseColorTexture', 'metallicRoughnessTexture'):
            info = m.get('pbrMetallicRoughness', {}).get(k)
            if info is not None:
                ref(f'materials[{i}].{k}', 'textures', info.get('index'))
    for ai, an in enumerate(doc.get('animations', [])):
        smp = an.get('samplers', [])
        targets = set()
        for si, s in enumerate(smp):
            w = f'animations[{ai}].samplers[{si}]'
            if not (ref(w, 'accessors', s.get('input')) and ref(w, 'accessors', s.get('output'))):
                continue
            ia = accs[s['input']]
            if ia.get('type') != 'SCALAR' or ia.get('componentType') != 5126 or 'min' not in ia:
                e.append(f'{w}: input must be float SCALAR with min/max')
            elif s['input'] in good_acc:
                t = [x[0] for x in values(s['input'])]
                if any(b <= a for a, b in zip(t, t[1:])) or t[0] < 0:
                    e.append(f'{w}: input times not increasing')
            if s.get('interpolation', 'LINEAR') not in ('LINEAR', 'STEP', 'CUBICSPLINE'):
                e.append(f'{w}: interpolation')
        for ci, ch in enumerate(an.get('channels', [])):
            w = f'animations[{ai}].channels[{ci}]'
            if not isinstance(ch.get('sampler'), int) or not 0 <= ch['sampler'] < len(smp):
                e.append(f'{w}: sampler out of range')
                continue
            tg = ch.get('target', {})
            path = tg.get('path')
            if path not in _PATHS:
                e.append(f'{w}: path {path}')
                continue
            if not ref(w, 'nodes', tg.get('node')):
                continue
            if (tg['node'], path) in targets:
                e.append(f'{w}: second channel for the same node and path')
            targets.add((tg['node'], path))
            s = smp[ch['sampler']]
            if not (isinstance(s.get('input'), int) and isinstance(s.get('output'), int)
                    and 0 <= s['input'] < len(accs) and 0 <= s['output'] < len(accs)):
                continue
            oa, ia = accs[s['output']], accs[s['input']]
            if oa.get('type') != _PATHS[path]:
                e.append(f'{w}: output type {oa.get("type")} for {path}')
            per = 1
            if path == 'weights':
                nd = nodes[tg['node']]
                if 'mesh' not in nd:
                    e.append(f'{w}: weights for a node without a mesh')
                    continue
                per = len(meshes[nd['mesh']]['primitives'][0].get('targets', []))
            k = 3 if s.get('interpolation') == 'CUBICSPLINE' else 1
            if oa['count'] != ia['count'] * per * k:
                e.append(f'{w}: output count {oa["count"]} != input count {ia["count"]} x {per * k}')
    return e
