"""The computer-player decision trace (docs/AI_RESEARCH.md stage 0; the game side is ee/EECPTrace.h).

A test-profile game run with EE_AI_TRACE=1 in the environment writes one debug string per AI decision; the harness
keeps them in the session log as `debugstring EE-AI ...` lines:

  EE-AI <game ms> p<player> <manager> <event> key=value ... | f=<food> w=<wood> s=<stone> g=<gold> i=<iron>

`EE-AIPROF` lines carry the AI's real-time cost; they vary between runs and are never compared.

  python -I tools/harness/ai_trace.py extract LOG [-o OUT]     # the EE-AI lines of a session log (or an evidence dir)
  python -I tools/harness/ai_trace.py compare LOG_A LOG_B [--until MS]   # exit 0 when identical, else the first diff
  python -I tools/harness/ai_trace.py summary LOG [--json]     # per-player metrics (events, epochs, last stats, profile)

LOG may be a session .txt/.raw.txt file or an evidence directory (its *.session.txt is used).
"""
import argparse
import collections
import json
import pathlib
import re
import sys

PREFIX = 'debugstring '
LINE = re.compile(r'^EE-AI (\d+) p(\d+) (\S+) (\S+) ?(.*?) \| f=(-?\d+) w=(-?\d+) s=(-?\d+) g=(-?\d+) i=(-?\d+)\s*$')
PROF = re.compile(r'^EE-AIPROF (\d+) p(\d+) updates=(\d+) sum_us=(\d+) max_us=(\d+)')
KV = re.compile(r'(\w+)=(\S+)')
RESOURCES = ('food', 'wood', 'stone', 'gold', 'iron')


def log_path(path):
    p = pathlib.Path(path)
    if p.is_dir():
        found = sorted(p.glob('*.session.txt')) or sorted(p.glob('*.session.raw.txt'))
        if not found:
            sys.exit(f'{p}: no *.session.txt')
        return found[0]
    return p


def debug_lines(path, prefix):
    """The debug strings of a log that start with prefix, in order (the harness prefix stripped)."""
    out = []
    with open(log_path(path), encoding='utf-8', errors='replace') as f:
        for line in f:
            if line.startswith(PREFIX + prefix):
                out.append(line[len(PREFIX):].rstrip('\r\n'))
    return out


INPUTS = re.compile(r' inputs=\d+')


def trace(path):
    return debug_lines(path, 'EE-AI ')


def compared(lines):
    """The trace as compared: the sim hash's input count is left out. The harness's own UI inputs (the civilization
    timer's messages at 240/270/300 game s) execute at wall-time-dependent PTs by design (ai_game.py), and the input
    count is a local counter; the simulation they reach is in the other hash fields."""
    return [INPUTS.sub('', x) for x in lines]


def parse(line):
    m = LINE.match(line)
    if not m:
        return None
    t, player, manager, event, rest = int(m[1]), int(m[2]), m[3], m[4], m[5]
    return dict(t=t, player=player, manager=manager, event=event, fields=dict(KV.findall(rest)),
                resources=dict(zip(RESOURCES, (int(v) for v in m.groups()[5:]))))


def line_time(line):
    try:
        return int(line.split(' ', 2)[1])
    except (IndexError, ValueError):
        return 0


def compare(a, b, out=sys.stdout, until=None):
    """Identical traces -> True; else prints the first differing line with context and returns False.
    A time-capped run stops at a harness poll, so the two runs end at different game times: both traces are cut
    before the earlier run's last game ms (or before `until`)."""
    ta, tb = compared(trace(a)), compared(trace(b))
    if until is None and ta and tb:
        until = min(line_time(ta[-1]), line_time(tb[-1]))
    if until is not None:
        ta = [x for x in ta if line_time(x) < until]
        tb = [x for x in tb if line_time(x) < until]
        print(f'comparing the lines before game ms {until}', file=out)
    for k, (x, y) in enumerate(zip(ta, tb)):
        if x != y:
            print(f'traces differ at line {k + 1} (of {len(ta)} / {len(tb)}):', file=out)
            for c in range(max(0, k - 3), k):
                print(f'    {ta[c]}', file=out)
            print(f'  A {x}\n  B {y}', file=out)
            return False
    if len(ta) != len(tb):
        short, n = ('A', len(ta)) if len(ta) < len(tb) else ('B', len(tb))
        longer = tb if short == 'A' else ta
        print(f'traces agree for {n} lines, then {short} ends; the other goes on with: {longer[n]}', file=out)
        return False
    print(f'traces identical: {len(ta)} lines', file=out)
    return True


