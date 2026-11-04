"""Round 39 differential harness (TEST ONLY): compare two per-PT hash streams (tools/boot/differential/sampler.py).

python -I tools/boot/differential/compare.py A.stream.gz B.stream.gz [--json OUT] [--max-units 20] [--pointers]

Reports: PTs sampled in each and in common (gaps), identical up to PT X, the first PT whose SIM world hash differs and
which columns/players differ there, separately the first RENDER-hash difference (render-written fields only: expected
noise unless the sim later diverges), the first RNG difference; then, from the snapshots nearest the first SIM
divergence (the last one at or before it and the first one after it, in both runs), the physics block, the players
and the units (matched by unit id) that differ, field by field with header names. Pointer members are skipped
(addresses differ between runs/images) unless --pointers; unit references are compared by the target's unit id.
Exit 0 identical over the common PTs, 1 SIM divergence, 2 render-only divergence, 3 nothing in common, 4 AI-only
divergence (ai column, version-5 C streams; AI objects compared by path in the snapshots: ai_layout.py).
"""
import argparse, gzip, json, pathlib, struct, sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import layout as L


def opentext(path):
    path = pathlib.Path(path)
    if not path.exists() and path.with_name(path.name + '.gz').exists():   # session_hook gzips the C stream at the end
        path = path.with_name(path.name + '.gz')
    return gzip.open(path, 'rt', encoding='ascii') if path.suffix == '.gz' else open(path, encoding='ascii')


def load_stream(path, ai=None, stats=None):
    """(header, {pt: columns}, trailer). Version 5 (C hasher) streams carry the `ai` token after `inputs`: it is
    moved out of the row into `ai` ({pt: [per-player hash or '-']}; '=' = not hashed that PT, left out), so the rows
    keep the version-4 column positions. '#stat' cost records go to `stats`."""
    header, rows, trailer = None, {}, None
    v5 = False
    try:
        with opentext(path) as f:
            for line in f:
                if line.startswith('#end '):
                    trailer = json.loads(line[5:])
                elif line.startswith('#stat '):
                    if stats is not None:
                        stats.append(json.loads(line[6:]))
                elif line.startswith('#'):
                    if header is None:
                        try:
                            header = json.loads(line[1:])
                            v5 = header.get('version', 0) >= 5 and ' ai ' in header.get('columns', '')
                        except ValueError:
                            pass
                elif line.strip():
                    parts = line.split()
                    if v5:
                        tok = parts.pop(1 + len(COLS))
                        if ai is not None and tok != '=':
                            ai[int(parts[0])] = tok.split('.')
                    rows[int(parts[0])] = parts[1:]
    except (EOFError, OSError, gzip.BadGzipFile) as e:   # a stream cut by a killed sampler: keep what was read
        print('note: %s truncated (%s)' % (path, e))
    return header, rows, trailer


def diff_ai(sa, sb, max_objects=40):
    """AI records of two snapshots (ai_layout.Walker paths): objects only in A / only in B, and the differing
    fields per object path (the first max_objects, in walk order)."""
    ra = {r['path']: r for r in sa.get('ai', [])}; rb = {r['path']: r for r in sb.get('ai', [])}
    order = [r['path'] for r in sa.get('ai', [])]
    out = dict(objects_a=len(ra), objects_b=len(rb), only_a=[k for k in order if k not in rb][:max_objects],
               only_b=[r['path'] for r in sb.get('ai', []) if r['path'] not in ra][:max_objects], objects=[])
    n = 0
    for k in order:
        if k not in rb:
            continue
        x, y = ra[k], rb[k]
        f = [(name, x['v'].get(name), y['v'].get(name)) for name in list(x['v']) + [m for m in y['v'] if m not in x['v']]
             if x['v'].get(name) != y['v'].get(name)]
        if x.get('cls') != y.get('cls'):
            f.insert(0, ('class (RTTI)', x.get('cls'), y.get('cls')))
        if f:
            n += 1
            if len(out['objects']) < max_objects:
                out['objects'].append(dict(path=k, type=x['type'], cls=x.get('cls'), fields=f))
    out['objects_differing'] = n
    return out


