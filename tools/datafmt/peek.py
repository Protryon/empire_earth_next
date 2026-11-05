import sys, struct, collections
d = open(sys.argv[1], 'rb').read()
S = 1960
n = struct.unpack_from('<I', d, 0)[0]
c = collections.Counter()
for i in range(n):
    r = d[4+i*S:4+(i+1)*S]
    seg = r[0x3b4:0x47c]
    if seg.strip(b'\0') and r[0x74:0x78] != b'\xff\xff\xff\xff':
        c[seg.rstrip(b'\0')[:60]] += 1
for k, v in c.most_common(12): print(v, k)
