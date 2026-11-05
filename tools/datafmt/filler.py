import sys, os, struct, collections
sys.argv = [sys.argv[0]]
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'analyze.py')).read().split('\ndef main')[0])
tot_cc = 0; tot = 0
for f, sname, kind in TABLES:
    d, recs, extra, end = records(f, sname, kind)
    c = collections.Counter(recs)
    dup = [(n, r.count(0xcc)) for r, n in c.most_common(3) if n > 1]
    cc = sum(r.count(0xcc) for r in recs)
    # bytes after NUL in char arrays that are non-zero and non-cc ("stack garbage")
    tot_cc += cc; tot += len(d)
    print(f"{f:22s} recs {len(recs):5d} distinct {len(c):5d} dup-groups {dup} 0xCC bytes {cc} ({100*cc/len(d):.1f}% of file)")
print('total', tot_cc, tot)