def ai_fmt(v):
    if isinstance(v, str) and len(v) == 8:   # a 4-byte scalar: also as an int and a float
        u = int.from_bytes(bytes.fromhex(v), 'little')
        return '%s (%d / %.7g)' % (v, struct.unpack('<i', struct.pack('<I', u))[0], struct.unpack('<f', struct.pack('<I', u))[0])
    if isinstance(v, list) and len(v) > 8:
        return '[%d: %s ...]' % (len(v), ', '.join(map(str, v[:8])))
    return str(v)


def print_ai(d, indent='  '):
    print('%sAI objects: A %d, B %d, differing %d' % (indent, d['objects_a'], d['objects_b'], d['objects_differing']))
    if d['only_a']: print('%sonly in A: %s' % (indent, d['only_a'][:10]))
    if d['only_b']: print('%sonly in B: %s' % (indent, d['only_b'][:10]))
    for o in d['objects'][:20]:
        print('%s %s (%s%s)' % (indent, o['path'], o['type'], ' ' + o['cls'] if o.get('cls') else ''))
        for name, x, y in o['fields'][:12]:
            print('%s   %-34s %s | %s' % (indent, name, ai_fmt(x), ai_fmt(y)))


def snap_path(stream):
    stream = pathlib.Path(stream)
    return stream.with_name(stream.name.removesuffix('.gz').removesuffix('.txt') + '.snap.jsonl.gz')


def snapshots(stream, wanted):
    """{pt: snapshot} for the PTs in `wanted` (reads the gzip once)."""
    out = {}
    p = snap_path(stream)
    if not p.exists():
        return out
    try:
        with gzip.open(p, 'rt', encoding='ascii') as f:
            for line in f:
                pt = int(line[7:line.index(',')]) if line.startswith('{"pt": ') else json.loads(line)['pt']
                if pt in wanted:
                    out[pt] = json.loads(line)
    except (EOFError, OSError, gzip.BadGzipFile):
        pass
    return out


def snapshot_pts(stream):
    pts = []
    p = snap_path(stream)
    if not p.exists():
        return pts
    try:
        with gzip.open(p, 'rt', encoding='ascii') as f:
            for line in f:
                pts.append(int(line[7:line.index(',')]))
    except (EOFError, OSError, gzip.BadGzipFile, ValueError):
        pass
    return pts


# layout digests that were the same layout: the digest repr'd two sets in hash-seed order until 2026-11-01 (hash2)
LAYOUT_ALIAS = {'a702f75bb49e': '79e97611a208'}
# informational columns, never in the divergence report: maprng = EEMap::mRandom, seeded per run outside the pins and
# never drawn in play (differs between two runs of the original from PT 0; dbg3 2026-11-01); shown once at the top
INFO_COLS = {'maprng'}
COLS = ['world', 'render', 'rng', 'maprng', 'nunits', 'inputs']
# maprng (EEMap::mRandom) is per-run (not pinned, never drawn in play) and inputs (gU531CF0, inputs processed: the
# game's own civ-timer messages arrive through the local-input path at wall-time-dependent PTs) are not part of world:
# informational only


def fmt(raw, off, size, typ):
    b = raw[off:off + size]
    if typ == 'float' and size == 4:
        return '%08x (%.7g)' % (struct.unpack('<I', b)[0], struct.unpack('<f', b)[0])
    if size == 4:
        return '%08x (%d)' % (struct.unpack('<I', b)[0], struct.unpack('<i', b)[0])
    return b.hex()


