import sys, os, collections
sys.path.insert(0, os.path.dirname(__file__))
from ssa import read_ssa
for path in sys.argv[1:]:
    d, hdr, files, tags, q = read_ssa(path)
    c = collections.Counter()
    for f in files:
        dirn = f[0].rsplit(chr(92),1)[0] if chr(92) in f[0] else ''
        ext = f[0].rsplit('.',1)[-1]
        c[(dirn, ext)] += 1
    print(path, len(files), tags)
    for k, v in sorted(c.items()): print('  ', k, v)
