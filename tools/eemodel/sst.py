"""The game's texture files: .sst (a 14-byte header + image files) and .tga, and their PNG form,
docs/MODEL_FORMATS.md section 3 and 5.

A texture as the game sees it (GETexture::Load with frame format 0, the only one any caller asks for): a surface size
(the .sst header's width and height; a .tga's own), and per frame one image: an uncompressed 24- or 32-bit Targa read
by GESurface::LoadTGA (rows and columns in the order its descriptor bits 4/5 give; a 24-bit image is opaque). Nothing
else is read: not the header's compressed flag or mip level count, not the mip levels after the first, not a Targa's
ID, colour map, origin or footer.

The PNG form: <name>.png is frame 0 (RGB for a 24-bit image, RGBA for 32-bit), <name>@<i>.png frame i. A surface size
that differs from frame 0's image size is kept in frame 0's tEXt chunk "ee:surface" ("<width>x<height>").
"""
import struct


class TextureError(Exception):
    pass


class Image:
    __slots__ = ('width', 'height', 'channels', 'pixels')

    def __init__(self, width, height, channels, pixels):
        self.width, self.height, self.channels, self.pixels = width, height, channels, pixels  # RGB(A), top down

    def __eq__(self, other):
        return (self.width, self.height, self.channels, self.pixels) == (
            other.width, other.height, other.channels, other.pixels)


class Texture:
    def __init__(self, width, height, frames, kind='sst'):
        self.width, self.height = width, height   # the surface size
        self.frames = frames                      # [Image]
        self.kind = kind                          # 'sst' or 'tga': the file it came from


def _read_tga(d, p):
    """GESurface::LoadTGA at offset p -> (Image, end offset)."""
    if p + 18 > len(d):
        raise TextureError('truncated Targa header')
    idl, _cmt, typ, _cmo, _cml, _cmd, _xo, _yo, w, h, bpp, desc = struct.unpack_from('<BBBHHBHHHHBB', d, p)
    if typ != 2 or bpp not in (24, 32):
        raise TextureError(f'Targa type {typ} depth {bpp}: the game reads only uncompressed 24/32-bit')
    n = bpp // 8
    start = p + 18 + idl
    size = n * w * h
    src = d[start:start + size]
    if len(src) != size:
        raise TextureError('truncated Targa pixels')
    stride = w * n
    rows = [src[y * stride:(y + 1) * stride] for y in range(h)]
    if not desc & 0x20:
        rows.reverse()                            # bottom-up file: the last row is the top
    if desc & 0x10:
        rows = [b''.join(r[x * n:(x + 1) * n] for x in range(w - 1, -1, -1)) for r in rows]
    px = bytearray(b''.join(rows))
    # BGR(A) -> RGB(A)
    px[0::n], px[2::n] = px[2::n], px[0::n]
    return Image(w, h, n, bytes(px)), start + size


def read(data, name):
    """A .sst or .tga file's bytes -> Texture (what the game reads)."""
    if name.lower().endswith('.tga'):
        img, _ = _read_tga(data, 0)
        return Texture(img.width, img.height, [img], 'tga')
    if len(data) < 14:
        raise TextureError('truncated header')
    _comp, _mips, frames, w, h = struct.unpack_from('<BBIII', data, 0)
    p = 14
    out = []
    for _ in range(frames):
        if p >= len(data):
            raise TextureError('truncated frame')
        typ = data[p]
        if typ != 0:
            raise TextureError(f'frame image type {typ} (only 0 = Targa occurs in the game data)')
        img, p = _read_tga(data, p + 1)
        out.append(img)
    return Texture(w, h, out, 'sst')


def _tga_bytes(img):
    """Image -> a canonical Targa: no ID, type 2, bottom-up rows (descriptor 0, or 8 alpha bits)."""
    n = img.channels
    px = bytearray(img.pixels)
    px[0::n], px[2::n] = px[2::n], px[0::n]
    stride = img.width * n
    rows = [bytes(px[y * stride:(y + 1) * stride]) for y in range(img.height)]
    rows.reverse()
    hdr = struct.pack('<BBBHHBHHHHBB', 0, 0, 2, 0, 0, 0, 0, 0, img.width, img.height, 8 * n, 8 if n == 4 else 0)
    return hdr + b''.join(rows)


def write(tex, kind=None):
    """Texture -> .sst bytes (header + type-0 Targa frames), or .tga bytes for a single-frame texture whose surface
    is its image (kind 'tga')."""
    kind = kind or tex.kind
    if kind == 'tga':
        if len(tex.frames) != 1 or (tex.width, tex.height) != (tex.frames[0].width, tex.frames[0].height):
            raise TextureError('a .tga holds one frame of the surface size')
        return _tga_bytes(tex.frames[0])
    out = bytearray(struct.pack('<BBIII', 0, 1, len(tex.frames), tex.width, tex.height))
    for img in tex.frames:
        out += b'\0' + _tga_bytes(img)
    return bytes(out)


def frame_name(stem, i):
    return f'{stem}.png' if i == 0 else f'{stem}@{i}.png'


def to_png(tex):
    """Texture -> [(file name suffix relative to the stem: '.png', '@1.png', ...), PNG bytes]."""
    import png
    out = []
    for i, img in enumerate(tex.frames):
        text = None
        if i == 0 and (tex.width, tex.height) != (img.width, img.height):
            text = {'ee:surface': f'{tex.width}x{tex.height}'}
        out.append((frame_name('', i), png.write(img.width, img.height, img.channels, img.pixels, text)))
    return out


def from_png(files):
    """[PNG bytes] (frame 0, 1, ...) -> Texture (kind 'sst')."""
    import png
    frames = []
    surface = None
    for i, data in enumerate(files):
        w, h, ch, px, text = png.read(data)
        frames.append(Image(w, h, ch, px))
        if i == 0:
            surface = (w, h)
            s = text.get('ee:surface')
            if s:
                a, _, b = s.partition('x')
                if not (a.isascii() and a.isdigit() and b.isascii() and b.isdigit()):
                    raise TextureError(f'bad ee:surface "{s}"')
                surface = (int(a), int(b))
    if not frames:
        raise TextureError('no frames')
    return Texture(surface[0], surface[1], frames, 'sst')
