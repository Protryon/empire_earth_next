import struct, sys
d = open(sys.argv[1], 'rb').read()
a, n = struct.unpack_from('<II', d, 0); p = 8
print('hdr', a, n, 'size', len(d), 'expect', 8 + n*132)
for i in range(n):
    r = d[p:p+132]; p += 132
    x = struct.unpack_from('<ii', r, 0); nm = r[8:108].split(b'\0')[0]; t = struct.unpack_from('<6i', r, 108)
    print(i, x, nm, t)
for path in sys.argv[1:]:
    import glob
for s in [d]:
    pass
