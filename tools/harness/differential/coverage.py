"""TEST ONLY (round 39, agent cov): which non-exact game functions do the campaign games execute?

  coverage.py list   --scores SCORES.json --out DIR [--fulllink-map MAP]
      DIR/nonexact.tsv (va, unit, status, score, size, equiv verdict, symbol) for every row whose status is not
      exact/bind_only, DIR/list_original.txt (rva + original va label) and, with a map, DIR/list_fulllink.txt (the
      same functions located in the full-link image by their names.txt symbol; label = original va).
  coverage.py env    DIR OUTDIR
      prints the EE_COVERAGE value for exe_boot.c (differential/coverage.h) for these lists.
  coverage.py report --list DIR --batch BATCHDIR... [--extra LABEL=COV.tsv ...] --out DIR
      joins every job of the given game_batch output dirs (<label>.json -> evidence raw log -> "TEST coverage armed
      ... out FILE") with its .cov.tsv and writes DIR/coverage.tsv + DIR/COVERAGE.md.
"""
import argparse
import collections
import json
import os
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
BASE = 0x400000


def load_sizes():
    sizes = {}
    with open(ROOT / 'analysis/exe/functions.jsonl', encoding='utf-8') as f:
        for line in f:
            r = json.loads(line)
            sizes[r['rva'] + BASE] = r['size']
    return sizes


def load_equiv():
    out = {}
    with open(ROOT / 'analysis/exe/equiv_sig.jsonl', encoding='utf-8') as f:
        for line in f:
            r = json.loads(line)
            out[int(r['rva'], 16)] = (r.get('verdict', ''), r.get('symbol', ''), r.get('date', ''))   # last record wins
    return out


def load_names():
    names = {}
    for line in (ROOT / 'analysis/exe/names.txt').read_text(encoding='latin-1').splitlines():
        if line.startswith('#') or not line.strip():
            continue
        parts = line.split(None, 1)
        if len(parts) == 2:
            names.setdefault(int(parts[0], 16) + BASE, parts[1].split()[0])
    return names


def folder_of(unit):
    p = pathlib.PureWindowsPath(unit).parts
    try:
        i = p.index('src')
        return '/'.join(p[i + 1:i + 3])   # e.g. Empire Earth/AI Thread
    except ValueError:
        return p[0]


def cmd_list(a):
    scores = json.loads(pathlib.Path(a.scores).read_text(encoding='utf-8'))
    sizes, equiv, names = load_sizes(), load_equiv(), load_names()
    out = pathlib.Path(a.out); out.mkdir(parents=True, exist_ok=True)
    rows = []
    for unit, funcs in scores.items():
        for va, (status, score) in funcs.items():
            if status in ('exact', 'bind_only'):
                continue
            va = int(va, 16)
            v = equiv.get(va, ('', '', ''))
            rows.append(dict(va=va, unit=unit, folder=folder_of(unit), status=status, score=score, size=sizes.get(va, 0),
                             verdict=v[0] or '-', symbol=names.get(va) or v[1] or '-'))
    rows.sort(key=lambda r: r['va'])
    with open(out / 'nonexact.tsv', 'w', encoding='utf-8') as f:
        f.write('va\tunit\tfolder\tstatus\tscore\tsize\tverdict\tsymbol\n')
        for r in rows:
            f.write(f"{r['va']:x}\t{r['unit']}\t{r['folder']}\t{r['status']}\t{r['score']}\t{r['size']}\t{r['verdict']}\t{r['symbol']}\n")
    (out / 'list_original.txt').write_text(''.join(f"{r['va'] - BASE:x} {r['va']:x}\n" for r in rows), encoding='ascii')
    print(f'{len(rows)} non-exact rows ({sum(r["size"] for r in rows)} B) -> {out}')
    if a.fulllink_map:
        sys.path.insert(0, str(ROOT / 'tools/boot'))
        from fulllink_locator import LinkMap
        lm = LinkMap(a.fulllink_map)
        ok = miss = 0
        with open(out / 'list_fulllink.txt', 'w', encoding='ascii') as f:
            for r in rows:
                sym = names.get(r['va'])
                if sym and sym in lm.by_name:
                    f.write(f"{lm.by_name[sym] - BASE:x} {r['va']:x}\n"); ok += 1
                else:
                    miss += 1
        print(f'full link: {ok} mapped by symbol, {miss} without a names.txt symbol in the map')


def cmd_env(a):
    d = pathlib.Path(a.dir).resolve()
    o = pathlib.Path(a.outdir).resolve(); o.mkdir(parents=True, exist_ok=True)
    full = d / 'list_fulllink.txt'
    print(f"{d / 'list_original.txt'} {full if full.exists() else d / 'list_original.txt'} {o}")


