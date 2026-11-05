"""Parse every db table of data.ssa with the source layouts and print per-field statistics."""
import struct, json, os, sys, collections, math

HERE = os.path.dirname(os.path.abspath(__file__))
DB = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'aoc', 'db')
S = json.load(open(os.path.join(HERE, 'structs.json')))

TABLES = [  # file, struct, container
    ('dbaiunittargeting', 'EEAIUnitTargetingRecord', 'count'),
    ('dbambientsounds', 'EEAmbientSoundRecord', 'count'),
    ('dbanimals', 'EEAnimalRecord', 'count'),
    ('dbareaeffect', 'EEAreaEffectData', 'count'),
    ('dbbuttons', 'EEButtonRecord', 'count'),
    ('dbcalamity', 'EECalamityData', 'count'),
    ('dbcivilization', 'EECivilizationCategoryEntry', 'count'),
    ('dbcivpowers', 'EECivPower', 'count'),
    ('dbcliffterrain', 'EECliffTerrainRecord', 'count'),
    ('dbcolortable', 'EEColorTableRecord', 'count'),
    ('dbcpbehavior', 'EECPBehavior', 'count'),
    ('dbeffects', 'EEEventEffect', 'raw2000'),
    ('dbfamily', 'EEDamageTableRecord', 'count'),
    ('dbgamevariant', 'EEGameVariantData', 'count'),
    ('dbgfxeffects', 'EEGraphicEffectRecord', 'count'),
    ('dbgraphics', 'EEGraphicObjectRecord', 'str33'),
    ('dbmusic', 'EEMusicRecord', 'count'),
    ('dbobjects', 'EEUnitClassRecord', 'count'),
    ('dbpremadecivs', 'EECivilizationEntry', 'count'),
    ('dbsounds', 'EESoundRecord', 'str1'),
    ('dbstartingresources', 'EEStartingResourcesRecord', 'count'),
    ('dbtechtree', 'EETechTreeRecord', 'techtree'),
    ('dbterrain', 'EETerrainRecord', 'str1'),
    ('dbterraintype', 'EETerrainTypeRecord', 'count'),
    ('dbuiback', 'UIDbDataBackgrounds', 'count'),
    ('dbuicontrolevents', 'UIDbDataEvents', 'count'),
    ('dbuicontrols', 'UIDbControl', 'count'),
    ('dbuifonts', 'UIDbDataFonts', 'count'),
    ('dbuiformevents', 'UIDbDataEvents', 'count'),
    ('dbuiforms', 'UIDbDataForms', 'count'),
    ('dbuihotkey', 'UIDbDataHotKeys', 'count'),
    ('dbunitbehavior', 'EEUnitBehaviorRecord', 'count'),
    ('dbunitset', 'EEUnitSetRecord', 'count'),
    ('dbupgrade', 'EEUCUpgradeRecord', 'count'),
    ('dbweapontohit', 'EEToHitRecord', 'count'),
    ('dbworld', 'EEWorldTableEntry', 'count'),
]


def ustr(d, p):
    n, = struct.unpack_from('<I', d, p)
    return d[p + 4:p + 4 + n], p + 4 + n


def records(f, sname, kind):
    d = open(os.path.join(DB, f + '.dat'), 'rb').read()
    sz = S[sname]['size']
    recs, extra = [], []
    p = 0
    if kind == 'count':
        n, = struct.unpack_from('<I', d, 0); p = 4
        for i in range(n):
            recs.append(d[p:p + sz]); p += sz
    elif kind == 'raw2000':
        for i in range(2000):
            recs.append(d[p:p + sz]); p += sz
    elif kind in ('str1', 'str33'):
        k = 1 if kind == 'str1' else 33
        n, = struct.unpack_from('<I', d, 0); p = 4
        for i in range(n):
            ss = []
            for j in range(k):
                s, p = ustr(d, p); ss.append(s)
            extra.append(ss)
            recs.append(d[p:p + sz]); p += sz
    elif kind == 'techtree':
        for e in range(15):
            nn, = struct.unpack_from('<I', d, p); p += 4
            for k in range(nn + 1):
                recs.append(d[p:p + sz]); p += sz
                c, = struct.unpack_from('<I', d, p); p += 4
                extra.append(list(struct.unpack_from('<%dI' % c, d, p))); p += 4 * c
    return d, recs, extra, p


def is_text(b):
    z = b.split(b'\0')[0]
    return all(32 <= c < 127 or c in (9,) or c >= 0xa0 for c in z)


