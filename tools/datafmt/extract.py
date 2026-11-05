import sys, os, struct
sys.path.insert(0, os.path.dirname(__file__))
from ssa import read_ssa
from blast import explode
src, dst, pat = sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv)>3 else ''
d, hdr, files, tags, q = read_ssa(src)
for name, s, e, l in files:
    if pat not in name: continue
    blk = d[s:s+l]
    if l > 12 and blk[:4] == b'PK01':
        usz = struct.unpack_from('<I', blk, 4)[0]
        out = explode(blk[12:])
        if len(out) != usz: print('SIZE MISMATCH', name, len(out), usz)
    else:
        out = blk
    p = os.path.join(dst, name.replace(chr(92), os.sep))
    os.makedirs(os.path.dirname(p), exist_ok=True)
    open(p, 'wb').write(out)
print('done')