def read_cov(path):
    hits = {}
    for line in pathlib.Path(path).read_text(encoding='utf-8').splitlines():
        if line.startswith('#'):
            continue
        p = line.split('\t')
        if len(p) >= 8 and p[2] == 'hit':
            hits[int(p[0], 16)] = dict(order=int(p[3]), ptkey=int(p[4]), pthits=int(p[5]), ms=int(p[6]))
        elif len(p) >= 3 and p[2] == 'guarded':
            hits.setdefault(int(p[0], 16), dict(order=0, ptkey=-1, pthits=0, ms=0, guarded=True))
    return hits


def batch_jobs(batch):
    """(label, cov path, session json) for every job of a game_batch output dir"""
    out = []
    for js in sorted(pathlib.Path(batch).glob('*.json')):
        if js.name == 'batch.json':
            continue
        try:
            j = json.loads(js.read_text(encoding='utf-8'))
        except Exception:
            continue
        ev = j.get('evidence') or (j.get('session') or {}).get('evidence')
        cov = None
        raws = list(pathlib.Path(ev).glob('*.session.raw.txt')) if ev else []
        for raw in raws:
            for line in raw.read_text(encoding='utf-8', errors='replace').splitlines():
                m = re.match(r'TEST coverage armed .* out (.+)$', line)
                if m:
                    cov = m.group(1).strip()
        log = js.with_suffix('.log')
        if log.exists():
            text = log.read_text(encoding='utf-8', errors='replace')
            m = re.findall(r'^AI GAME (.*)$', text, re.M) or re.findall(r'^(UI EXERCISE FAILED: .*)$', text, re.M)
            j['_outcome'] = (m[-1] if m else ('exercise ok' if j.get('exercise', {}).get('done') else '-')).replace('|', '/')[:90]
        out.append((js.stem, cov, j))
    return out


