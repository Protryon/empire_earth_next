import struct, sys
for path in sys.argv[1:]:
    d = open(path, 'rb').read(); n, = struct.unpack_from('<I', d, 0); p = 4
    rows = []
    for i in range(n):
        L, = struct.unpack_from('<I', d, p); s = d[p+4:p+4+L]; p += 4 + L
        r = d[p:p+120]; p += 120
        rows.append((s, r[:100].split(b'\0')[0], struct.unpack_from('<ii', r, 100), r[108:120].hex()))
    print(path, n, 'end', p, len(d))
    for x in rows[:4] + rows[-2:]: print('  ', x)
d = open(sys.argv[1].replace('dbterraingraytextures', 'dbsounds'), 'rb').read()
p = 4
for i in range(3):
    L, = struct.unpack_from('<I', d, p); print('sound str len', L, d[p+4:p+4+L]); p += 4 + L + 132
