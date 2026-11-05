"""PNG reader and writer (standard library only), docs/MODEL_FORMATS.md section 5.

`write` makes 8-bit RGB or RGBA images (filter Up on every row but the first, zlib level 9) with optional tEXt
chunks. `read` takes any PNG an editor writes (bit depths 1-16, grey, grey+alpha, palette, RGB, RGBA, tRNS, Adam7
interlacing) and returns 8-bit RGB or RGBA the way the game's libpng reading does it (palette and grey expanded,
tRNS to alpha, 16-bit samples reduced to their high byte).
"""
import struct
import zlib

SIGNATURE = b'\x89PNG\r\n\x1a\n'


class PngError(Exception):
    pass


def _chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)


def _swar_sub(x, y, n):
    """Bytewise (x - y) mod 256 of two n-byte big integers."""
    h = int.from_bytes(b'\x80' * n, 'big')
    m = (1 << (8 * n)) - 1
    return (((x | h) - (y & ~h & m)) ^ ((x ^ ~y & m) & h)) & m


def _swar_add(x, y, n):
    h = int.from_bytes(b'\x80' * n, 'big')
    m = (1 << (8 * n)) - 1
    lo = m & ~h
    return (((x & lo) + (y & lo)) ^ ((x ^ y) & h)) & m


def write(width, height, channels, pixels, text=None):
    """8-bit RGB (channels 3) or RGBA (4) rows, top to bottom -> PNG bytes."""
    if channels not in (3, 4):
        raise PngError('channels must be 3 or 4')
    stride = width * channels
    if len(pixels) != stride * height:
        raise PngError('pixel data does not match the size')
    raw = bytearray()
    prev = None
    for y in range(height):
        row = pixels[y * stride:(y + 1) * stride]
        if prev is None:
            raw += b'\0' + row
        else:
            d = _swar_sub(int.from_bytes(row, 'big'), int.from_bytes(prev, 'big'), stride)
            raw += b'\2' + d.to_bytes(stride, 'big')
        prev = row
    out = SIGNATURE + _chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6 if channels == 4 else 2, 0, 0, 0))
    for k, v in (text or {}).items():
        out += _chunk(b'tEXt', k.encode('latin-1') + b'\0' + v.encode('latin-1'))
    out += _chunk(b'IDAT', zlib.compress(bytes(raw), 9)) + _chunk(b'IEND', b'')
    return out


def _paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    return b if pb <= pc else c