def cmd_report(a):
    d = pathlib.Path(a.list)
    rows = []
    with open(d / 'nonexact.tsv', encoding='utf-8') as f:
        hdr = f.readline().rstrip('\n').split('\t')
        for line in f:
            r = dict(zip(hdr, line.rstrip('\n').split('\t')))
            r['va'] = int(r['va'], 16); r['size'] = int(r['size'])
            rows.append(r)
    games = []   # (label, hits)
    for b in a.batch:
        for label, cov, j in batch_jobs(b):
            if cov and pathlib.Path(cov).exists():
                games.append((f'{pathlib.Path(b).name}/{label}', read_cov(cov), j))
            else:
                print(f'no coverage file for {b}/{label} ({cov})')
    for e in a.extra or []:
        label, path = e.split('=', 1)
        games.append((label, read_cov(path), {}))
    out = pathlib.Path(a.out); out.mkdir(parents=True, exist_ok=True)
    by = {r['va']: r for r in rows}
    for r in rows:
        r['games'] = []; r['first_pt'] = None; r['guarded'] = False
    for label, hits, _ in games:
        for va, h in hits.items():
            r = by.get(va)
            if not r:
                continue
            if h.get('guarded'):
                r['guarded'] = True; continue
            r['games'].append(label)
            if h['ptkey'] >= 0 and (r['first_pt'] is None or h['ptkey'] < r['first_pt']):
                r['first_pt'] = h['ptkey']
    with open(out / 'coverage.tsv', 'w', encoding='utf-8') as f:
        f.write('va\texecuted\tngames\tfirst_pt\tsize\tstatus\tscore\tverdict\tfolder\tunit\tsymbol\tgames\n')
        for r in sorted(rows, key=lambda r: (bool(r['games']), -r['size'])):
            ex = 'yes' if r['games'] else ('guarded' if r['guarded'] else 'never')
            fp = '' if r['first_pt'] is None else str(r['first_pt'])
            f.write(f"{r['va']:x}\t{ex}\t{len(r['games'])}\t{fp}\t{r['size']}\t{r['status']}\t{r['score']}\t{r['verdict']}\t"
                    f"{r['folder']}\t{r['unit']}\t{r['symbol']}\t{','.join(r['games'])}\n")
    # summary
    md = ['# Non-exact function coverage of the campaign games (round 39, agent cov)', '',
          'TEST ONLY. One-shot int3 at the entry of every non-exact game function (status not exact/bind_only), '
          'tools/boot/differential/coverage.h via `EE_COVERAGE`; original image (orig-bin/aoc/EE-AOC.unwrapped.exe).',
          'An entry counts as executed when any game hit it. "first PT" is the smallest C-hasher PT key at the first hit '
          '(blank: hit before PT sync started, i.e. startup/menus/loading, or the game ran without --diff-stream).', '',
          *([pathlib.Path(a.preface).read_text(encoding='utf-8').rstrip(), ''] if a.preface else []),
          '## Games', '', '| game | seed | job args | wall s | last PT | outcome | non-exact hit | only this game |',
          '|---|---:|---|---:|---:|---|---:|---:|']
    for label, hits, j in games:
        b = j.get('batch', {})
        hit = {va for va, h in hits.items() if not h.get('guarded') and va in by}
        only = sum(1 for va in hit if len(by[va]['games']) == 1)
        al, args = list(b.get('args', [])), []
        while al:
            x = al.pop(0)
            if x in ('--timeout', '--max-game-seconds'): al and al.pop(0); continue
            if x not in ('--exercise', '--diff-stream', 'auto', '--lobby', '--no-human-defeat'): args.append(x)
        args = ' '.join(args).replace('--ai-players ', 'AI ').replace('--map-size ', '').replace('--map-type ', '').replace('--difficulty ', '')
        md.append(f"| {label} | {b.get('seed', '-')} | {args} | {b.get('wall_s', '-')} | {(b.get('pt') or {}).get('last', ['', '-'])[1]} | "
                  f"{j.get('_outcome', '-')} | {len(hit)} | {only} |")
    ex = [r for r in rows if r['games']]; nv = [r for r in rows if not r['games']]
    tb = lambda rs: sum(r['size'] for r in rs)
    md += ['', '## Summary', '', f'- non-exact rows: {len(rows)} ({tb(rows):,} B)',
           f'- executed in at least one game: {len(ex)} ({tb(ex):,} B, {100 * tb(ex) / max(1, tb(rows)):.1f}% of bytes)',
           f'- never executed: {len(nv)} ({tb(nv):,} B)',
           f"- harness-guarded (not measurable, counted never): {sum(1 for r in rows if r['guarded'] and not r['games'])}", '']
    md += ['### By equiv_sig verdict', '', '| verdict | executed n | executed B | never n | never B |', '|---|---:|---:|---:|---:|']
    for v in sorted({r['verdict'] for r in rows}):
        e1 = [r for r in ex if r['verdict'] == v]; n1 = [r for r in nv if r['verdict'] == v]
        md.append(f'| {v} | {len(e1)} | {tb(e1):,} | {len(n1)} | {tb(n1):,} |')
    md += ['', '### By folder', '', '| folder | executed n | executed B | never n | never B |', '|---|---:|---:|---:|---:|']
    folders = collections.defaultdict(lambda: [[], []])
    for r in rows:
        folders[r['folder']][0 if r['games'] else 1].append(r)
    for k, (e1, n1) in sorted(folders.items(), key=lambda kv: -tb(kv[1][1])):
        md.append(f'| {k} | {len(e1)} | {tb(e1):,} | {len(n1)} | {tb(n1):,} |')
    pre = [r for r in ex if r['first_pt'] is None]; few = [r for r in ex if len(r['games']) == 1]
    md += ['', '### How strongly executed', '',
           f'- first hit only outside synced PTs (startup, menus, lobby, loading; or a game without --diff-stream): '
           f'{len(pre)} ({tb(pre):,} B)',
           f'- executed in exactly one game: {len(few)} ({tb(few):,} B)',
           f'- executed in every game: {sum(1 for r in ex if len(r["games"]) == len(games))}']
    md += ['', '## Never executed (review work list, by bytes)', '',
           'Full list with unit and symbol: coverage.tsv (rows with executed=never).', '',
           '| va | bytes | status | score | verdict | folder | symbol |', '|---|---:|---|---:|---|---|---|']
    for r in sorted(nv, key=lambda r: -r['size'])[:a.top]:
        md.append(f"| {r['va']:x} | {r['size']} | {r['status']} | {r['score']} | {r['verdict']} | {r['folder']} | `{r['symbol']}` |")
    if len(nv) > a.top:
        md.append(f'| ... {len(nv) - a.top} more in coverage.tsv | | | | | | |')
    (out / 'COVERAGE.md').write_text('\n'.join(md) + '\n', encoding='utf-8')
    print(f'{len(games)} games; executed {len(ex)} ({tb(ex)} B), never {len(nv)} ({tb(nv)} B) -> {out}')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('list'); p.add_argument('--scores', required=True); p.add_argument('--out', required=True)
    p.add_argument('--fulllink-map')
    p = sub.add_parser('env'); p.add_argument('dir'); p.add_argument('outdir')
    p = sub.add_parser('report'); p.add_argument('--list', required=True); p.add_argument('--batch', nargs='*', default=[])
    p.add_argument('--extra', nargs='*'); p.add_argument('--out', required=True); p.add_argument('--top', type=int, default=400)
    p.add_argument('--preface', help='markdown inserted after the header (campaign notes)')
    a = ap.parse_args()
    {'list': cmd_list, 'env': cmd_env, 'report': cmd_report}[a.cmd](a)


if __name__ == '__main__':
    main()
