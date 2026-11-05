import struct, sys
d = open(sys.argv[1], 'rb').read(); p = 0
for e in range(15):
    nn, = struct.unpack_from('<I', d, p); p += 4
    valid = 0; ids = []
    for k in range(nn + 1):
        r = d[p:p+268]; p += 268
        c, = struct.unpack_from('<I', d, p); p += 4 + 4*c
        rid = struct.unpack_from('<i', r, 0x68)[0]
        if k == 0: eid = (rid, r[:100].split(b'\0')[0], c)
        elif rid != 0: valid += 1; ids.append(rid)
    print(e, 'nodes', nn, 'valid', valid, 'epoch rec', eid, 'id range', (min(ids), max(ids)) if ids else None)