def _unfilter(data, pos, width, height, bpp_bits, out):
    """Unfilter one (sub-)image starting at data[pos]; appends its rows to out; returns the new position."""
    stride = (width * bpp_bits + 7) // 8
    bpp = max(1, bpp_bits // 8)
    prev = bytes(stride)
    for _ in range(height):
        if pos + 1 + stride > len(data):
            raise PngError('image data too short')
        ft = data[pos]
        row = bytearray(data[pos + 1:pos + 1 + stride])
        pos += 1 + stride
        if ft == 0:
            pass
        elif ft == 1:
            for i in range(bpp, stride):
                row[i] = (row[i] + row[i - bpp]) & 255
        elif ft == 2:
            row = bytearray(_swar_add(int.from_bytes(row, 'big'), int.from_bytes(prev, 'big'), stride)
                            .to_bytes(stride, 'big')) if stride else row
        elif ft == 3:
            for i in range(stride):
                left = row[i - bpp] if i >= bpp else 0
                row[i] = (row[i] + ((left + prev[i]) >> 1)) & 255
        elif ft == 4:
            for i in range(stride):
                left = row[i - bpp] if i >= bpp else 0
                ul = prev[i - bpp] if i >= bpp else 0
                row[i] = (row[i] + _paeth(left, prev[i], ul)) & 255
        else:
            raise PngError(f'filter type {ft}')
        out.append(bytes(row))
        prev = row
    return pos


def _samples(row, width, depth, nch):
    """A row's samples as ints (depth 1-16)."""
    if depth == 8:
        return row[:width * nch]
    if depth == 16:
        return [row[2 * i] for i in range(width * nch)]   # high byte (libpng png_set_strip_16)
    per = 8 // depth
    mask = (1 << depth) - 1
    out = []
    for i in range(width * nch):
        byte = row[i // per]
        shift = 8 - depth * (i % per + 1)
        out.append((byte >> shift) & mask)
    return out


def read(data):
    """PNG bytes -> (width, height, channels 3|4, pixels bytes top to bottom, {tEXt keyword: text})."""
    if data[:8] != SIGNATURE:
        raise PngError('not a PNG file')
    pos = 8
    ihdr = None
    idat = bytearray()
    plte = trns = None
    text = {}
    while pos + 8 <= len(data):
        n, kind = struct.unpack_from('>I4s', data, pos)
        body = data[pos + 8:pos + 8 + n]
        if len(body) != n:
            raise PngError('truncated chunk')
        pos += 12 + n
        if kind == b'IHDR':
            ihdr = struct.unpack('>IIBBBBB', body)
        elif kind == b'PLTE':
            plte = body
        elif kind == b'tRNS':
            trns = body
        elif kind == b'IDAT':
            idat += body
        elif kind == b'tEXt':
            k, _, v = body.partition(b'\0')
            text[k.decode('latin-1')] = v.decode('latin-1')
        elif kind == b'IEND':
            break
    if ihdr is None:
        raise PngError('no IHDR')
    w, h, depth, ctype, comp, filt, inter = ihdr
    if comp or filt or inter > 1 or ctype not in (0, 2, 3, 4, 6) or w == 0 or h == 0:
        raise PngError('unsupported PNG header')
    nch = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[ctype]
    raw = zlib.decompress(bytes(idat))
    bits = depth * nch
    if depth == 8 and ctype in (2, 6) and inter == 0 and trns is None:
        rows = []
        _unfilter(raw, 0, w, h, bits, rows)
        return w, h, nch, b''.join(rows), text
    # rows of samples, full image
    grid = [[None] * w for _ in range(h)]
    if inter == 0:
        rows = []
        _unfilter(raw, 0, w, h, bits, rows)
        passes = [(0, 0, 1, 1, w, h, rows)]
    else:
        passes = []
        p = 0
        for x0, y0, dx, dy in ((0, 0, 8, 8), (4, 0, 8, 8), (0, 4, 4, 8), (2, 0, 4, 4), (0, 2, 2, 4), (1, 0, 2, 2),
                               (0, 1, 1, 2)):
            pw = (w - x0 + dx - 1) // dx
            ph = (h - y0 + dy - 1) // dy
            rows = []
            if pw and ph:
                p = _unfilter(raw, p, pw, ph, bits, rows)
            passes.append((x0, y0, dx, dy, pw, ph, rows))
    has_alpha = ctype in (4, 6) or trns is not None
    out_ch = 4 if has_alpha else 3
    scale = (255 // ((1 << depth) - 1)) if depth < 8 and ctype != 3 else 1
    if ctype == 3 and plte is None:
        raise PngError('palette image without PLTE')
    pix = bytearray(w * h * out_ch)
    tkey = None
    if trns is not None and ctype == 0:
        tkey = struct.unpack('>H', trns[:2])[0]
    elif trns is not None and ctype == 2:
        tkey = struct.unpack('>HHH', trns[:6])
    for x0, y0, dx, dy, pw, ph, rows in passes:
        for j, row in enumerate(rows):
            y = y0 + j * dy
            s = _samples(row, pw, depth, nch)
            raw16 = struct.unpack(f'>{pw * nch}H', row[:2 * pw * nch]) if depth == 16 else None
            for i in range(pw):
                x = x0 + i * dx
                o = (y * w + x) * out_ch
                if ctype == 6:
                    pix[o:o + 4] = bytes(s[4 * i:4 * i + 4])
                elif ctype == 2:
                    pix[o:o + 3] = bytes(s[3 * i:3 * i + 3])
                    if out_ch == 4:
                        key = raw16[3 * i:3 * i + 3] if raw16 else tuple(s[3 * i:3 * i + 3])
                        pix[o + 3] = 0 if tuple(key) == tkey else 255
                elif ctype == 0:
                    g = s[i] * scale
                    pix[o:o + 3] = bytes((g, g, g))
                    if out_ch == 4:
                        key = raw16[i] if raw16 else s[i]
                        pix[o + 3] = 0 if key == tkey else 255
                elif ctype == 4:
                    g = s[2 * i]
                    pix[o:o + 4] = bytes((g, g, g, s[2 * i + 1]))
                else:
                    k = s[i]
                    if 3 * k + 3 > len(plte):
                        raise PngError('palette index out of range')
                    pix[o:o + 3] = plte[3 * k:3 * k + 3]
                    if out_ch == 4:
                        pix[o + 3] = trns[k] if k < len(trns) else 255
    return w, h, out_ch, bytes(pix), text
