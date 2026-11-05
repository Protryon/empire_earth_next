"""The .cem model format ("SSMF"): reader and writer, docs/MODEL_FORMATS.md section 2.

A file is one model tree: the root model followed by its sub-models, each a complete model with its own header
(depth first, in file order). Version 2.0 is read and written field by field (`read` / `write`: byte-identical for
every canonical file); versions 1.0-1.3 are read (`read` follows LoadLegacyVersion) and converted to the 2.0 model the
game builds from them in memory (legacy.py, ConvertLegacyModel).

Floats are kept as their 32-bit patterns (array('f') round-trips the bits); strings as their raw bytes, of which the
game uses the part before the first NUL (UString::Load: strlen).
"""
import struct
from array import array


class CemError(Exception):
    pass


class Material:
    __slots__ = ('name', 'texture_index', 'ranges', 'vertex_start', 'vertex_count', 'texture')

    def __init__(self, name=b'', texture_index=0, ranges=None, vertex_start=0, vertex_count=0, texture=b''):
        self.name = name                    # UString (raw bytes, NUL and tail included when read)
        self.texture_index = texture_index  # u32: the frame of a multi-frame texture (EEROUnits)
        self.ranges = ranges or []          # [(polygon start, polygon count)] per LOD
        self.vertex_start = vertex_start    # u32
        self.vertex_count = vertex_count    # u32
        self.texture = texture              # UString: texture file name without extension


class Frame:
    __slots__ = ('radius', 'vertices', 'tags', 'transform', 'bbox')

    def __init__(self, radius, vertices, tags, transform, bbox):
        self.radius = radius        # array('f', [r])
        self.vertices = vertices    # array('f'): vertexCount x (x y z, nx ny nz, u v)
        self.tags = tags            # array('f'): tagPointCount x (x y z)
        self.transform = transform  # array('f'): 16, mMatrix[4][4] row-major
        self.bbox = bbox            # array('f'): 6, bottom-left-front xyz, top-right-back xyz


class Model:
    def __init__(self):
        self.major, self.minor = 2, 0
        self.polygon_count = 0      # counts chunk mPolygonCount (the size of the LOD 0 polygon array)
        self.name = b''
        self.center = array('f', [0.0, 0.0, 0.0])
        self.lods = []              # [array('I') of 3 x count vertex indices] per LOD
        self.materials = []
        self.tag_names = []
        self.frames = []
        self.children = []
        self.legacy = None          # the 1.x data as read (legacy.py), None for 2.x

    @property
    def vertex_count(self):
        return len(self.frames[0].vertices) // 8 if self.frames else 0


def cstr(b):
    """The game's view of a UString's bytes: up to the first NUL."""
    i = b.find(b'\0')
    return b if i < 0 else b[:i]


class Reader:
    def __init__(self, data, pos=0):
        self.d, self.p = data, pos

    def take(self, n):
        if self.p + n > len(self.d):
            raise CemError(f'truncated at {self.p:#x} (need {n} bytes)')
        b = self.d[self.p:self.p + n]
        self.p += n
        return b

    def u32(self):
        return struct.unpack('<I', self.take(4))[0]

    def u16(self):
        return struct.unpack('<H', self.take(2))[0]

    def u8(self):
        return self.take(1)[0]

    def floats(self, n):
        a = array('f')
        a.frombytes(self.take(4 * n))
        return a

    def u32s(self, n):
        a = array('I')
        a.frombytes(self.take(4 * n))
        return a

    def string(self):
        n = self.u32()
        return self.take(n)


def read(data):
    """bytes -> the root Model (sub-models in .children). Raises CemError where the game's loader fails."""
    r = Reader(data)
    m = _read_model(r)
    m.trailing = data[r.p:]
    return m


def _read_header(r):
    tag = r.take(4)
    if tag != b'SSMF':
        raise CemError(f'not an SSMF model (tag {tag!r})')
    major = r.u16()
    if major not in (1, 2):
        raise CemError(f'unsupported major version {major}')
    minor = r.u16()
    if minor not in (0, 1, 2, 3):
        raise CemError(f'unsupported minor version {minor}')
    return major, minor


def _read_model(r):
    major, minor = _read_header(r)
    if major < 2:
        import legacy
        m = legacy.read_body(r, major, minor)
    else:
        m = Model()
        m.major, m.minor = major, minor
        _read_current(r, m)
    for _ in range(m._child_count):
        m.children.append(_read_model(r))
    del m._child_count
    return m


def _read_current(r, m):
    # LoadCountsChunk
    m.polygon_count = r.u32()
    vc = r.u32()
    tc = r.u32()
    mc = r.u32()
    fc = r.u32()
    if m.polygon_count == 0 or vc == 0 or mc == 0 or fc == 0:
        raise CemError('zero polygon, vertex, material or frame count')
    m._child_count = r.u32()
    lc = r.u32()
    # LoadMiscellaneousChunk
    m.name = r.string()
    m.center = r.floats(3)
    # LoadPolygonsChunk
    for _ in range(lc):
        n = r.u32()
        m.lods.append(r.u32s(3 * n))
    # LoadMaterialsChunk
    for _ in range(mc):
        mat = Material()
        mat.name = r.string()
        mat.texture_index = r.u32()
        mat.ranges = [(r.u32(), r.u32()) for _ in range(lc)]
        mat.vertex_start = r.u32()
        mat.vertex_count = r.u32()
        mat.texture = r.string()
        m.materials.append(mat)
    # LoadTagPointsChunk
    m.tag_names = [r.string() for _ in range(tc)]
    # LoadFrameChunk
    for _ in range(fc):
        radius = r.floats(1)
        verts = r.floats(8 * vc)
        tags = r.floats(3 * tc)
        m.frames.append(Frame(radius, verts, tags, r.floats(16), r.floats(6)))


def ustring(b):
    """UString::Save: u32 length (with the NUL), the characters, NUL."""
    b = cstr(b)
    return struct.pack('<I', len(b) + 1) + b + b'\0'


def raw_string(b):
    return struct.pack('<I', len(b)) + b


def write(m, exact=True):
    """Model tree -> version 2.0 bytes. exact: strings keep their stored bytes (tails after the NUL); otherwise they
    are written as UString::Save does."""
    out = bytearray()
    _write_model(out, m, raw_string if exact else ustring)
    if exact and getattr(m, 'trailing', b''):
        out += m.trailing
    return bytes(out)


def _write_model(out, m, s):
    out += b'SSMF' + struct.pack('<HH', 2, 0)
    tc = len(m.tag_names)
    out += struct.pack('<7I', m.polygon_count, m.vertex_count, tc, len(m.materials), len(m.frames),
                       len(m.children), len(m.lods))
    out += s(m.name) + m.center.tobytes()
    for lod in m.lods:
        out += struct.pack('<I', len(lod) // 3) + lod.tobytes()
    for mat in m.materials:
        out += s(mat.name) + struct.pack('<I', mat.texture_index)
        for a, b in mat.ranges:
            out += struct.pack('<II', a, b)
        out += struct.pack('<II', mat.vertex_start, mat.vertex_count) + s(mat.texture)
    for n in m.tag_names:
        out += s(n)
    for f in m.frames:
        out += f.radius.tobytes() + f.vertices.tobytes() + f.tags.tobytes() + f.transform.tobytes() + f.bbox.tobytes()
    for c in m.children:
        _write_model(out, c, s)


def walk(m, depth=0):
    """(model, depth) for the tree in file order."""
    yield m, depth
    for c in m.children:
        yield from walk(c, depth + 1)