def diff_record(ra, rb, table, size, pointers, render_only=None):
    """Differing members of two raw records: [(name, type, class, a, b)]; class sim|render|pointer."""
    out = []
    for off, sz, name, typ, count in table:
        if off >= size:
            continue
        sz = min(sz, size - off)
        a, b = ra[off:off + sz], rb[off:off + sz]
        if a == b:
            continue
        ptr = not L.hashed(typ)
        if ptr and not pointers:
            continue
        cls = 'pointer/container' if ptr else 'render' if name in L.RENDER_FIELDS else 'noise' if name in L.NOISE else 'ui' if name in L.UI_FIELDS or name in L.EXTRA_UI_FIELDS else 'sim'
        if name in L.RENDER_FIELDS and L.RENDER_FIELDS[name] != 0xff:   # partly render-written: split the bits
            bits = L.RENDER_FIELDS[name]
            if (a[0] & ~bits) != (b[0] & ~bits):
                cls = 'sim'
        if L.hashed(typ) and SIZE_OK(typ, count) and count and count > 1:
            w = L.SCALARS[typ]
            for i in range(count):
                if a[i * w:(i + 1) * w] != b[i * w:(i + 1) * w]:
                    out.append(('%s[%d]' % (name, i), typ, cls, fmt(ra, off + i * w, w, typ), fmt(rb, off + i * w, w, typ)))
            continue
        w = L.SCALARS.get(typ, sz) if L.hashed(typ) else sz
        out.append((name, typ, cls, fmt(ra, off, min(w, sz), typ), fmt(rb, off, min(w, sz), typ)))
    return out


def SIZE_OK(typ, count):
    return typ in L.SCALARS


def diff_snapshots(sa, sb, pointers=False, max_units=20):
    rep = {'pt': sa['pt']}
    pa, pb = bytes.fromhex(sa['physics']), bytes.fromhex(sb['physics'])
    rep['physics'] = [(name, fmt(pa, o, 4, 'float' if t == 'f' else 'long'), fmt(pb, o, 4, 'float' if t == 'f' else 'long'))
                      for o, t, name in L.PHYSICS_FIELDS if pa[o:o + 4] != pb[o:o + 4]]
    rep['maprng'] = None if sa['maprng'] == sb['maprng'] else (sa['maprng'], sb['maprng'])
    rep['inputs'] = None if sa['inputs'] == sb['inputs'] else (sa['inputs'], sb['inputs'])
    ptab, utab = L.player_table(), L.unit_table()
    players = []
    for x, y in zip(sa['players'], sb['players']):
        d = diff_record(bytes.fromhex(x['raw']), bytes.fromhex(y['raw']), ptab, L.PLAYER_SIZE, pointers)
        st = [(i, a, b) for i, (a, b) in enumerate(zip(x['stats'], y['stats'])) if a != b]
        if d or st or x['epoch'] != y['epoch']:
            players.append(dict(index=x['index'], slot=x['slot'], fields=d, stats=st,
                                epoch=None if x['epoch'] == y['epoch'] else (x['epoch'], y['epoch'])))
    rep['players'] = players
    ua = {u['id']: u for u in sa['units']}; ub = {u['id']: u for u in sb['units']}
    rep['only_a'] = sorted(set(ua) - set(ub)); rep['only_b'] = sorted(set(ub) - set(ua))
    order_a = [u['id'] for u in sa['units'] if u['id'] in ub]; order_b = [u['id'] for u in sb['units'] if u['id'] in ua]
    rep['order_differs'] = order_a != order_b
    units = []
    for uid in [u['id'] for u in sa['units'] if u['id'] in ub]:
        x, y = ua[uid], ub[uid]
        ra, rb = bytes.fromhex(x['raw']), bytes.fromhex(y['raw'])
        if x['kind'] == 'complex' and y['kind'] == 'complex':   # v6: conditional fields (mU118 while mF214 unset)
            ra, rb = L.unit_zero_if(ra), L.unit_zero_if(rb)
        d = diff_record(ra, rb, utab, min(len(ra), len(rb)), pointers)
        extra = []
        if x['cls'] != y['cls']:
            extra.append(('class id (+0x28 of mUnitClass)', 'id', 'sim', x['cls'], y['cls']))
        if x['player'] != y['player']:
            extra.append(('owner (world player index)', 'id', 'sim', x['player'], y['player']))
        for name, a, b in zip(L.UNIT_REFS, x.get('refs', []), y.get('refs', [])):
            if a != b:
                extra.append((name + ' -> unit id', 'id', 'sim', a, b))
        ga, gb = bytes.fromhex(x['goal']), bytes.fromhex(y['goal'])
        if ga[4:0x20] != gb[4:0x20]:   # EEUnitGoal's own members (+0x04..+0x1d); the rest depends on the goal class
            for o in range(4, 0x20, 4):
                if ga[o:o + 4] != gb[o:o + 4]:
                    extra.append(('mGoalState+0x%x' % o, 'goal', 'sim', ga[o:o + 4].hex(), gb[o:o + 4].hex()))
        if d or extra:
            units.append(dict(id=uid, player=x['player'], kind=x['kind'], cls=x['cls'], fields=extra + d))
    rep['units_differing'] = len(units)
    rep['sim_units'] = sum(1 for u in units if any(f[2] == 'sim' for f in u['fields']))
    rep['units'] = sorted(units, key=lambda u: (not any(f[2] == 'sim' for f in u['fields']), u['id']))[:max_units]
    return rep


