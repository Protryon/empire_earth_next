import struct, sys
def read_ssa(path):
    d = open(path,'rb').read()
    ident, ver, res = struct.unpack_from('<III', d, 0)
    p = 12
    fatsize, = struct.unpack_from('<I', d, p); p += 4
    end = p + fatsize - 4  # fatsize includes its own 4 bytes? check
    files = []
    pos = 0
    q = p
    while pos < fatsize:
        n, = struct.unpack_from('<I', d, q); q += 4
        name = d[q:q+n].split(b'\0')[0].decode('latin-1'); q += n
        s, e, l = struct.unpack_from('<iii', d, q); q += 12
        files.append((name, s, e, l))
        pos += 4 + n + 12
    ntags, = struct.unpack_from('<I', d, q); q += 4
    tags = []
    for i in range(ntags):
        n, = struct.unpack_from('<I', d, q); q += 4; a = d[q:q+n]; q += n
        n, = struct.unpack_from('<I', d, q); q += 4; b = d[q:q+n]; q += n
        tags.append((a, b))
    return d, (ident, ver, res, fatsize), files, tags, q
if __name__ == '__main__':
    d, hdr, files, tags, q = read_ssa(sys.argv[1])
    print(hex(hdr[0]), hdr[1:], len(files), 'tags', tags, 'datastart', q)
    for f in files:
        if len(sys.argv) < 3 or sys.argv[2] in f[0]:
            blk = d[f[1]:f[1]+f[3]]
            comp = blk[:4] == b'PK01'
            usz = struct.unpack_from('<I', blk, 4)[0] if comp else f[3]
            print(f"{f[0]:60s} {f[1]:9d} {f[3]:8d} {'PK' if comp else '--'} {usz}")
