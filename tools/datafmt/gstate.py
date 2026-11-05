import sys, os, collections, re
sys.argv=[sys.argv[0]]
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'analyze.py')).read().split('\ndef main')[0])
d, recs, extra, end = records('dbgraphics', 'EEGraphicObjectRecord', 'str33')
for k in range(14):
    tex = [e[3+2*k] for e in extra if e[3+2*k]]; mod = [e[4+2*k] for e in extra if e[4+2*k]]
    words = collections.Counter()
    for m in mod + tex:
        s = m.decode().lower().split(chr(92))[-1]
        for w in ['hunt','build','carry','chop','mine','farm','forage','die','death','idle','fidget','attack','walk','run','fish','angle','diag','repair','convert','heal','work','fire','gate','open']:
            if w in s: words[w] += 1
    print(k, len(tex), len(mod), words.most_common(4), [m.decode() for m in mod[:2]])
