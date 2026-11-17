"""TEST ONLY: RFIL, the render fill's capture (ee/EERenderCapture.h; the object plan's Lane R gate).

A game run with EE_RENDER_CAPTURE=<absolute file> writes one record per filled frame. The live game draws on wall
time, so two runs fill different frames: frames are matched by their keys (the physics tick, the camera's
transformation and yaw, the free-camera, orthographic and ambient flags, the unit under the pointer, and a digest of
the visible tiles' units that the harness's input can change: the selection, an order), the first frame of each key
in each run.
Matched frames must be equal in every unit's and object's digest, the texture maps, the selected and picked lists,
the ghost records and the ground circles. The objects' sway fractions run on wall time (the sway clock) and are
compared only with --sway; the pointed-at unit (the mouse) is part of the frame.

  python -I tools/rfil.py summary CAPTURE
  python -I tools/rfil.py compare A B [--sway] [--min-matched N]      exit 0: every matched frame equal
"""
import argparse
import struct
import sys

TAG = 0x4C494652   # 'RFIL'


class Reader:
    def __init__(self, data):
        self.d = data
        self.i = 0

    def u32(self):
        v = struct.unpack_from('<I', self.d, self.i)[0]
        self.i += 4
        return v

    def i32(self):
        v = struct.unpack_from('<i', self.d, self.i)[0]
        self.i += 4
        return v

    def f32bits(self):
        return self.u32()

    def u64(self):
        v = struct.unpack_from('<Q', self.d, self.i)[0]
        self.i += 8
        return v


def frame(payload):
    r = Reader(payload)
    f = {}
    f['frame'] = r.u32()
    f['tick'] = r.u32()
    f['sway'] = r.u32()
    f['yaw'] = r.f32bits()
    f['camera'] = r.u64()
    f['flags'] = r.u32()
    f['picked'] = r.i32()
    f['world'] = r.u64()
    f['fighting'] = r.f32bits()
    f['count'] = r.f32bits()
    f['units'] = [(r.u32(), r.f32bits(), r.u64()) for _ in range(r.u32())]
    shadows = [(r.f32bits(), r.f32bits(), r.u64()) for _ in range(r.u32())]
    f['shadows'] = [(o, d) for o, _, d in shadows]
    f['fractions'] = [s for _, s, _ in shadows]
    maps = []
    for _ in range(6):
        maps.append(tuple((r.i32(), r.i32()) for _ in range(r.u32())))
    f['maps'] = tuple(maps)
    f['selected_units'] = tuple(r.i32() for _ in range(r.u32()))
    f['selected_objects'] = tuple(r.i32() for _ in range(r.u32()))
    f['selected_unit'] = r.i32()
    f['selected_object'] = r.i32()
    f['debug_unit'] = r.i32()
    f['debug_text'] = r.u32()
    f['rect_ids'] = tuple(r.u32() for _ in range(r.u32()))
    f['ghosts'] = tuple(r.u64() for _ in range(r.u32()))
    f['lines'] = tuple(r.u64() for _ in range(r.u32()))
    if r.i != len(payload):
        raise ValueError(f'record of {len(payload)} bytes read to {r.i}')
    return f


def read(path):
    data = open(path, 'rb').read()
    frames, i = [], 0
    while i + 8 <= len(data):
        tag, size = struct.unpack_from('<II', data, i)
        if i + 8 + size > len(data):
            break   # a record cut by the game's end
        if tag == TAG:
            frames.append(frame(data[i + 8:i + 8 + size]))
        i += 8 + size
    return frames


def key(f):
    return (f['tick'], f['camera'], f['yaw'], f['flags'], f['picked'], f['world'])


COMPARED = ['units', 'shadows', 'maps', 'selected_units', 'selected_objects', 'selected_unit', 'selected_object',
            'debug_unit', 'debug_text', 'rect_ids', 'ghosts', 'lines', 'fighting', 'count']


def first_by_key(frames):
    out = {}
    for f in frames:
        out.setdefault(key(f), f)
    return out


def differences(a, b, sway):
    names = [n for n in COMPARED if a[n] != b[n]]
    if sway and a['fractions'] != b['fractions']:
        names.append('fractions')
    return names


def summary(path):
    frames = read(path)
    keys = first_by_key(frames)
    units = sum(len(f['units']) for f in frames)
    shadows = sum(len(f['shadows']) for f in frames)
    ticks = sorted({f['tick'] for f in frames})
    print(f'{path}: {len(frames)} frames, {len(keys)} keys, ticks {ticks[0] if ticks else "-"}..'
          f'{ticks[-1] if ticks else "-"} ({len(ticks)}), {units} units, {shadows} objects, '
          f'{sum(len(f["selected_units"]) for f in frames)} selected units, '
          f'{sum(1 for f in frames if f["picked"] >= 0)} frames with a picked unit, '
          f'{sum(len(f["ghosts"]) for f in frames)} ghost records, {sum(len(f["lines"]) for f in frames)} circles')
    # the same key twice in one run: the fill of the same world and camera
    seen, same, differ = {}, 0, 0
    for f in frames:
        k = key(f)
        if k in seen:
            if differences(seen[k], f, False):
                differ += 1
            else:
                same += 1
        else:
            seen[k] = f
    print(f'  repeated keys within the run: {same} equal, {differ} different')
    return frames


def compare(a_path, b_path, sway, minimum):
    a = first_by_key(read(a_path))
    b = first_by_key(read(b_path))
    common = sorted(set(a) & set(b))
    bad = []
    for k in common:
        d = differences(a[k], b[k], sway)
        if d:
            bad.append((k, d))
    units = sum(len(a[k]['units']) for k in common)
    shadows = sum(len(a[k]['shadows']) for k in common)
    print(f'{len(a)} keys in A, {len(b)} in B, {len(common)} matched ({units} units, {shadows} objects); '
          f'{len(bad)} different')
    for k, d in bad[:20]:
        print(f'  tick {k[0]} camera {k[1]:016x}: {", ".join(d)}')
    if bad:
        print('RFIL DIFFER')
        return 1
    if len(common) < minimum:
        print(f'RFIL TOO FEW MATCHED ({len(common)} < {minimum})')
        return 1
    print('RFIL EQUAL')
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    s = sub.add_parser('summary')
    s.add_argument('capture')
    c = sub.add_parser('compare')
    c.add_argument('a')
    c.add_argument('b')
    c.add_argument('--sway', action='store_true', help='compare the objects\' sway fractions too (wall time)')
    c.add_argument('--min-matched', type=int, default=1)
    a = ap.parse_args()
    if a.cmd == 'summary':
        summary(a.capture)
        return 0
    return compare(a.a, a.b, a.sway, a.min_matched)


if __name__ == '__main__':
    sys.exit(main())