def ai_report(a, res, aia, aib, sta, stb):
    """The ai column: the first PT whose AI hash differs (per player) and its ranges, against the first unit/player
    and world divergence; then the AI objects that differ in the snapshots around it, by object path."""
    if not aia or not aib:
        if aia or aib:
            print('\nai column only in %s' % ('A' if aia else 'B'))
        return
    common = sorted(set(aia) & set(aib))
    pos = {pt: k for k, pt in enumerate(common)}
    bad = [pt for pt in common if aia[pt] != aib[pt]]
    res['ai'] = dict(hashed_a=len(aia), hashed_b=len(aib), common=len(common), cost_a=sta[-1] if sta else None,
                     cost_b=stb[-1] if stb else None)
    for tag, st in (('A', sta), ('B', stb)):
        if st:
            print('ai cost %s: %.3f ms/line, %d objects/line (at PT %d), %d read failures%s' % (
                tag, st[-1]['ai_ms_per_line'], st[-1]['ai_objects_per_line'], st[-1]['pt'], st[-1]['ai_read_failures'],
                # v7 (hash3): identity-check sentinels and page-cache evictions; v<=6 failures include cache-full misses
                ', %d invalid (identity sentinels), %d cache evictions' % (st[-1]['ai_invalid'], st[-1]['cache_evictions'])
                if 'ai_invalid' in st[-1] else ''))
    if not bad:
        res['ai']['identical_to'] = common[-1] if common else None
        print('\nAI identical over all %d common hashed PTs (to PT %s)' % (len(common), common[-1] if common else None))
        return
    first = bad[0]
    if res.get('verdict') in ('identical', 'render-only'):
        res['verdict'] = 'ai-divergence'   # exit 4: units/players identical, the AI objects differ
    players = [i for i, (p, q) in enumerate(zip(aia[first], aib[first])) if p != q]
    rs = []
    for pt in bad:
        if rs and pos[pt] == rs[-1][2] + 1:
            rs[-1][1] = pt; rs[-1][2] += 1
        else:
            rs.append([pt, pt, pos[pt]])
    before = [p for p in common if p < first]
    res['ai'].update(first=first, identical_to=before[-1] if before else None, players=players, count=len(bad),
                     ranges=[r[:2] for r in rs[:20]], nranges=len(rs),
                     persistent_from=rs[-1][0] if rs[-1][1] == common[-1] else None,
                     first_world=res.get('first', {}).get('world'), first_players=res.get('first', {}).get('players'))
    print('\nAI identical up to PT %s; first ai divergence PT %d, players %s; %d PTs in %d ranges %s%s' % (
        res['ai']['identical_to'], first, players, len(bad), len(rs), [r[:2] for r in rs[:6]],
        ', persistent from PT %d' % rs[-1][0] if res['ai']['persistent_from'] else ''))
    print('  vs first world divergence PT %s, first unit/player divergence PT %s' % (
        res['ai']['first_world'], res['ai']['first_players']))
    if a.no_snapshots:
        return
    both = sorted(set(snapshot_pts(a.a)) & set(snapshot_pts(a.b)))
    le = [p for p in both if p <= first]; ge = [p for p in both if p >= first]
    want = set(([le[-1]] if le else []) + ([ge[0]] if ge else []))
    sa, sb = snapshots(a.a, want), snapshots(a.b, want)
    res['ai']['snapshots'] = []
    for pt in sorted(want):
        if 'ai' not in sa.get(pt, {}) or 'ai' not in sb.get(pt, {}):
            continue
        d = diff_ai(sa[pt], sb[pt])
        d['pt'] = pt
        res['ai']['snapshots'].append(d)
        print('\n AI snapshot PT %d (%s the first ai divergence):' % (pt, 'at' if pt == first else 'before' if pt < first else 'after'))
        print_ai(d)
    if not want:
        print('  (no snapshot common to both runs around the ai divergence; use dump.py --pt %d)' % first)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('a'); ap.add_argument('b')
    ap.add_argument('--json')
    ap.add_argument('--max-units', type=int, default=20)
    ap.add_argument('--pointers', action='store_true')
    ap.add_argument('--no-snapshots', action='store_true')
    ap.add_argument('--at', default='', help='comma list of snapshot PTs to diff instead of the ones around the first divergence')
    a = ap.parse_args()
    aia, aib, sta, stb = {}, {}, [], []
    ha, ra, ta = load_stream(a.a, aia, sta); hb, rb, tb = load_stream(a.b, aib, stb)
    if (ha or {}).get('unit_neg1') or (hb or {}).get('unit_neg1'):   # opt-in (round 40 mp): same snapshot view
        L.enable_mp_mask()
    for h in (ha, hb):   # version-8 (--human-script) streams: their extra UI fields are labelled ui in the diffs
        if h and h.get('ui_extra'):
            L.EXTRA_UI_FIELDS.update(h['ui_extra'])
    common = sorted(set(ra) & set(rb))
    res = dict(a=a.a, b=b if (b := a.b) else None, sampled_a=len(ra), sampled_b=len(rb), common=len(common),
               range_a=[min(ra), max(ra)] if ra else None, range_b=[min(rb), max(rb)] if rb else None,
               trailer_a=ta, trailer_b=tb)
    if ha and hb and (LAYOUT_ALIAS.get(ha.get('layout'), ha.get('layout')), ha.get('version'), ha.get('hasher')) != (
            LAYOUT_ALIAS.get(hb.get('layout'), hb.get('layout')), hb.get('version'), hb.get('hasher')):
        res['warning'] = 'different sampler layouts %s vs %s: hashes are not comparable' % (ha.get('layout'), hb.get('layout'))
    print('A: %d PTs %s  B: %d PTs %s  common %d' % (len(ra), res['range_a'], len(rb), res['range_b'], len(common)))
    if common:
        mi = COLS.index('maprng'); x0, y0 = ra[common[0]][mi], rb[common[0]][mi]
        print('maprng (EEMap::mRandom, per-run seed, informational, not compared): A %s B %s' % (x0, y0))
    if res.get('warning'):
        print('WARNING', res['warning'])
    if not common:
        print('nothing in common'); res['verdict'] = 'none'
        if a.json: pathlib.Path(a.json).write_text(json.dumps(res, indent=1))
        sys.exit(3)
    first = {}
    for pt in common:
        x, y = ra[pt], rb[pt]
        for i, c in enumerate(COLS):
            if c not in first and c not in INFO_COLS and x[i] != y[i]:
                first[c] = pt
        if 'players' not in first and x[len(COLS):] != y[len(COLS):]:
            first['players'] = pt
    # every column's divergent PTs as ranges: a transient difference (e.g. an ulp in a push vector that the sim later
    # zeroes) shows as short ranges; 'persistent_from' = the PT from which the column differs to the end
    ranges, persistent = {}, {}
    for i, c in enumerate(COLS + ['players']):
        if c in INFO_COLS:
            continue
        bad = [(k, pt) for k, pt in enumerate(common)
               if (ra[pt][i] != rb[pt][i] if c != 'players' else ra[pt][len(COLS):] != rb[pt][len(COLS):])]
        if not bad:
            continue
        bad_pts = [pt for _, pt in bad]
        rs = []
        for k, pt in bad:
            if rs and k == rs[-1][2] + 1:
                rs[-1][1] = pt; rs[-1][2] = k
            else:
                rs.append([pt, pt, k])
        ranges[c] = dict(count=len(bad_pts), ranges=[r[:2] for r in rs[:20]], nranges=len(rs))
        if rs[-1][1] == common[-1]:
            persistent[c] = rs[-1][0]
    res['ranges'] = ranges; res['persistent_from'] = persistent
    sim = first.get('world')
    res['first'] = first
    if sim is None:
        res['identical_to'] = common[-1]
        res['verdict'] = 'identical' if 'render' not in first else 'render-only'
        print('SIM identical over all %d common PTs (to PT %d)' % (len(common), common[-1]))
        if 'render' in first:
            print('RENDER hash first differs at PT %d (render-written fields only: noise unless the sim follows)' % first['render'])
    else:
        before = [p for p in common if p < sim]
        res['identical_to'] = before[-1] if before else None
        res['verdict'] = 'sim-divergence'
        x, y = ra[sim], rb[sim]
        cols = [c for i, c in enumerate(COLS) if x[i] != y[i] and c != 'maprng']
        players = [i for i, (p, q) in enumerate(zip(x[len(COLS):], y[len(COLS):])) if p != q]
        res['at_first'] = dict(columns=cols, players=players)
        if 'inputs' in first and first['inputs'] <= sim:
            res['input_timing'] = True
            print('NOTE inputs processed (gU531CF0) differ from PT %d: a UI/harness input (e.g. the civ-timer expiry '
                  'EEMCivTimerExpired, posted on UI time) reached the sim at different PTs: harness timing, not code'
                  % first['inputs'])
        print('SIM identical up to PT %s; first divergent PT %d: columns %s, players %s' % (res['identical_to'], sim, cols, players))
        for c, r in ranges.items():
            print('  %-8s differs at %d PTs in %d ranges %s%s%s' % (c, r['count'], r['nranges'], r['ranges'][:6],
                  '...' if r['nranges'] > 6 else '', ', persistent from PT %d' % persistent[c] if c in persistent else ''))
        for c in ('rng', 'render', 'nunits', 'players'):
            if c in first:
                print('  first %s difference at PT %d' % (c, first[c]))
        if not a.no_snapshots:
            spa, spb = set(snapshot_pts(a.a)), set(snapshot_pts(a.b))
            both = sorted(spa & spb)
            le = [p for p in both if p <= sim]; ge = [p for p in both if p >= sim]
            want = set(([le[-1]] if le else []) + ([ge[0]] if ge else []))
            if a.at:
                want = {int(x) for x in a.at.split(',') if x} & set(both)
            sa, sb = snapshots(a.a, want), snapshots(a.b, want)
            res['snapshots'] = []
            for pt in sorted(want):
                d = diff_snapshots(sa[pt], sb[pt], a.pointers, a.max_units)
                res['snapshots'].append(d)
                print('\nsnapshot PT %d (%s the first divergence):' % (pt, 'at' if pt == sim else 'before' if pt < sim else 'after'))
                for name, p, q in d['physics']:
                    print('  physics %-24s %s | %s' % (name, p, q))
                pass   # map rng: informational (INFO_COLS), kept in the json only
                for pl in d['players']:
                    print('  player %d (slot %d): %d fields, %d stats series, epoch %s' % (pl['index'], pl['slot'], len(pl['fields']), len(pl['stats']), pl['epoch']))
                    for f in pl['fields'][:10]:
                        print('    %-28s [%s] %s | %s' % (f[0], f[2], f[3], f[4]))
                if d['only_a'] or d['only_b']:
                    print('  units only in A: %s; only in B: %s' % (d['only_a'][:20], d['only_b'][:20]))
                if d['order_differs']:
                    print('  world unit list order differs')
                print('  units differing: %d (%d with sim fields)' % (d['units_differing'], d['sim_units']))
                for u in d['units']:
                    print('   unit %d (player %d, %s, class %s):' % (u['id'], u['player'], u['kind'], u['cls']))
                    for f in u['fields'][:12]:
                        print('     %-30s [%s] %s | %s' % (f[0], f[2], f[3], f[4]))
            if not want:
                print('(no snapshot common to both runs around the divergence; use dump.py)')
    ai_report(a, res, aia, aib, sta, stb)
    if a.json:
        pathlib.Path(a.json).write_text(json.dumps(res, indent=1))
    sys.exit({'identical': 0, 'render-only': 2, 'ai-divergence': 4}.get(res['verdict'], 1))


if __name__ == '__main__':
    main()
