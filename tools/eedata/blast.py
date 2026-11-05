"""PKWARE Data Compression Library "explode" (a Python port of zlib's contrib/blast.c, Mark Adler, zlib licence).

explode(data) -> bytes: one DCL implode stream (the payload after a "PK01" block's 12-byte header).
"""


def _construct(rep):
    length = []
    for byte in rep:
        length += [byte & 15] * ((byte >> 4) + 1)
    count = [0] * 14
    for n in length:
        count[n] += 1
    offs = [0] * 14
    for i in range(1, 13):
        offs[i + 1] = offs[i] + count[i]
    symbol = [0] * len(length)
    for sym, n in enumerate(length):
        if n:
            symbol[offs[n]] = sym
            offs[n] += 1
    return count, symbol


_LITLEN = [11, 124, 8, 7, 28, 7, 188, 13, 76, 4, 10, 8, 12, 10, 12, 10, 8, 23, 8, 9, 7, 6, 7, 8, 7, 6, 55, 8, 23, 24,
           12, 11, 7, 9, 11, 12, 6, 7, 22, 5, 7, 24, 6, 11, 9, 6, 7, 22, 7, 11, 38, 7, 9, 8, 25, 11, 8, 11, 9, 12, 8,
           12, 5, 38, 5, 38, 5, 11, 7, 5, 6, 21, 6, 10, 53, 8, 7, 24, 10, 27, 44, 253, 253, 253, 252, 252, 252, 13, 12,
           45, 12, 45, 12, 61, 12, 45, 44, 173]
_LENLEN = [2, 35, 36, 53, 38, 23]
_DISTLEN = [2, 20, 53, 230, 247, 151, 248]
_BASE = [3, 2, 4, 5, 6, 7, 8, 9, 10, 12, 16, 24, 40, 72, 136, 264]
_EXTRA = [0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8]
_LIT, _LEN, _DIST = _construct(_LITLEN), _construct(_LENLEN), _construct(_DISTLEN)


def explode(data):
    pos = bitbuf = bitcnt = 0
    out = bytearray()

    def bits(need):
        nonlocal pos, bitbuf, bitcnt
        val = bitbuf
        while bitcnt < need:
            if pos >= len(data):
                raise ValueError('explode: input ends early')
            val |= data[pos] << bitcnt
            pos += 1
            bitcnt += 8
        bitbuf = val >> need
        bitcnt -= need
        return val & ((1 << need) - 1)

    def decode(h):
        nonlocal pos, bitbuf, bitcnt
        count, symbol = h
        code = first = index = 0
        bb, left, n = bitbuf, bitcnt, 1
        while True:
            while left:
                code |= (bb & 1) ^ 1
                bb >>= 1
                c = count[n]
                if code < first + c:
                    bitbuf, bitcnt = bb, (bitcnt - n) & 7
                    return symbol[index + (code - first)]
                index += c
                first = (first + c) << 1
                code <<= 1
                n += 1
                left -= 1
            left = 13 - n
            if left <= 0:
                raise ValueError('explode: ran out of codes')
            if pos >= len(data):
                raise ValueError('explode: input ends early')
            bb = data[pos]
            pos += 1
            left = min(left, 8)

    lit = bits(8)
    if lit > 1:
        raise ValueError('explode: bad literal flag')
    dictb = bits(8)
    if not 4 <= dictb <= 6:
        raise ValueError('explode: bad dictionary size')
    while True:
        if bits(1):
            sym = decode(_LEN)
            n = _BASE[sym] + bits(_EXTRA[sym])
            if n == 519:
                break
            sym = 2 if n == 2 else dictb
            dist = (decode(_DIST) << sym) + bits(sym) + 1
            if dist > len(out):
                raise ValueError('explode: distance too far back')
            for _ in range(n):
                out.append(out[-dist])
        else:
            out.append(decode(_LIT) if lit else bits(8))
    return bytes(out)
