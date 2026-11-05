# Port of zlib contrib/blast.c (PKWARE DCL explode)
class _S:
    pass
def _construct(lens):
    # lens: list of (count, len) compact rep -> expand
    length = []
    for byte in lens:
        n = (byte >> 4) + 1; l = byte & 15
        length += [l]*n
    count = [0]*14
    for l in length: count[l] += 1
    offs = [0]*14
    for i in range(1,13): offs[i+1] = offs[i] + count[i]
    symbol = [0]*len(length)
    for sym,l in enumerate(length):
        if l: symbol[offs[l]] = sym; offs[l] += 1
    return count, symbol
LITLEN = [11,124,8,7,28,7,188,13,76,4,10,8,12,10,12,10,8,23,8,9,7,6,7,8,7,6,55,8,23,24,12,11,7,9,11,12,6,7,22,5,7,24,6,11,9,6,7,22,7,11,38,7,9,8,25,11,8,11,9,12,8,12,5,38,5,38,5,11,7,5,6,21,6,10,53,8,7,24,10,27,44,253,253,253,252,252,252,13,12,45,12,45,12,61,12,45,44,173]
LENLEN = [2,35,36,53,38,23]
DISTLEN = [2,20,53,230,247,151,248]
BASE = [3,2,4,5,6,7,8,9,10,12,16,24,40,72,136,264]
EXTRA = [0,0,0,0,0,0,0,0,1,2,3,4,5,6,7,8]
_lit = _construct(LITLEN); _len = _construct(LENLEN); _dist = _construct(DISTLEN)
def explode(data):
    pos = 0; bitbuf = 0; bitcnt = 0
    out = bytearray()
    def bits(need):
        nonlocal pos, bitbuf, bitcnt
        val = bitbuf
        while bitcnt < need:
            val |= data[pos] << bitcnt; pos += 1; bitcnt += 8
        bitbuf = val >> need; bitcnt -= need
        return val & ((1 << need) - 1)
    def decode(h):
        nonlocal pos, bitbuf, bitcnt
        count, symbol = h
        code = first = index = 0
        bb = bitbuf; left = bitcnt; ln = 1
        while True:
            while left:
                code |= (bb & 1) ^ 1; bb >>= 1
                c = count[ln]
                if code < first + c:
                    bitbuf = bb; bitcnt = (bitcnt - ln) & 7
                    return symbol[index + (code - first)]
                index += c; first += c; first <<= 1; code <<= 1; ln += 1; left -= 1
            left = (13) - ln
            if left <= 0: raise ValueError('ran out of codes')
            bb = data[pos]; pos += 1
            if left > 8: left = 8
    lit = bits(8)
    if lit > 1: raise ValueError('bad lit flag')
    dictb = bits(8)
    if dictb < 4 or dictb > 6: raise ValueError('bad dict')
    while True:
        if bits(1):
            symbol = decode(_len)
            ln = BASE[symbol] + bits(EXTRA[symbol])
            if ln == 519: break
            symbol = 2 if ln == 2 else dictb
            dist = decode(_dist) << symbol
            dist += bits(symbol); dist += 1
            if dist > len(out): raise ValueError('dist too far')
            for _ in range(ln): out.append(out[-dist])
        else:
            symbol = decode(_lit) if lit else bits(8)
            out.append(symbol)
    return bytes(out)