def summary(path):
    """Per-player metrics: event counts, the start line, epoch advance times, the stats over time and the profile."""
    players = collections.defaultdict(lambda: dict(events=collections.Counter(), epochs=[], stats=[], start=None,
                                                   first_attack_ms=None, prof_sum_us=0, prof_updates=0,
                                                   prof_max_us=0))
    for line in trace(path):
        e = parse(line)
        if e is None or e['manager'] == 'sim':   # the sim hash lines (p0) are for compare only
            continue
        p = players[e['player']]
        p['events'][e['event']] += 1
        if e['event'] == 'start':
            p['start'] = dict(e['fields'], t=e['t'])
        elif e['event'] == 'epoch.advance':
            p['epochs'].append(dict(t=e['t'], **{k: int(v) for k, v in e['fields'].items()}))
        elif e['event'] == 'attack.group' and e['fields'].get('kept') == '1' and p['first_attack_ms'] is None:
            p['first_attack_ms'] = e['t']
        elif e['event'] == 'stats':
            row = dict(t=e['t'], **{k: int(v) for k, v in e['fields'].items() if re.fullmatch(r'-?\d+', v)})
            if 'gather' in e['fields']:   # gatherers by resource: food/wood/stone/gold/iron/unknown
                for name, v in zip(RESOURCES + ('unknown',), e['fields']['gather'].split('/')):
                    row['gather_' + name] = int(v)
            row['unspent'] = sum(e['resources'].values())
            row.update(e['resources'])
            p['stats'].append(row)
    for line in debug_lines(path, 'EE-AIPROF '):
        m = PROF.match(line)
        if m:
            p = players[int(m[2])]
            p['prof_updates'] += int(m[3])
            p['prof_sum_us'] += int(m[4])
            p['prof_max_us'] = max(p['prof_max_us'], int(m[5]))
    out = {}
    for pid in sorted(players):
        p = players[pid]
        stats = p['stats']
        last = stats[-1] if stats else {}
        out[pid] = dict(
            start=p['start'], events=dict(sorted(p['events'].items())), epoch_advances=p['epochs'],
            first_attack_ms=p['first_attack_ms'], last_stats=last,
            mean_unspent=round(sum(s['unspent'] for s in stats) / len(stats)) if stats else None,
            mean_idle=round(sum(s.get('idle', 0) for s in stats) / len(stats), 2) if stats else None,
            pop_blocked_samples=sum(1 for s in stats if s.get('pop', 0) >= s.get('cap', 1 << 30)),
            stats_samples=len(stats), stats=stats,
            profile=dict(updates=p['prof_updates'], mean_us=round(p['prof_sum_us'] / p['prof_updates'])
                         if p['prof_updates'] else None, max_us=p['prof_max_us']))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    e = sub.add_parser('extract')
    e.add_argument('log')
    e.add_argument('-o', '--out')
    c = sub.add_parser('compare')
    c.add_argument('a')
    c.add_argument('b')
    c.add_argument('--until', type=int, help='compare the lines before this game ms (default: the earlier end)')
    s = sub.add_parser('summary')
    s.add_argument('log')
    s.add_argument('--json', action='store_true', help='the full summary (with every stats row) as JSON')
    a = ap.parse_args()
    if a.cmd == 'extract':
        text = '\n'.join(trace(a.log)) + '\n'
        if a.out:
            pathlib.Path(a.out).write_text(text, encoding='utf-8')
        else:
            sys.stdout.write(text)
    elif a.cmd == 'compare':
        sys.exit(0 if compare(a.a, a.b, until=a.until) else 1)
    else:
        result = summary(a.log)
        if a.json:
            print(json.dumps(result, indent=2))
            return
        for pid, p in result.items():
            last = p['last_stats']
            print(f"p{pid}: start {p['start']}")
            print(f"    events {p['events']}")
            print(f"    epochs {[(x['t'] // 1000, x.get('to')) for x in p['epoch_advances']]} (game s, epoch)")
            print(f"    first kept attack group {p['first_attack_ms'] and p['first_attack_ms'] // 1000} s; "
                  f"mean unspent {p['mean_unspent']}; mean idle citizens {p['mean_idle']}; "
                  f"pop-capped samples {p['pop_blocked_samples']}/{p['stats_samples']}")
            print(f"    last stats {last}")
            print(f"    AI cost {p['profile']}")


if __name__ == '__main__':
    main()