def fstats(recs, off, typ, count):
    vals = []
    for r in recs:
        if typ in ('s32', 'u32', 'f32'):
            v = struct.unpack_from('<' + {'s32': 'i', 'u32': 'I', 'f32': 'f'}[typ], r, off)[0]
        elif typ in ('bool', 'u8'):
            v = r[off]
        elif typ == 's8':
            v = r[off:off + count]
        else:
            v = r[off:off + count]
        vals.append(v)
    return vals


def summarize(vals, typ):
    if typ == 's8':
        texts = sum(1 for v in vals if v.split(b'\0')[0])
        junk = sum(1 for v in vals if v.split(b'\0', 1)[-1].strip(b'\0') if b'\0' in v)
        ccfill = sum(1 for v in vals if b'\xcc\xcc\xcc\xcc' in v)
        ex = [v.split(b'\0')[0].decode('latin-1') for v in vals if v.split(b'\0')[0]][:3]
        return f"strings {texts}/{len(vals)}; bytes after NUL non-zero in {junk} (0xCC fill in {ccfill}); e.g. {ex}"
    c = collections.Counter(vals)
    if typ == 'f32':
        fin = [v for v in vals if not math.isnan(v)]
        return f"distinct {len(c)}; min {min(fin):g} max {max(fin):g}; top {[(round(k,4),n) for k,n in c.most_common(3)]}"
    return f"distinct {len(c)}; min {min(vals)} max {max(vals)}; top {c.most_common(3)}"


def gap_dwords(recs, off, size):
    """describe a gap region dword by dword"""
    out = []
    o = off
    # string-like region at start?
    while o + 4 <= off + size:
        iv = [struct.unpack_from('<i', r, o)[0] for r in recs]
        fv = [struct.unpack_from('<f', r, o)[0] for r in recs]
        c = collections.Counter(iv)
        idx = sum(1 for i, v in enumerate(iv) if v == i)
        fl = sum(1 for v in fv if v != 0 and 1e-4 < abs(v) < 1e7)
        out.append((o, len(c), c.most_common(2), idx, fl))
        o += 4
    return out


def main():
    rep = {}
    for f, sname, kind in TABLES:
        d, recs, extra, end = records(f, sname, kind)
        st = S[sname]
        print(f"\n### {f}.dat  struct {sname} ({st['size']} B)  records {len(recs)}  parsed-to {end} of {len(d)}")
        if not recs:
            continue
        # header check: name[100] + id + index
        names = sum(1 for r in recs if r[:100].split(b'\0')[0] and is_text(r[:100]))
        print(f"  name@0 printable in {names}/{len(recs)}")
        for fd in st['fields']:
            if fd['type'] == 'pad':
                continue
            if fd['name'].startswith('_') or (fd['type'] in ('u8', 's8') and fd['count'] > 8 and 'Name' not in fd['name'] and 'File' not in fd['name'] and 'Sound' not in fd['name']):
                print(f"  +0x{fd['off']:03x} {fd['name']}[{fd['count']}] GAP:")
                if fd['count'] >= 100 and fd['off'] == 0:
                    v = fstats(recs, 0, 's8', 100)
                    print('      as char[100]:', summarize(v, 's8'))
                    rest = fd['count'] - 100
                    for o, nd, top, idx, fl in gap_dwords(recs, 100, rest):
                        print(f"      +0x{o:03x}: distinct {nd} top {top} ==index {idx} floaty {fl}")
                else:
                    for o, nd, top, idx, fl in gap_dwords(recs, fd['off'], fd['count']):
                        print(f"      +0x{o:03x}: distinct {nd} top {top} ==index {idx} floaty {fl}")
                continue
            if fd['count'] > 1 and fd['type'] != 's8':
                v = fstats(recs, fd['off'], fd['type'], 1)
                print(f"  +0x{fd['off']:03x} {fd['type']} {fd['name']}[{fd['count']}] (first elt) {summarize(v, fd['type'])}")
                continue
            v = fstats(recs, fd['off'], fd['type'], fd['count'])
            print(f"  +0x{fd['off']:03x} {fd['type']} {fd['name']} {summarize(v, fd['type'])}")
        if extra and kind != 'techtree':
            for j in range(len(extra[0])):
                vals = [e[j] for e in extra]
                ne = sum(1 for v in vals if v.rstrip(b'\0'))
                print(f"  str[{j}] nonempty {ne}/{len(vals)} e.g. {[v.rstrip(bytes(1)).decode('latin-1') for v in vals if v.rstrip(bytes(1))][:3]}")


main()
